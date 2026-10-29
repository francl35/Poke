#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace mh::gpu {

struct KtxPackImage {
    std::uint32_t width{}, height{};
    std::vector<std::uint8_t> pixels; // top-left origin, straight-alpha RGBA8
};

[[nodiscard]] bool is_ktx2_image(std::span<const std::uint8_t> bytes);
bool decode_ktx2_image(std::span<const std::uint8_t> bytes, KtxPackImage &out, std::string &error);

} // namespace mhp2g::gpu
