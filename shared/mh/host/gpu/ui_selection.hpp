#pragma once

#include <cstdint>

namespace mh::gpu::ui {

// Opt-in selection for the unmodified Japanese MHP2G sheets observed in a
// texture dump. Keys use xxh64, ignoreAddress=true, as TextureDumper does.
// A different palette/content falls back to native sampling until reviewed.
enum class SelectiveSheet : std::uint8_t { None, Items, Equipment, Menu, MixedHud };

[[nodiscard]] constexpr SelectiveSheet classify_ui_sheet(
    std::uint64_t cache_key, std::uint32_t data_hash, std::uint32_t width, std::uint32_t height) {
    if (width == 256u && height == 256u) {
        if (cache_key == 0x535E616Bull && data_hash == 0x070E3774u) return SelectiveSheet::Items;
        if (cache_key == 0xF3587722ull && data_hash == 0x82E3316Fu) return SelectiveSheet::Menu;
        if (cache_key == 0xE580C2A8ull && data_hash == 0x7B1FD603u) return SelectiveSheet::MixedHud;
    }
    if (width == 128u && height == 128u && cache_key == 0xE3FCE3FBull && data_hash == 0xEB2F4E67u)
        return SelectiveSheet::Equipment;
    return SelectiveSheet::None;
}

struct UiBounds {
    float u0{}, v0{}, u1{}, v1{};
};

[[nodiscard]] constexpr bool ui_region_contains(
    const UiBounds &b, float u0, float v0, float u1, float v1) {
    // All comparisons must pass, so NaNs, reversed and out-of-range bounds
    // cannot accidentally select a draw. Bounds are in original PSP texels.
    return b.u0 <= b.u1 && b.v0 <= b.v1 && b.u0 >= u0 && b.v0 >= v0 && b.u1 <= u1 && b.v1 <= v1;
}

[[nodiscard]] constexpr bool select_ui_draw(SelectiveSheet sheet, const UiBounds &bounds) {
    switch (sheet) {
    case SelectiveSheet::Items:
        return ui_region_contains(bounds, 0.0f, 0.0f, 256.0f, 256.0f);
    case SelectiveSheet::Equipment:
        return ui_region_contains(bounds, 0.0f, 0.0f, 128.0f, 128.0f);
    case SelectiveSheet::Menu:
        // Menu alphabet, button hints and game logo; this sheet has no panels.
        return ui_region_contains(bounds, 0.0f, 0.0f, 256.0f, 256.0f);
    case SelectiveSheet::MixedHud:
        // Only the button legend at the top left and status icons at the
        // top right. Health/stamina pieces, clock, sharpness, backgrounds,
        // panel borders and lower parts of this sheet remain native.
        return ui_region_contains(bounds, 0.0f, 0.0f, 174.0f, 32.0f) ||
               ui_region_contains(bounds, 184.0f, 0.0f, 256.0f, 80.0f);
    default:
        return false;
    }
}

} // namespace mhp2g::gpu::ui
