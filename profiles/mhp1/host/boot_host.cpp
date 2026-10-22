#include "boot_host.hpp"
#include <fstream>
#include "headless_ge.hpp"

#include "hle/hle_common.hpp"
#include "kernel/kernel.hpp"
#include "psprecomp/common.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>

#include "install/user_data.hpp"
#include "settings/settings.hpp"

namespace mhp1 {
void register_audio_services(mhp2g::HleRegistrar &);
void register_platform_services(mhp2g::HleRegistrar &);
void register_stock_modules(mhp2g::HleRegistrar &);
namespace {
struct EmbeddedNid {
    const char *library;
    std::uint32_t nid;
    const char *name;
};
constexpr EmbeddedNid kNids[] = {
#include "nid_table.inc"
};

inline constexpr std::uint32_t kMainModuleId = 0x01000001u;

void register_boot_services(mhp2g::HleRegistrar &hle, std::uint32_t image_end) {
    using namespace mhp2g;
    hle.add("sceGe_user", "sceGeEdramGetAddr", [](psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
        kernel().finish(ctx, psprecomp::GuestMemory::kVramPhysicalBase);
    });
    hle.add("sceGe_user", "sceGeEdramGetSize", [](psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
        kernel().finish(ctx, psprecomp::GuestMemory::kVramSize);
    });
    const auto coherent_cache = [](psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
        // The interpreter fetches current guest memory. No native game/overlay
        // corpus is registered, so there is no translated instruction cache.
        kernel().finish(ctx, 0u);
    };
    for (const char *name :
        {"sceKernelDcacheWritebackAll", "sceKernelDcacheWritebackInvalidateAll", "sceKernelDcacheInvalidateRange",
            "sceKernelDcacheWritebackRange", "sceKernelIcacheInvalidateAll", "sceKernelIcacheInvalidateRange"})
        hle.add("UtilsForUser", name, coherent_cache);
    hle.add("UtilsForUser", "sceKernelLibcClock", [](psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
        kernel().finish(ctx, static_cast<std::uint32_t>(kernel().now_us()));
    });
    const auto epoch_us = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count());
    hle.add("UtilsForUser", "sceKernelLibcTime", [epoch_us](psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
        const auto seconds = static_cast<std::uint32_t>((epoch_us + kernel().now_us()) / 1'000'000u);
        if (arg(ctx, 0) != 0u) rt.memory().store32(arg(ctx, 0), seconds);
        kernel().finish(ctx, seconds);
    });
    hle.add("UtilsForUser", "sceKernelLibcGettimeofday",
        [epoch_us](psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
            const auto now = epoch_us + kernel().now_us();
            if (arg(ctx, 0) != 0u) {
                rt.memory().store32(arg(ctx, 0), static_cast<std::uint32_t>(now / 1'000'000u));
                rt.memory().store32(arg(ctx, 0) + 4u, static_cast<std::uint32_t>(now % 1'000'000u));
            }
            if (arg(ctx, 1) != 0u) {
                rt.memory().store32(arg(ctx, 1), 0u);
                rt.memory().store32(arg(ctx, 1) + 4u, 0u);
            }
            kernel().finish(ctx, 0u);
        });
    hle.add("InterruptManager", "sceKernelRegisterSubIntrHandler",
        [](psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
            auto &handler = kernel().sub_interrupts[arg(ctx, 0)][arg(ctx, 1)];
            handler.handler = arg(ctx, 2);
            handler.argument = arg(ctx, 3);
            kernel().finish(ctx, 0u);
        });
    hle.add("InterruptManager", "sceKernelEnableSubIntr", [](psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
        auto &handlers = kernel().sub_interrupts[arg(ctx, 0)];
        const auto found = handlers.find(arg(ctx, 1));
        if (found != handlers.end()) found->second.enabled = true;
        kernel().finish(ctx, found != handlers.end() ? 0u : error::kIllegalArgument);
    });
    hle.add("InterruptManager", "sceKernelReleaseSubIntrHandler",
        [](psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
            const bool removed = kernel().sub_interrupts[arg(ctx, 0)].erase(arg(ctx, 1)) != 0u;
            kernel().finish(ctx, removed ? 0u : error::kIllegalArgument);
        });
    hle.add("LoadExecForUser", "sceKernelExitGame",
        [](psprecomp::Runtime &rt, psprecomp::AllegrexContext &) { rt.stop("guest called sceKernelExitGame"); });
    hle.add("ModuleMgrForUser", "sceKernelGetModuleId",
        [](psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { kernel().finish(ctx, kMainModuleId); });
    hle.add("ModuleMgrForUser", "sceKernelGetModuleIdByAddress",
        [image_end](psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
            const auto address = rt.memory().canonical(arg(ctx, 0));
            // Only the main executable's verified PT_LOAD span has a module
            // identity in this bootstrap. Heap/kernel/other-module addresses
            // must not receive the main module's ID merely for being mapped.
            if (address < psprecomp::kDefaultPspUserLoadBase || address >= image_end) {
                rt.stop(
                    "Unsupported module address in sceKernelGetModuleIdByAddress: " + psprecomp::hex32(arg(ctx, 0)));
                return;
            }
            kernel().finish(ctx, kMainModuleId);
        });
    for (const auto &[name, descriptor] :
        {std::pair{"sceKernelStdin", 0u}, std::pair{"sceKernelStdout", 1u}, std::pair{"sceKernelStderr", 2u}})
        hle.add("StdioForUser", name,
            [descriptor](psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { kernel().finish(ctx, descriptor); });
}
} // namespace

BootHost *BootHost::active_ = nullptr;

BootHost::BootHost(psprecomp::Runtime &runtime, std::span<const psprecomp::PspImport> imports, std::uint32_t gp,
    std::uint32_t image_end, const std::filesystem::path &disc, const std::filesystem::path &memory_stick)
    : runtime_(runtime) {
    #if defined(MHP1_HAS_VULKAN)
        const auto data_dir =
            std::filesystem::absolute(memory_stick).parent_path() / "renderer-data";
        std::filesystem::create_directories(data_dir);
        mhp2g::install::set_data_directory_override(data_dir);

        // Preserve the current profile's unsupported-feature restrictions.
        auto &player = mhp2g::settings::current();
        player.selective_ui = false;
        player.right_stick = mhp2g::settings::RightStick::DPad;
        player.touch_controls = false;
        player.free_camera = false;
        player.analog_camera = false;
        player.lock_on = false;
        player.mouse = false;
    #endif
    if (active_ != nullptr) throw psprecomp::Error("Only one MHP1 diagnostic host may be active");
    if (runtime.function_count() != 0u)
        throw psprecomp::Error("The diagnostic host requires an empty registry; do not link a game AOT corpus");
    if (image_end < psprecomp::kDefaultPspUserLoadBase || image_end > 0x09F00000u)
        throw psprecomp::Error("Invalid image end or insufficient space for the diagnostic loader stack");
    if (std::getenv("PSPRECOMP_NO_INTERPRETER") != nullptr)
        throw psprecomp::Error("Unset PSPRECOMP_NO_INTERPRETER for this interpreter-based diagnostic");
    for (const auto &import : imports) {
        if ((import.stub_address & 3u) != 0u || import.stub_address < psprecomp::kDefaultPspUserLoadBase ||
            !runtime.memory().contains(import.stub_address, 8u) ||
            !imports_.emplace(import.stub_address, import).second)
            throw psprecomp::Error("Invalid or duplicate PSP import stub");
    }
    // Additional 1.x I/O exports, verified against PSPSDK IoFileMgrForUser.S.
    runtime.nids().add("IoFileMgrForUser", 0x3251EA56u, "sceIoPollAsync");
    runtime.nids().add("IoFileMgrForUser", 0xA0B5A7C2u, "sceIoReadAsync");
    runtime.nids().add("IoFileMgrForUser", 0x71B19E77u, "sceIoLseekAsync");
    for (const auto &nid : kNids) runtime.nids().add(nid.library, nid.nid, nid.name);
    mhp2g::kernel().install(runtime, gp, image_end);
    mhp2g::HleRegistrar hle(runtime);
    mhp2g::register_threadman(hle);
    mhp2g::register_sysmem(hle);
    mhp2g::register_io(hle, disc, memory_stick, true);
    register_boot_services(hle, image_end);
    register_audio_services(hle);
    mhp2g::register_atrac(hle);
    mhp2g::register_mpeg(hle);
    register_platform_services(hle);
    register_stock_modules(hle);
    //mhp2g::register_savedata(hle, memory_stick);
    mhp2g::register_utility(hle, memory_stick);
    mhp2g::register_font(hle);
    register_headless_ge(hle, memory_stick.parent_path() / "renderer-data");
    hle.add(
        "sceGe_user", "sceGeEdramSetAddrTranslation", [this](psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
            const auto width = mhp2g::arg(ctx, 0);
            const auto previous = edram_translation_width_;
            // PSPSDK: width zero leaves the setting unchanged; the return
            // value is the previously configured width, or zero before setup.
            if (width != 0u && width != 512u && width != 1024u && width != 2048u && width != 4096u) {
                rt.stop("Unsupported diagnostic EDRAM translation width=" + std::to_string(width));
                return;
            }
            if (width != 0u) {
                edram_translation_width_ = width;
                if (previous == 0u)
                    std::cout << "[mhp1-boot] edram_translation_width=" << width << " previous=" << previous
                              << " headless=1\n";
            }
            // Store GE configuration only. The headless decoder does not rasterize
            // or implement translated GPU accesses.
            mhp2g::kernel().finish(ctx, previous);
        });
    hle.add("sceDisplay", "sceDisplaySetMode", [this](psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
        const auto mode = mhp2g::arg(ctx, 0);
        const auto width = mhp2g::arg(ctx, 1);
        const auto height = mhp2g::arg(ctx, 2);
        // This bootstrap models the standard PSP LCD configuration. Other
        // configurations may be legal on hardware; report them as unsupported
        // here rather than fabricating a PSP error or silently accepting them.
        if (mode != 0u || width != 480u || height != 272u) {
            rt.stop("Unsupported diagnostic display mode=" + std::to_string(mode) + " width=" + std::to_string(width) +
                " height=" + std::to_string(height));
            return;
        }
        if (!display_mode_.configured)
            std::cout << "[mhp1-boot] display_mode=" << mode << " size=" << width << "x" << height << " headless=1\n";
        display_mode_ = {true, mode, width, height};
        mhp2g::kernel().finish(ctx, 0u);
    });
    hle.add("SysMemUserForUser", "sceKernelSetCompiledSdkVersion",
        [this](psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
            compiled_sdk_version_ = mhp2g::arg(ctx, 0);
            mhp2g::kernel().finish(ctx, 0u);
        });
    for (const auto &[pc, import] : imports_) {
        bound_[pc] = hle.bound(import.library, import.nid);
        if (bound_[pc]) ++bound_imports_;
        runtime.register_function(pc, &import_bridge, "mhp1_import_bridge");
    }
    active_ = this;
    psprecomp::set_runtime_missing_function_hook(&missing_function);
    psprecomp::set_runtime_pre_dispatch_hook(&pre_dispatch);
    std::cout << "[mhp1-boot] imports=" << imports_.size() << " implemented=" << bound_imports_
              << " unresolved=" << imports_.size() - bound_imports_ << "\n";
}

BootHost::~BootHost() {
    psprecomp::set_runtime_missing_function_hook(nullptr);
    psprecomp::set_runtime_pre_dispatch_hook(nullptr);
    psprecomp::set_runtime_starvation_hook(nullptr, 0u);
    psprecomp::set_runtime_thread_identity(-1, "");
    active_ = nullptr;
}

void BootHost::check_deadline() {
    if (running_ && std::chrono::steady_clock::now() >= deadline_)
        runtime_.stop("MHP1 diagnostic wall-time limit reached");
}

bool BootHost::missing_function(psprecomp::Runtime &, psprecomp::AllegrexContext &, std::uint32_t) {
    active_->check_deadline();
    return false; // The runtime interprets current memory, including overlay code.
}

void BootHost::pre_dispatch(psprecomp::Runtime &, psprecomp::AllegrexContext &, std::uint32_t, std::int32_t) {
    active_->check_deadline();
}

void BootHost::import_bridge(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    active_->invoke(ctx);
}

void BootHost::invoke(psprecomp::AllegrexContext &ctx) {
    check_deadline();
    if (runtime_.stopped()) return;
    const auto found = imports_.find(runtime_.memory().canonical(ctx.pc));
    if (found == imports_.end()) throw psprecomp::Error("Import bridge reached at an unknown address");
    const auto &import = found->second;
    ++import_calls_;
    if (++calls_[found->first] == 1u) {
        std::cout << "[mhp1-boot] first_call=" << import.library
                  << "::" << runtime_.nids().resolve(import.library, import.nid).value_or(psprecomp::hex32(import.nid))
                  << " stub=" << psprecomp::hex32(ctx.pc) << " ra=" << psprecomp::hex32(ctx.gpr[31])
                  << " a0=" << psprecomp::hex32(ctx.gpr[4]) << " a1=" << psprecomp::hex32(ctx.gpr[5])
                  << " a2=" << psprecomp::hex32(ctx.gpr[6]) << " a3=" << psprecomp::hex32(ctx.gpr[7]) << "\n";
    }
    const auto token = psprecomp::capture_runtime_execution_context();
    const std::uint32_t pc = ctx.pc;
    const std::uint32_t return_address = ctx.gpr[31];
    // Unbound calls stop in Runtime::invoke_import, without a success stub.
    runtime_.invoke_import(import.library, import.nid, ctx);
    // A blocking HLE may have switched threads or entered an interrupt. Only
    // return to this caller if it still owns the context, as in generated wrappers.
    if (!runtime_.stopped() && psprecomp::runtime_execution_context_matches(token) && ctx.pc == pc)
        ctx.pc = return_address;
}

void BootHost::write_import_audit(const std::filesystem::path &path) const {
    std::ofstream file(path);
    file << "library,nid,name,stub_address,bound\n";
    for (const auto &[pc, import] : imports_)
        file << import.library << "," << psprecomp::hex32(import.nid) << ","
             << runtime_.nids().resolve(import.library, import.nid).value_or("unknown") << "," << psprecomp::hex32(pc)
             << "," << (bound_.at(pc) ? 1 : 0) << "\n";
    if (!file) throw psprecomp::Error("Cannot write MHP1 import audit");
}

void BootHost::run(std::uint32_t entry, unsigned seconds, std::uint64_t max_dispatches) {
    if (ran_) throw psprecomp::Error("A diagnostic host can run only once");
    if (max_dispatches == 0u || max_dispatches > 100'000'000u)
        throw psprecomp::Error("Diagnostic bounds must be 1..60 seconds and 1..100000000 dispatches");
    if ((entry & 3u) != 0u || !runtime_.memory().contains(entry, 4u))
        throw psprecomp::Error("Invalid diagnostic entry address");
    constexpr std::string_view boot_path = "disc0:/PSP_GAME/SYSDIR/EBOOT.BIN";
    mhp2g::write_cstring(runtime_.memory(), mhp2g::kBootArgumentAddress, boot_path, boot_path.size() + 1u);
    runtime_.cpu().set_gpr(4u, static_cast<std::uint32_t>(boot_path.size() + 1u));
    runtime_.cpu().set_gpr(5u, mhp2g::kBootArgumentAddress);
    mhp2g::kernel().start_loader_thread(runtime_.cpu(), entry, 0u);
    ran_ = running_ = true;
    deadline_ = seconds == 0u
        ? std::chrono::steady_clock::time_point::max()
        : std::chrono::steady_clock::now() + std::chrono::seconds(seconds);

    if (seconds == 0u)
        max_dispatches = std::numeric_limits<std::uint64_t>::max();
    std::cout << "[mhp1-boot] execution=interpreter seconds=" << seconds << " max_dispatches=" << max_dispatches
              << "\n";
    runtime_.run(entry, max_dispatches);
    running_ = false;
    std::cout << "[mhp1-boot] stopped=" << runtime_.stop_reason() << " pc=" << psprecomp::hex32(runtime_.cpu().pc)
              << " import_calls=" << import_calls_ << " sdk=" << psprecomp::hex32(compiled_sdk_version_) << "\n"
              << mhp2g::kernel().describe_threads() << "\n";
}
} // namespace mhp1
