// Entirely synthetic ELF and functions: no game-derived code or assets.
#include "main_native.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
constexpr std::uint32_t kBase = 0x08804000u;
unsigned calls = 0u;
void native_entry(psprecomp::Runtime &, psprecomp::AllegrexContext &) {
    ++calls;
}
unsigned bridge_calls = 0u;
void import_bridge(psprecomp::Runtime &, psprecomp::AllegrexContext &) {
    ++bridge_calls;
}

bool points_to(psprecomp::Runtime &runtime, std::uint32_t pc, psprecomp::Runtime::RecompiledFunction expected) {
    if (!runtime.has_function(pc)) return false;
    const auto old_calls = calls;
    const auto old_bridge_calls = bridge_calls;
    psprecomp::AllegrexContext context{};
    const bool invoked = runtime.invoke_isolated_aot(pc, context);
    const bool correct = expected == &native_entry
        ? (calls == old_calls + 1u && bridge_calls == old_bridge_calls)
        : (expected == &import_bridge && bridge_calls == old_bridge_calls + 1u && calls == old_calls);
    calls = old_calls;
    bridge_calls = old_bridge_calls;
    return invoked && correct;
}
void check(bool value, const char *message) {
    if (!value) throw std::runtime_error(message);
}

psprecomp::Elf32Image synthetic_elf() {
    constexpr std::uint32_t text_size = 0x4008u;
    constexpr std::uint32_t payload_size = 0x8004u;
    constexpr std::uint32_t section_offset = 0x100u + payload_size;
    std::vector<std::uint8_t> bytes(section_offset + 120u);
    const auto word = [&](std::uint32_t offset, std::uint32_t value) {
        for (unsigned i = 0u; i < 4u; ++i) bytes[offset + i] = static_cast<std::uint8_t>(value >> (i * 8u));
    };
    const auto half = [&](std::uint32_t offset, std::uint16_t value) {
        bytes[offset] = static_cast<std::uint8_t>(value);
        bytes[offset + 1u] = static_cast<std::uint8_t>(value >> 8u);
    };
    bytes[0] = 0x7Fu;
    bytes[1] = 'E';
    bytes[2] = 'L';
    bytes[3] = 'F';
    bytes[4] = 1u;
    bytes[5] = 1u;
    bytes[6] = 1u;
    half(16u, 2u);
    half(18u, 8u);
    word(20u, 1u);
    word(24u, kBase);
    word(28u, 52u);
    word(32u, section_offset);
    half(40u, 52u);
    half(42u, 32u);
    half(44u, 1u);
    half(46u, 40u);
    half(48u, 3u);
    word(52u, 1u);
    word(56u, 0x100u);
    word(60u, kBase);
    word(64u, kBase);
    word(68u, payload_size);
    word(72u, payload_size);
    word(76u, 7u);
    word(80u, 4u);
    const auto text = section_offset + 40u;
    word(text + 4u, 1u);
    word(text + 8u, 6u);
    word(text + 12u, kBase);
    word(text + 16u, 0x100u);
    word(text + 20u, text_size);
    word(text + 32u, 4u);
    const auto data = section_offset + 80u;
    word(data + 4u, 1u);
    word(data + 8u, 3u);
    word(data + 12u, kBase + 0x8000u);
    word(data + 16u, 0x8100u);
    word(data + 20u, 4u);
    word(data + 32u, 4u);
    return psprecomp::Elf32Image::from_bytes(std::move(bytes), "synthetic-main-native");
}
} // namespace

namespace psprecomp {
void register_generated_functions(Runtime &runtime) {
    runtime.register_generated_unit(0u, kBase, 0x4000u, &native_entry);
    runtime.register_generated_unit(1u, kBase + 0x4000u, 0x4000u, &native_entry);
    runtime.register_function(kBase, &native_entry, "recomp_unit_synthetic");
    runtime.register_function(kBase + 0x4000u, &native_entry, "recomp_unit_synthetic_1");
    runtime.register_function(kBase + 0x4004u, &native_entry, "recomp_unit_synthetic_import");
}
} // namespace psprecomp

int main() {
    try {
#if defined(_WIN32)
        _putenv_s("MHP1_NATIVE_MAIN", "1");
#else
        setenv("MHP1_NATIVE_MAIN", "1", 1);
#endif
        auto elf = synthetic_elf();
        psprecomp::Runtime runtime;
        elf.load_into(runtime.memory());
        const std::array<psprecomp::PspImport, 1> imports{{{"synthetic", 1u, kBase + 0x4004u}}};
        mhp1::prepare_main_native(runtime, elf, imports);
        mhp1::bind_main_native_import_bridge(runtime, &import_bridge);
        runtime.register_function(kBase + 0x4004u, &import_bridge, "mhp1_import_bridge");
        check(mhp1::main_native_active(runtime), "Native registration missing");

        runtime.memory().store32(kBase + 0x8000u, 123u);
        mhp1::revalidate_main_native(runtime);
        check(points_to(runtime, kBase, &native_entry), "Data write invalidated executable code");

        runtime.memory().store32(kBase, 0x24020001u);
        mhp1::revalidate_main_native(runtime);
        check(!runtime.has_function(kBase), "Changed code still registered");
        runtime.cpu().pc = kBase;
        check(!runtime.invoke_chained_unit(runtime.cpu(), 0u), "Retired dense unit is still chainable");
        check(!runtime.invoke_chained_direct<&native_entry, 0u, 0u, kBase>(runtime.cpu()),
            "Retired compile-time direct chain remained executable");
        check(calls == 0u, "Stale generated function executed");
        check(points_to(runtime, kBase + 0x4000u, &native_entry), "Unchanged neighboring unit was removed");

        runtime.memory().store32(kBase + 0x4000u, 0x24020002u);
        mhp1::revalidate_main_native(runtime);
        check(!runtime.has_function(kBase + 0x4000u), "Second changed unit still registered");
        check(points_to(runtime, kBase + 0x4004u, &import_bridge), "Import bridge lost during invalidation");
        mhp1::revalidate_main_native(runtime);
        check(points_to(runtime, kBase + 0x4004u, &import_bridge), "Repeated validation removed import bridge");
        mhp1::reset_main_native(runtime);
        check(!mhp1::main_native_active(runtime), "Native ownership was not cleared");
        std::cout << "mhp1_main_native_tests PASS\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
