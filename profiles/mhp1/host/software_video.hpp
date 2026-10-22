#pragma once
#include "gpu/ge_state.hpp"
#include <array>
#include <filesystem>
namespace mhp1 {
// Deliberately limited reference rasterizer for first-pixel diagnostics.
// Unsupported draw state throws; no Vulkan, game hooks or user settings.
bool retain_video_register(std::uint32_t command, std::uint32_t value);
void rasterize_2d(
    psprecomp::GuestMemory &memory, const mhp2g::gpu::DrawCall &draw, const std::array<std::uint32_t, 256> &registers);
void capture_framebuffer(const psprecomp::GuestMemory &memory, std::uint32_t address, std::uint32_t stride,
    std::uint32_t format, const std::filesystem::path &path);
}
