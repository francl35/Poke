#pragma once

#include "psprecomp/common.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace mhp1 {
inline constexpr std::string_view kExecutableSha256 =
    "255215e8573d48e9d74bee0e78c9779789bfc4b600a8271402906c5d0d74e312";
inline constexpr std::uint32_t kEntry = 0x08899C2Cu;
inline constexpr std::uint32_t kGp = 0x0892A100u;
inline constexpr std::uint32_t kRamBytes = 32u * 1024u * 1024u;
// These are section addresses, not verified bounds of loaded overlay images.
inline constexpr std::array<std::uint32_t, 3> kOverlayBases = {0x098D4700u, 0x09A5A180u, 0x09AF6900u};

inline void require_executable_hash(std::string_view actual) {
    if (actual != kExecutableSha256) throw psprecomp::Error("Unsupported executable SHA-256: " + std::string(actual));
}

inline std::uint32_t overlay_base(std::string_view name) {
    if (name.starts_with(".")) return 0u;
    if (name.ends_with("_task.ovl")) return kOverlayBases[0];
    if (name == "demo_sub.ovl" || name == "game_sub.ovl") return kOverlayBases[1];
    if (name.size() == 11 && name.starts_with("stage") && name.ends_with(".ovl") && name[5] >= '0' && name[5] <= '9' &&
        name[6] >= '0' && name[6] <= '9')
        return kOverlayBases[2];
    return 0u;
}

struct DxConfig {
    std::string version;
    unsigned minimap_percent{};
};

inline DxConfig inspect_config(std::span<const std::uint8_t> bytes) {
    if (bytes.size() != 48u) throw psprecomp::Error("Expected a 48-byte FreedomDX configuration");
    std::string version;
    for (std::size_t i = 0; i < 8u && bytes[i] != 0u; ++i) {
        if ((bytes[i] < '0' || bytes[i] > '9') && bytes[i] != '.')
            throw psprecomp::Error("Invalid FreedomDX configuration version label");
        version.push_back(static_cast<char>(bytes[i]));
    }
    if (version != "1.8.0") throw psprecomp::Error("Unverified FreedomDX configuration format: " + version);
    return {version, bytes[0x13]};
}
} // namespace mhp1
