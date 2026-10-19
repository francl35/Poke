#pragma once

#include "save_data/aes128.hpp"
#include <optional>
#include <string_view>

namespace mhp2g::savedata {
// Public format constants, independently confirmed against the supplied save
// and the regional-key data in FUComplete 1.4.0 PRELOAD.BIN. No loader code is copied.
inline constexpr Block kFucPreviousKey{0xcd,0x1f,0x20,0x59,0xae,0x70,0xef,0x68,0xdc,0xa2,0x45,0x13,0xb4,0x5a,0xdb,0x0a};
inline constexpr Block kFucUsKey{0x4a,0x1f,0xf3,0x59,0xae,0xb6,0xef,0xf8,0x1c,0xa8,0xcb,0x23,0xbc,0xa5,0x7b,0xb3};

inline constexpr bool reviewed_fuc_save_variant() {
#if defined(MHP2G_AOT_ELF_SHA256)
    return std::string_view(MHP2G_AOT_ELF_SHA256) ==
        "02811bb3a529d48d707072a726e87106d0ecdab62aa7e494b23ecf1469eb662f";
#else
    return false;
#endif
}

inline bool fuc_us_main_save(std::string_view game, std::string_view file) {
    return reviewed_fuc_save_variant() && game == "ULUS10391" && file == "MHP2NDG.BIN";
}

inline Block effective_save_key(std::string_view game, std::string_view file, const Block &supplied) {
    if (fuc_us_main_save(game, file) && supplied == kFucPreviousKey) return kFucUsKey;
    return supplied;
}

// Compatibility for saves produced by earlier Poke FUComplete builds. Callers
// must authenticate the file hash with this candidate before using it to decrypt.
inline std::optional<Block> previous_fuc_save_key(
    std::string_view game, std::string_view file, const Block *active) {
    if (fuc_us_main_save(game, file) && active != nullptr && *active == kFucUsKey)
        return kFucPreviousKey;
    return std::nullopt;
}
} // namespace mhp2g::savedata
