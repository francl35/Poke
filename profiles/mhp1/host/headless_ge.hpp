#pragma once
#include <filesystem>
namespace mh {
class HleRegistrar;
}
namespace mhp1 {
// Executes GE commands, with optional reference 2D rasterization/captures.
void register_headless_ge(mh::HleRegistrar &hle, const std::filesystem::path &data = {});
}
