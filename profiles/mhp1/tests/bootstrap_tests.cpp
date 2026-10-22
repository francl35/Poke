#include "boot_host.hpp"
#include <bit>
#include <cstdlib>

#include "hle/hle_common.hpp"
#include "kernel/kernel.hpp"
#include "psprecomp/common.hpp"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <stdexcept>
#include <string_view>

namespace {
void check(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}
template <typename Function> void rejects(Function function) {
    bool rejected = false;
    try {
        function();
    } catch (const psprecomp::Error &) {
        rejected = true;
    }
    check(rejected, "Expected an invalid bootstrap request to be rejected");
}
struct Scratch {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("mhp1-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Scratch() { std::filesystem::create_directories(path); }
    ~Scratch() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
};
constexpr std::uint32_t kCode = 0x08810000u;
constexpr std::uint32_t kImport = 0x08820000u;
constexpr std::uint32_t kData = 0x08830000u;
constexpr std::uint32_t kDelayNid = 0xCEADEB47u;
std::uint32_t jump(std::uint32_t address, bool link = false) {
    return (link ? 0x0C000000u : 0x08000000u) | ((address >> 2u) & 0x03FFFFFFu);
}

void test_ge(std::string_view mode) {
    Scratch scratch;
    psprecomp::Runtime rt;
    mhp1::BootHost host(rt, {}, 0u, 0x08900000u, {}, scratch.path);
    auto &ctx = rt.cpu();
    auto call = [&](std::uint32_t nid, std::uint32_t a = 0u, std::uint32_t b = 0u, std::uint32_t c = 0u,
                    std::uint32_t d = 0u) {
        ctx.gpr[4] = a;
        ctx.gpr[5] = b;
        ctx.gpr[6] = c;
        ctx.gpr[7] = d;
        rt.invoke_import("sceGe_user", nid, ctx);
        return ctx.gpr[2];
    };
    rt.memory().store32(kData, 0x0F000007u);      // FINISH 7
    rt.memory().store32(kData + 4u, 0x0C000000u); // END
    if (mode == "ge-init" || mode == "ge-render") {
        // Synthetic reset pattern, using documented state registers rather than game code.
        const std::array<std::uint32_t, 68> commands = {0x15, 0x16, 0x1C, 0x20, 0x24, 0x25, 0x26, 0x27, 0x28, 0x2C,
            0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x36, 0x37, 0x38, 0x50, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
            0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xC1, 0xC8, 0xCA,
            0xCC, 0xD0, 0xD8, 0xD9, 0xDA, 0xDC, 0xDD, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE8, 0xE9, 0xF0, 0xF1, 0xF2, 0xF3,
            0xF4, 0xF5, 0xF6, 0xF7};
        unsigned offset = 0;
        for (const auto cmd : commands) {
            rt.memory().store32(kData + offset, cmd << 24);
            offset += 4;
        }
        rt.memory().store32(kData + offset, 0x0C000000u);
        const auto initialized = call(0xAB49E76Au, kData, 0, 0xFFFFFFFFu);
        check(!rt.stopped(), "Register initialization rejected");
        check(call(0x03444EB4u, initialized, 1) == 0, "Initialization did not complete");
        if (mode == "ge-init") return;
        const auto vertices = kData + 0x400u;
        for (unsigned i = 0; i < 2; ++i) {
            rt.memory().store32(vertices + i * 16u, 0xFF0040FFu);
            rt.memory().store32(vertices + i * 16u + 4, std::bit_cast<std::uint32_t>(i ? 64.0f : 8.0f));
            rt.memory().store32(vertices + i * 16u + 8, std::bit_cast<std::uint32_t>(i ? 48.0f : 8.0f));
            rt.memory().store32(vertices + i * 16u + 12, std::bit_cast<std::uint32_t>(0.0f));
        }
        const std::array<std::uint32_t, 11> list = {0x9C000000u, 0x9D040200u, 0xD2000003u, 0x16043DDFu, 0xD5043DDFu,
            0xE7000001u, 0x1280019Cu, 0x10080000u, 0x01830400u, 0x04060002u, 0x0C000000u};
        for (unsigned i = 0; i < list.size(); ++i) rt.memory().store32(kData + i * 4u, list[i]);
        call(0xAB49E76Au, kData, 0, 0xFFFFFFFFu);
        check(!rt.stopped(), "Synthetic GE render list failed");
        check(rt.memory().load32(0x04000000u + (10 * 512 + 10) * 4) == 0xFF0040FFu, "GE did not rasterize the sprite");
        check(rt.memory().load32(0x04000000u) == 0u, "GE sprite escaped its bounds");
        ctx.gpr[4] = 0x04000000u;
        ctx.gpr[5] = 512;
        ctx.gpr[6] = 3;
        ctx.gpr[7] = 0;
        rt.invoke_import("sceDisplay", 0x289D82FEu, ctx);
        check(!rt.stopped() && ctx.gpr[2] == 0, "Framebuffer submission failed");
        const auto capture_dir = std::filesystem::path(std::getenv("MHP1_RENDER_DIR"));
        ctx.gpr[4] = 0x04000000u;
        ctx.gpr[5] = 512;
        ctx.gpr[6] = 3;
        ctx.gpr[7] = 1;
        rt.invoke_import("sceDisplay", 0x289D82FEu, ctx);
        check(!std::filesystem::exists(capture_dir / "vblank_2.bmp"), "Deferred framebuffer captured before vblank");
        mhp2g::kernel().on_vblank();
        check(std::filesystem::exists(capture_dir / "vblank_2.bmp"), "Deferred framebuffer was not captured");
        for (unsigned invalid = 0; invalid < 5; ++invalid) {
            ctx.gpr[4] = invalid == 0 ? 0x07000000u : 0x04000000u;
            ctx.gpr[5] = invalid == 1 ? 500u : 512u;
            ctx.gpr[6] = invalid == 2 ? 4u : 3u;
            ctx.gpr[7] = invalid == 3 ? 2u : 0u;
            if (invalid == 4) ctx.gpr[4] += 1u;
            rt.invoke_import("sceDisplay", 0x289D82FEu, ctx);
            check(ctx.gpr[2] >= 0x80000000u, "Invalid framebuffer accepted");
        }
        ctx.gpr[4] = 0;
        ctx.gpr[5] = 0;
        ctx.gpr[6] = 0;
        ctx.gpr[7] = 0;
        rt.invoke_import("sceDisplay", 0x289D82FEu, ctx);
        mhp2g::kernel().on_vblank();
        check(!std::filesystem::exists(capture_dir / "vblank_3.bmp"), "Disabled display was captured");
        ctx.gpr[4] = 0x04000000u;
        ctx.gpr[5] = 512;
        ctx.gpr[6] = 3;
        ctx.gpr[7] = 0;
        rt.invoke_import("sceDisplay", 0x289D82FEu, ctx);
        for (unsigned i = 0; i < 10; ++i) mhp2g::kernel().on_vblank();
        check(!std::filesystem::exists(capture_dir / "vblank_8.bmp"), "Capture limit exceeded");

        return;
    }
    if (mode == "ge-options") {
        call(0xAB49E76Au, kData, 0u, 0xFFFFFFFFu, kData);
        check(rt.stop_reason().find("options/context") != std::string::npos, "Options silently ignored");
        return;
    }
    if (mode == "ge-invalid-list") {
        rt.memory().store32(kData, 0x10000000u);      // BASE 0
        rt.memory().store32(kData + 4u, 0x08FFFFFFu); // JUMP to unmapped memory
        call(0xAB49E76Au, kData, 0u, 0xFFFFFFFFu);
        check(rt.stop_reason().find("did not reach END") != std::string::npos, "Malformed list accepted");
        return;
    }
    if (mode == "ge-callback") {
        const auto cb = kData + 0x100u;
        rt.memory().store32(cb, kCode);
        rt.memory().store32(cb + 4u, 0x1234u);
        rt.memory().store32(cb + 8u, kCode + 0x20u);
        rt.memory().store32(cb + 12u, 0x5678u);
        const auto id = call(0xA4FC06A4u, cb);
        rt.memory().store32(kData, 0x0E000003u);      // SIGNAL 3
        rt.memory().store32(kData + 4u, 0x0F000007u); // FINISH 7
        rt.memory().store32(kData + 8u, 0x0C000000u);
        mhp2g::kernel().start_loader_thread(ctx, kCode + 0x40u, 0u);
        ctx.gpr[31] = kCode + 0x44u;
        call(0xAB49E76Au, kData, 0u, id);
        check(mhp2g::kernel().in_interrupt() && ctx.pc == kCode && ctx.gpr[4] == 3u && ctx.gpr[5] == 0x1234u,
            "Signal callback was not delivered with its arguments");
        mhp2g::kernel().interrupt_return_stub(ctx);
        check(ctx.pc == kCode + 0x20u && ctx.gpr[4] == 7u && ctx.gpr[5] == 0x5678u,
            "Finish callback was not delivered in order");
        mhp2g::kernel().interrupt_return_stub(ctx);
        check(!mhp2g::kernel().in_interrupt() && ctx.pc == kCode + 0x44u, "GE callbacks did not restore the caller");
        return;
    }
    const auto list = call(0xAB49E76Au, kData, kData, 0xFFFFFFFFu);
    check(list > 0u && list < 0x80000000u, "List was not queued");
    check(call(0x03444EB4u, list, 1u) == 3u, "Stalled list was reported complete");
    if (mode == "ge-blocking" || mode == "ge-draw-blocking") {
        auto &kernel = mhp2g::kernel();
        kernel.start_loader_thread(ctx, kCode, 0);
        const auto main = kernel.current_uid();
        const auto worker = kernel.create_thread("GE producer", kCode + 0x100u, 0x40u, 0x4000u, 0, 0);
        check(worker > 0 && kernel.start_thread(ctx, worker, 0, 0) == 0, "Cannot start GE producer");
        ctx.gpr[31] = kCode + 0x10u;
        if (mode == "ge-blocking")
            call(0x03444EB4u, list, 0);
        else
            call(0xB287BD61u, 0);
        check(!rt.stopped() && kernel.current_uid() == worker &&
                kernel.find_thread(main)->status == mhp2g::ThreadStatus::Waiting,
            "GE sync did not yield to the producer");
        ctx.gpr[31] = kCode + 0x104u;
        call(0xE0D68148u, list, 0);
        kernel.delay_current(ctx, 1000);
        check(!rt.stopped() && kernel.current_uid() == main && ctx.pc == kCode + 0x10u && ctx.gpr[2] == 0,
            "GE completion did not resume its waiting caller");
        return;
    }
    if (mode == "ge-head") {
        call(0x1C0D95A6u, kData, 0u, 0xFFFFFFFFu);
        check(rt.stop_reason().find("head insertion") != std::string::npos, "Active head insertion accepted");
        return;
    }
    const auto second = call(0xAB49E76Au, kData, 0u, 0xFFFFFFFFu);
    check(call(0x03444EB4u, second, 1u) == 1u, "Queued list overtook stalled list");
    check(call(0xB287BD61u, 1u) == 3u, "Draw peek lost stall state");
    check(call(0xE0D68148u, list, 0u) == 0u, "Stall update failed");
    check(call(0x03444EB4u, list, 0u) == 0u, "Resumed list did not complete");
    check(call(0x03444EB4u, second, 1u) == 0u, "Queue did not drain");
    check(call(0xB287BD61u, 0u) == 0u, "Completed draw wait failed");
    check(call(0x03444EB4u, 999u, 1u) >= 0x80000000u, "Unknown list accepted");
    check(call(0x03444EB4u, list, 2u) >= 0x80000000u, "Invalid sync mode accepted");
    check(call(0xAB49E76Au, kData + 1u, 0u, 0xFFFFFFFFu) >= 0x80000000u, "Unaligned list accepted");
    check(call(0xA4FC06A4u, 0u) >= 0x80000000u, "Null callback accepted");
    const auto cb = kData + 0x100u;
    for (unsigned i = 0; i < 4; ++i) rt.memory().store32(cb + i * 4u, 0u);
    const auto callback = call(0xA4FC06A4u, cb);
    check(callback > 0u && callback < 0x80000000u, "Callback registration failed");
    check(call(0xAB49E76Au, kData, 0u, callback) < 0x80000000u, "Callback list failed");
    check(call(0x05DB22CEu, callback) == 0u, "Callback removal failed");
    check(call(0x05DB22CEu, callback) >= 0x80000000u, "Unknown callback removal accepted");
    check(call(0xAB49E76Au, kData, 0u, callback) >= 0x80000000u, "Deleted callback accepted");
    // Transfer two 16-bit pixels from RAM to VRAM using actual GE commands.
    const std::array<std::uint32_t, 8> transfer = {
        0xB2830200u, 0xB3080002u, 0xB4000000u, 0xB5040002u, 0xEB000000u, 0xEC000000u, 0xEE000001u, 0xEA000000u};
    for (unsigned i = 0; i < transfer.size(); ++i) rt.memory().store32(kData + i * 4u, transfer[i]);
    rt.memory().store32(kData + 32u, 0x0C000000u);
    rt.memory().store32(kData + 0x200u, 0x12345678u);
    call(0xAB49E76Au, kData, 0u, 0xFFFFFFFFu);
    check(rt.memory().load32(0x04000000u) == 0x12345678u, "GE transfer did not update VRAM");
}

void test_services() {
    Scratch scratch;
    std::ofstream(scratch.path / "fixture.bin", std::ios::binary) << "abc";
    psprecomp::Runtime runtime;
    const std::array<psprecomp::PspImport, 5> imports = {{{"ThreadManForUser", kDelayNid, kImport},
        {"MHP1Test", 0xDEADBEEFu, kImport + 8u}, {"IoFileMgrForUser", 0x109F50BCu, kImport + 16u},
        {"IoFileMgrForUser", 0x6A638D83u, kImport + 24u}, {"IoFileMgrForUser", 0x810C4BC3u, kImport + 32u}}};
    auto duplicates = imports;
    duplicates[1].stub_address = duplicates[0].stub_address;
    rejects([&] { mhp1::BootHost bad(runtime, duplicates, 0u, 0x08900000u, {}, scratch.path); });
    rejects([&] { mhp1::BootHost bad(runtime, {}, 0u, 0x0A000000u, {}, scratch.path); });
    mhp1::BootHost host(runtime, imports, 0u, 0x08900000u, {}, scratch.path);
    check(host.bound_imports() == 4u, "Unexpected import coverage");
    host.write_import_audit(scratch.path / "imports.csv");
    std::ifstream audit(scratch.path / "imports.csv");
    const std::string audited{std::istreambuf_iterator<char>(audit), {}};
    check(audited.find("MHP1Test,0xDEADBEEF,unknown,0x08820008,0") != std::string::npos,
        "Missing unresolved import in audit");
    check(audited.find("sceKernelDelayThread") != std::string::npos, "Missing bound import in audit");
    rejects([&] { mhp1::BootHost bad(runtime, {}, 0u, 0x08900000u, {}, scratch.path); });
    auto &ctx = runtime.cpu();
    // Main-module lookups include the observed entry and the load-span bounds.
    runtime.invoke_import("ModuleMgrForUser", 0xF0A26395u, ctx);
    const auto module_id = ctx.gpr[2];
    check(module_id != 0u && module_id < 0x80000000u, "Invalid main-module ID");
    for (const auto address : {0x08899C2Cu, 0x08804000u, 0x088FFFFFu}) {
        ctx.set_gpr(4, address);
        runtime.invoke_import("ModuleMgrForUser", 0xD8B73127u, ctx);
        check(!runtime.stopped() && ctx.gpr[2] == module_id, "Main-module address lookup failed");
    }
    // Graphics-memory discovery must identify actual VRAM, not main RAM or
    // the full mirrored address span. Neither query consumes its argument.
    runtime.memory().store32(0x08A5ED00u, 0x13579BDFu);
    ctx.set_gpr(4, 0x08A5ED00u); // Argument observed in the real startup capture.
    runtime.invoke_import("sceGe_user", 0xE47E40E4u, ctx);
    const auto vram_base = ctx.gpr[2];
    check(!runtime.stopped() && vram_base == 0x04000000u, "EDRAM base query failed");
    runtime.invoke_import("sceGe_user", 0x1F6752ADu, ctx);
    const auto vram_size = ctx.gpr[2];
    check(!runtime.stopped() && vram_size == 0x00200000u, "EDRAM size query failed");
    check(runtime.memory().contains(vram_base, vram_size), "Reported EDRAM range is not backed by guest memory");
    runtime.memory().store32(vram_base, 0x01234567u);
    runtime.memory().store32(vram_base + vram_size - 4u, 0x89ABCDEFu);
    check(runtime.memory().load32(vram_base) == 0x01234567u &&
            runtime.memory().load32(vram_base + vram_size - 4u) == 0x89ABCDEFu,
        "EDRAM query bounds do not identify accessible storage");
    check(runtime.memory().load32(0x08A5ED00u) == 0x13579BDFu,
        "An EDRAM query unexpectedly wrote through the unused argument");
    check(host.edram_translation_width() == 0u, "Unexpected initial translation state");
    ctx.set_gpr(4, 0u);
    runtime.invoke_import("sceGe_user", 0xB77905EAu, ctx);
    check(!runtime.stopped() && ctx.gpr[2] == 0u && host.edram_translation_width() == 0u,
        "Initial translation query failed");
    std::uint32_t previous_width = 0u;
    for (const auto width : {1024u, 1024u, 512u, 2048u, 4096u, 1024u}) {
        ctx.set_gpr(4, width);
        runtime.invoke_import("sceGe_user", 0xB77905EAu, ctx);
        check(!runtime.stopped() && ctx.gpr[2] == previous_width && host.edram_translation_width() == width,
            "Translation setter did not preserve or return the previous width");
        previous_width = width;
    }
    ctx.set_gpr(4, 0u);
    runtime.invoke_import("sceGe_user", 0xB77905EAu, ctx);
    check(!runtime.stopped() && ctx.gpr[2] == 1024u && host.edram_translation_width() == 1024u,
        "Zero-width query reset the translation setting");
    check(runtime.memory().load32(vram_base) == 0x01234567u &&
            runtime.memory().load32(vram_base + vram_size - 4u) == 0x89ABCDEFu,
        "Translation setup unexpectedly changed physical VRAM contents");
    check(!host.display_mode().configured, "Display was configured before SetMode");
    ctx.set_gpr(4, 0u);
    ctx.set_gpr(5, 480u);
    ctx.set_gpr(6, 272u);
    ctx.set_gpr(2, 0xCAFEBABEu);
    runtime.invoke_import("sceDisplay", 0x0E20F177u, ctx);
    check(!runtime.stopped() && ctx.gpr[2] == 0u && host.display_mode().configured && host.display_mode().mode == 0u &&
            host.display_mode().width == 480u && host.display_mode().height == 272u,
        "Standard LCD mode was not stored");
    runtime.invoke_import("sceDisplay", 0x0E20F177u, ctx);
    check(!runtime.stopped() && ctx.gpr[2] == 0u && host.display_mode().height == 272u,
        "Repeated LCD mode request failed");
    // Verify metadata and clocks without inventing a success stub for missing HLE.
    ctx.set_gpr(4, 0x02000010u);
    runtime.invoke_import("SysMemUserForUser", 0x7591C7DBu, ctx);
    check(host.compiled_sdk_version() == 0x02000010u && ctx.gpr[2] == 0u, "SDK version was not stored");
    ctx.set_gpr(4, kData + 256u);
    runtime.invoke_import("UtilsForUser", 0x27CC57F0u, ctx);
    check(ctx.gpr[2] > 0u && runtime.memory().load32(kData + 256u) == ctx.gpr[2], "LibcTime failed");
    ctx.set_gpr(4, kData + 256u);
    ctx.set_gpr(5, 0u);
    runtime.invoke_import("UtilsForUser", 0x71EC4271u, ctx);
    check(ctx.gpr[2] == 0u && runtime.memory().load32(kData + 260u) < 1'000'000u, "Gettimeofday failed");
    ctx.set_gpr(4, 30u);
    ctx.set_gpr(5, 0u);
    ctx.set_gpr(6, kCode);
    ctx.set_gpr(7, 123u);
    runtime.invoke_import("InterruptManager", 0xCA04A2B9u, ctx);
    runtime.invoke_import("InterruptManager", 0xFB8E22ECu, ctx);
    check(ctx.gpr[2] == 0u && mhp2g::kernel().sub_interrupts[30u][0u].enabled, "Interrupt registration failed");
    runtime.invoke_import("InterruptManager", 0xD61E6961u, ctx);
    check(ctx.gpr[2] == 0u && mhp2g::kernel().sub_interrupts[30u].empty(), "Interrupt release failed");
    // Exercise actual host allocation/free and memory-stick I/O implementations.
    mhp2g::write_cstring(runtime.memory(), kData, "test", 32u);
    ctx.set_gpr(4, 2u);
    ctx.set_gpr(5, kData);
    ctx.set_gpr(6, 0u);
    ctx.set_gpr(7, 1024u);
    runtime.invoke_import("SysMemUserForUser", 0x237DBD4Fu, ctx);
    const auto block = static_cast<mhp2g::SceUID>(ctx.gpr[2]);
    check(block > 0 && mhp2g::kernel().find_block(block) != nullptr, "Memory allocation failed");
    ctx.set_gpr(4, static_cast<std::uint32_t>(block));
    runtime.invoke_import("SysMemUserForUser", 0xB6D61D02u, ctx);
    check(ctx.gpr[2] == 0u && mhp2g::kernel().find_block(block) == nullptr, "Memory free failed");
    mhp2g::write_cstring(runtime.memory(), kData, "ms0:/fixture.bin", 64u);
    ctx.set_gpr(4, kData);
    ctx.set_gpr(5, 1u);
    ctx.set_gpr(6, 0u);
    runtime.invoke_import("IoFileMgrForUser", 0x109F50BCu, ctx);
    const auto fd = ctx.gpr[2];
    check(fd >= 3u && fd < 0x80000000u, "Memory-stick open failed");
    ctx.set_gpr(4, fd);
    ctx.set_gpr(5, kData + 128u);
    ctx.set_gpr(6, 3u);
    runtime.invoke_import("IoFileMgrForUser", 0x6A638D83u, ctx);
    check(
        ctx.gpr[2] == 3u && runtime.memory().load8(kData + 128u) == 'a' && runtime.memory().load8(kData + 130u) == 'c',
        "Memory-stick read failed");
    ctx.set_gpr(4, fd);
    runtime.invoke_import("IoFileMgrForUser", 0x810C4BC3u, ctx);
    check(ctx.gpr[2] == 0u, "Memory-stick close failed");
    // DelayThread must switch to idle and resume the caller, then this program
    // patches an instruction that the interpreter must execute as 42, not 7.
    const std::array<std::uint32_t, 14> code = {0x240403E8u, jump(kImport, true), 0u, 0x3C080881u, 0x3C092402u,
        0x3529002Au, 0xAD090028u, jump(kCode + 40u), 0u, 0u, 0x24020007u, 0x3C080883u, 0xAD020000u, jump(kImport + 8u)};
    for (std::size_t i = 0; i < code.size(); ++i)
        runtime.memory().store32(kCode + static_cast<std::uint32_t>(4u * i), code[i]);
    rejects([&] { host.run(kCode, 0u); });
    rejects([&] { host.run(kCode, 1u, 0u); });
    host.run(kCode, 5u, 1000u);
    check(runtime.memory().load32(kData) == 42u, "Modified instruction was not executed");
    check(mhp2g::kernel().now_us() >= 1000u, "DelayThread did not advance guest time");
    check(runtime.stop_reason().find("Missing HLE import MHP1Test::") != std::string::npos,
        "Unknown import did not stop with its identity");
    check(host.import_calls() == 2u, "Unexpected bridge call count");
    rejects([&] { host.run(kCode, 1u); });
}

void test_module_outside(std::uint32_t address) {
    Scratch scratch;
    psprecomp::Runtime runtime;
    mhp1::BootHost host(runtime, {}, 0u, 0x08900000u, {}, scratch.path);
    runtime.cpu().set_gpr(4, address);
    runtime.cpu().set_gpr(2, 0xCAFEBABEu);
    runtime.invoke_import("ModuleMgrForUser", 0xD8B73127u, runtime.cpu());
    check(runtime.stopped() && runtime.stop_reason().find("Unsupported module address") != std::string::npos,
        "An address outside the supported main module did not stop explicitly");
    check(runtime.cpu().gpr[2] == 0xCAFEBABEu, "Unsupported lookup returned a fabricated module ID");
}

void test_display_unsupported(std::string_view mode) {
    Scratch scratch;
    psprecomp::Runtime runtime;
    mhp1::BootHost host(runtime, {}, 0u, 0x08900000u, {}, scratch.path);
    auto &ctx = runtime.cpu();
    // An unsupported request must not overwrite an already configured mode.
    ctx.set_gpr(4, 0u);
    ctx.set_gpr(5, 480u);
    ctx.set_gpr(6, 272u);
    runtime.invoke_import("sceDisplay", 0x0E20F177u, ctx);
    ctx.set_gpr(2, 0xCAFEBABEu);
    if (mode == "display-mode")
        ctx.set_gpr(4, 0x1Au);
    else if (mode == "display-width")
        ctx.set_gpr(5, 0u);
    else
        ctx.set_gpr(6, 0u);
    runtime.invoke_import("sceDisplay", 0x0E20F177u, ctx);
    check(runtime.stopped() && runtime.stop_reason().find("Unsupported diagnostic display mode=") != std::string::npos,
        "Unsupported display request was silently accepted");
    check(ctx.gpr[2] == 0xCAFEBABEu && host.display_mode().mode == 0u && host.display_mode().width == 480u &&
            host.display_mode().height == 272u,
        "Unsupported display request changed the result or display state");
}

void test_translation_unsupported(std::uint32_t width) {
    Scratch scratch;
    psprecomp::Runtime runtime;
    mhp1::BootHost host(runtime, {}, 0u, 0x08900000u, {}, scratch.path);
    auto &ctx = runtime.cpu();
    ctx.set_gpr(4, 1024u);
    runtime.invoke_import("sceGe_user", 0xB77905EAu, ctx);
    ctx.set_gpr(4, width);
    ctx.set_gpr(2, 0xCAFEBABEu);
    runtime.invoke_import("sceGe_user", 0xB77905EAu, ctx);
    check(runtime.stopped() &&
            runtime.stop_reason().find("Unsupported diagnostic EDRAM translation width=") != std::string::npos,
        "Unsupported translation width was silently accepted");
    check(host.edram_translation_width() == 1024u && ctx.gpr[2] == 0xCAFEBABEu,
        "Unsupported translation width changed state or returned a fabricated value");
}

void test_import_trace() {
    Scratch scratch;
    psprecomp::Runtime runtime;
    std::vector<psprecomp::PspImport> imports;
    std::vector<std::uint32_t> code = {0x24040001u, 0x24050002u, 0x24060003u, 0x24070004u};
    // Reach an unsupported import after more than 64 distinct imported stubs.
    // Its first-call arguments must still be present in the diagnostic log.
    for (std::uint32_t i = 0; i < 65u; ++i) {
        const auto stub = kImport + 8u * i;
        imports.push_back({"sceGe_user", 0xE47E40E4u, stub});
        code.push_back(jump(stub, true));
        code.push_back(0u);
    }
    const auto unknown_stub = kImport + 8u * 65u;
    imports.push_back({"MHP1TraceTest", 0xDEADBEEFu, unknown_stub});
    code.push_back(jump(unknown_stub, true));
    code.push_back(0u);
    for (std::size_t i = 0; i < code.size(); ++i)
        runtime.memory().store32(kCode + static_cast<std::uint32_t>(4u * i), code[i]);
    mhp1::BootHost host(runtime, imports, 0u, 0x08900000u, {}, scratch.path);
    std::ostringstream captured;
    struct Capture {
        std::streambuf *old;
        explicit Capture(std::ostream &stream) : old(std::cout.rdbuf(stream.rdbuf())) {}
        ~Capture() { std::cout.rdbuf(old); }
    } capture(captured);
    host.run(kCode, 5u, 1000u);
    check(host.import_calls() == 66u, "Trace fixture did not reach all imports");
    check(captured.str().find("first_call=MHP1TraceTest::0xDEADBEEF") != std::string::npos,
        "First-call trace was exhausted before the unknown import");
    check(captured.str().find(" a2=0x00000003 a3=0x00000004") != std::string::npos,
        "Trace omitted the third and fourth argument registers");
}

void test_bound(bool wall_time) {
    Scratch scratch;
    psprecomp::Runtime runtime;
    mhp1::BootHost host(runtime, {}, 0u, 0x08900000u, {}, scratch.path);
    runtime.memory().store32(kCode, jump(kCode));
    runtime.memory().store32(kCode + 4u, 0u);
    const auto start = std::chrono::steady_clock::now();
    host.run(kCode, wall_time ? 1u : 5u, wall_time ? 100'000'000u : 5u);
    if (wall_time) {
        check(runtime.stop_reason().find("wall-time limit") != std::string::npos, "Wall-time bound failed");
        check(std::chrono::steady_clock::now() - start < std::chrono::seconds(5), "Deadline overran");
    } else {
        check(runtime.stop_reason().find("Dispatch limit") != std::string::npos, "Dispatch bound failed");
    }
}
} // namespace

int main(int argc, char **argv) {
    try {
        const std::string_view mode = argc == 2 ? argv[1] : "services";
        if (mode.starts_with("ge"))
            test_ge(mode);
        else if (mode == "services")
            test_services();
        else if (mode == "deadline")
            test_bound(true);
        else if (mode == "dispatch")
            test_bound(false);
        else if (mode == "module-low")
            test_module_outside(0x08803FFFu);
        else if (mode == "module-end")
            test_module_outside(0x08900000u);
        else if (mode == "module-unmapped")
            test_module_outside(0x0A000000u);
        else if (mode == "translation-invalid")
            test_translation_unsupported(513u);
        else if (mode == "translation-negative")
            test_translation_unsupported(0xFFFFFFFFu);
        else if (mode == "trace")
            test_import_trace();
        else if (mode == "display-mode" || mode == "display-width" || mode == "display-height")
            test_display_unsupported(mode);
        else
            throw std::runtime_error("Unknown test mode");
        std::cout << "MHP1 bootstrap tests passed: " << mode << "\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
