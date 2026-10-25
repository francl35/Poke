// Main-code AOT opt-in and conservative cache-flush invalidation for FreedomDX.
#include "main_native.hpp"

#include "psprecomp/common.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <vector>
#include <utility>

namespace mhp1 {
namespace {
constexpr std::uint32_t kUnitBase = 0x08804000u;
constexpr std::uint32_t kUnitSpan = 0x4000u;
constexpr std::uint32_t kFirstOverlay = 0x098D4700u;

struct Region {
    std::uint32_t address{};
    std::vector<std::uint8_t> bytes;
};
struct Unit {
    std::vector<Region> regions;
    bool retired{};
};
psprecomp::Runtime *owner = nullptr;
psprecomp::Runtime::RecompiledFunction import_bridge_function = nullptr;
std::map<std::uint32_t, Unit> units;
std::vector<std::uint32_t> import_pcs;

// Registering a non-chainable entry poisons the generated unit's fast paths.
// The entry is removed immediately afterward, before guest execution resumes.
void poison_entry(psprecomp::Runtime &runtime, psprecomp::AllegrexContext &) {
    runtime.stop("MHP1 native invalidation sentinel unexpectedly executed");
}

bool changed(const psprecomp::GuestMemory &memory, const Region &region) {
    // MHP1_FDX_DYNAMIC_LHU_GUARD
    const auto size = static_cast<std::uint32_t>(region.bytes.size());
    if (const auto *current = memory.raw_pointer(region.address, size)) {
        if (std::memcmp(current, region.bytes.data(), size) == 0) return false;
    }
    for (std::uint32_t i = 0; i < size; ++i) {
        if (memory.load8(region.address + i) == region.bytes[i]) continue;
#if defined(MHP1_FDX_DYNAMIC_LHU)
        // Accept only a whole known replacement of its original instruction.
        // CMake enables this only while both adapted generated sites exist.
        const std::uint32_t pc = (region.address + i) & ~3u;
        if ((pc == 0x0881D558u || pc == 0x0881D560u) &&
            pc >= region.address && size >= 4u && pc - region.address <= size - 4u) {
            const std::uint32_t offset = pc - region.address;
            std::uint32_t before = 0u;
            for (unsigned byte = 0u; byte < 4u; ++byte)
                before |= static_cast<std::uint32_t>(region.bytes[offset + byte]) << (byte * 8u);
            const std::uint32_t original = pc == 0x0881D558u ? 0x9445F580u : 0x9446F582u;
            const std::uint32_t replacement = pc == 0x0881D558u ? 0x24050144u : 0x24060024u;
            if (before == original && memory.load32(pc) == replacement) continue;
        }
#endif
        return true;
    }
    return false;
}
} // namespace

bool main_native_active(const psprecomp::Runtime &runtime) noexcept {
    return owner == &runtime;
}

void prepare_main_native(
    psprecomp::Runtime &runtime, const psprecomp::Elf32Image &elf, std::span<const psprecomp::PspImport> imports) {
    if (std::getenv("MHP1_NATIVE_MAIN") == nullptr) return;
#if !defined(MHP1_HAS_MAIN_AOT)
    (void)runtime;
    (void)elf;
    (void)imports;
    throw psprecomp::Error("MHP1_NATIVE_MAIN requires configuring with -DMHP1_BUILD_NATIVE_MAIN=ON");
#else
    if (owner != nullptr || runtime.function_count() != 0u)
        throw psprecomp::Error("Main AOT must be registered once, before BootHost");
    units.clear();
    import_pcs.clear();
    for (const auto &entry : imports) import_pcs.push_back(entry.stub_address);

    // Snapshot executable, file-backed sections only. Mutable data and reserved
    // overlay BSS must not invalidate the original main executable's corpus.
    for (const auto &section : elf.sections()) {
        if ((section.flags & 4u) == 0u || section.type == 8u || section.size == 0u) continue;
        const std::uint32_t start = elf.section_runtime_address(section);
        if (start < kUnitBase || start >= kFirstOverlay || section.size > kFirstOverlay - start)
            throw psprecomp::Error("Main executable section overlaps an overlay or lies outside main code");
        const std::uint32_t end = start + section.size;
        for (std::uint32_t cursor = start; cursor < end;) {
            const std::uint32_t base = kUnitBase + ((cursor - kUnitBase) / kUnitSpan) * kUnitSpan;
            const std::uint32_t count = std::min(end - cursor, base + kUnitSpan - cursor);
            Region region{cursor, std::vector<std::uint8_t>(count)};
            for (std::uint32_t i = 0; i < count; ++i) region.bytes[i] = runtime.memory().load8(cursor + i);
            units[base].regions.push_back(std::move(region));
            cursor += count;
        }
    }
    if (units.empty()) throw psprecomp::Error("No executable sections available for main AOT validation");
    psprecomp::register_generated_functions(runtime);
    if (runtime.function_count() == 0u) throw psprecomp::Error("Main AOT registry is empty");
    owner = &runtime;
    std::cout << "[mhp1-native] main registered=" << runtime.function_count() << " watched_units=" << units.size()
              << " unit_span=0x4000\n";
#endif
}

void bind_main_native_import_bridge(psprecomp::Runtime &runtime, psprecomp::Runtime::RecompiledFunction bridge) {
    if (!main_native_active(runtime)) return;
    if (bridge == nullptr) throw psprecomp::Error("Main AOT requires a valid import bridge");
    import_bridge_function = bridge;
}

void revalidate_main_native(psprecomp::Runtime &runtime) {
    if (!main_native_active(runtime)) return;
    if (import_bridge_function == nullptr) throw psprecomp::Error("Main AOT import bridge was not bound");
    for (auto &[base, unit] : units) {
        if (unit.retired) continue;
        const bool dirty = std::any_of(unit.regions.begin(), unit.regions.end(),
            [&](const Region &region) { return changed(runtime.memory(), region); });
        if (!dirty) continue;

        unsigned differences = 0u;
        for (const auto &region : unit.regions) {
            for (std::size_t i = 0; i < region.bytes.size(); ++i) {
                const auto address =
                    region.address + static_cast<std::uint32_t>(i);
                const auto current = runtime.memory().load8(address);
                if (current == region.bytes[i]) continue;
                if (differences < 16u) {
                    std::cout << "[mhp1-native-diff] address="
                            << psprecomp::hex32(address)
                            << " old="
                            << psprecomp::hex32(region.bytes[i])
                            << " new="
                            << psprecomp::hex32(current) << "\n";
                }
                ++differences;
            }
        }
        std::cout << "[mhp1-native-diff] unit="
                << psprecomp::hex32(base)
                << " changed_bytes=" << differences << "\n";

        struct Bridge {
            std::uint32_t pc;
            psprecomp::Runtime::RecompiledFunction function;
        };
        std::vector<Bridge> bridges;
        for (const auto pc : import_pcs) {
            if (pc >= base && pc < base + kUnitSpan) {
                if (runtime.has_function(pc)) bridges.push_back({pc, import_bridge_function});
            }
        }
        // unregister_functions alone does not clear the generated-unit table.
        // Poison first to disable both outer fast lookup and direct AOT chains.
        runtime.register_function(base, &poison_entry, "mhp1_native_retired");
        runtime.unregister_functions(base, base + kUnitSpan);
        for (const auto &bridge : bridges) runtime.register_function(bridge.pc, bridge.function, "mhp1_import_bridge");
        unit.retired = true;
        std::cout << "[mhp1-native] retired changed unit " << psprecomp::hex32(base)
                  << "; interpreter fallback, imports preserved=" << bridges.size() << "\n";
    }
}

void reset_main_native(psprecomp::Runtime &runtime) noexcept {
    if (owner != &runtime) return;
    owner = nullptr;
    import_bridge_function = nullptr;
    units.clear();
    import_pcs.clear();
}
} // namespace mhp1
