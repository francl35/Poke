#pragma once
// Experimental FUComplete compatibility. Code changes are observed only at
// guest instruction-cache flush boundaries, never by polling gameplay data.
#include "psprecomp/runtime.hpp"
#include "psprecomp/elf32.hpp"
#include "psprecomp/common.hpp"
#include "settings/settings.hpp"
#include <optional>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string_view>
#include <vector>

namespace mhp2g::fuc_code_patches {
struct CodeRange {
    std::uint32_t base{};
    std::vector<std::uint8_t> previous;
};
inline auto &states() {
    static std::map<psprecomp::Runtime *, std::vector<CodeRange>> value;
    return value;
}
inline bool requested() {
    // Environment overrides are applied and displayed by the settings system.
    return settings::current().fuc_runtime_patches;
}
inline std::optional<bool> &startup_choice() {
    static std::optional<bool> value;
    return value;
}
inline bool restart_pending() {
    const auto value = startup_choice();
    return value.has_value() && *value != requested();
}
inline bool active(psprecomp::Runtime &runtime) { return states().contains(&runtime); }
inline void initialize(psprecomp::Runtime &runtime, const psprecomp::Elf32Image &elf,
                       std::uint32_t load_base, std::string_view corpus_hash) {
    states().erase(&runtime);
    // Capture once. Menu changes must not affect this running session, since
    // disabled compiled units cannot safely be restored without a fresh runtime.
    startup_choice() = requested();
    if (!*startup_choice()) {
        std::cerr << "[fuc-code] disabled by startup setting; changing it requires restarting Poke\n";
        return;
    }
    constexpr std::string_view expected =
        "02811bb3a529d48d707072a726e87106d0ecdab62aa7e494b23ecf1469eb662f";
    if (corpus_hash != expected) {
        std::cerr << "[fuc-code] disabled: this experiment requires the verified FUComplete corpus\n";
        return;
    }
    std::vector<CodeRange> ranges;
    std::size_t total = 0;
    for (const auto &section : elf.sections()) {
        // SHF_ALLOC | SHF_EXECINSTR. Do not monitor writable data/BSS as code.
        if ((section.flags & 6u) != 6u || section.type == 8u || section.size == 0u) continue;
        const auto base = elf.section_runtime_address(section, load_base);
        if (!runtime.memory().contains(base, section.size) || section.size > 16u * 1024u * 1024u)
            continue;
        CodeRange range{base, std::vector<std::uint8_t>(section.size)};
        for (std::uint32_t i = 0; i < section.size; ++i)
            range.previous[i] = runtime.memory().load8(base + i);
        total += range.previous.size();
        ranges.push_back(std::move(range));
    }
    if (ranges.empty()) {
        std::cerr << "[fuc-code] disabled: no allocated executable ELF sections available\n";
        return;
    }
    states().emplace(&runtime, std::move(ranges));
    std::cerr << "[fuc-code] enabled: watching " << total
              << " executable bytes; changed generated units use the interpreter\n";
}
inline void synchronize(psprecomp::Runtime &runtime) {
    const auto found = states().find(&runtime);
    if (found == states().end()) return;
    auto &memory = runtime.memory();
    for (auto &range : found->second) {
        const auto *live = memory.raw_pointer(range.base, range.previous.size());
        if (!live) continue;
        for (std::size_t i = 0; i < range.previous.size(); ++i) {
            if (live[i] == range.previous[i]) continue;
            const auto address = range.base + static_cast<std::uint32_t>(i);
            const auto removed = runtime.invalidate_generated_code_unit(address);
            // The first changed byte in a generated unit clears all of that
            // unit's translated entries. Following changed bytes find none.
            if (removed)
                std::cerr << "[fuc-code] changed=" << psprecomp::hex32(address)
                          << " removed_aot_entries=" << removed << '\n';
        }
        std::copy(live, live + range.previous.size(), range.previous.begin());
    }
}
} // namespace mhp2g::fuc_code_patches
