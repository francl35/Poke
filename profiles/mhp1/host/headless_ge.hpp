#pragma once
#include <filesystem>
namespace mhp2g {
class HleRegistrar;
}
namespace mhp1 {
// Executes GE commands, with optional reference 2D rasterization/captures.
void register_headless_ge(mhp2g::HleRegistrar &hle, const std::filesystem::path &data = {});
}
