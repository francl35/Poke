#pragma once
#include <array>
#include <cmath>
#include <cstdint>
namespace mhp2g::gpu::ui {
// Compatibility with the traced ddeab148 HD sheet: its gauge artwork uses
// the stock 44..52 rows, while FUComplete submits 43..51 for that exact quad.
// Return a normalized UV offset; never move screen geometry or the scissor.
inline float sharpness_replacement_v_offset(bool known_sheet,
    std::uint32_t width, std::uint32_t height,
    const std::array<float,4> &screen, const std::array<float,4> &uv) {
    if (!known_sheet || width != 256 || height != 256) return 0;
    constexpr std::array<float,4> expected_screen{60,32,104,40};
    constexpr std::array<float,4> expected_uv{108,43,152,51};
    for (unsigned i=0;i<4;++i) {
        if (!std::isfinite(screen[i]) || !std::isfinite(uv[i]) ||
            std::abs(screen[i]-expected_screen[i]) > 0.01f ||
            std::abs(uv[i]-expected_uv[i]) > 0.01f) return 0;
    }
    return 1.0f/256.0f;
}
} // namespace mhp2g::gpu::ui
