// Host-only adapters for the reused PSP kernel and I/O implementations.
// These disable optional MHP2G frontend services; they are not PSP import stubs.
#include "gpu/ge_state.hpp"
#include <cstdlib>
#include "gpu/ui_textures.hpp"
#include "kernel/fast_forward.hpp"
#include "kernel/fast_loading.hpp"
#include "mods/mhp2g_mods.hpp"
#include "perf/frame_stats.hpp"
#include "settings/settings.hpp"

#if !defined(MHP1_HAS_VULKAN)
namespace mh::settings {
Settings &current() {
    // Never reads or writes MHP2G's settings or application-data directory.
    static Settings settings = [] {
        Settings value;
        value.name_entry = NameEntry::Keyboard;
        const char *renderer = std::getenv("MHP1_RENDERER");
        value.unthrottled = !renderer || std::string_view(renderer) != "1";
        value.fast_loading = false;
        value.texture_pack = false;
        value.selective_ui = false;
        value.upscale_maps = false;
        value.crisp_text = false;
        value.analog_camera = false;
        value.lock_on = false;
        value.mouse = false;
        return value;
    }();
    return settings;
}
} // namespace mhp2g::settings
#endif

#if !defined(MHP1_HAS_VULKAN)
namespace mh::fast_loading {
void note_disc_read() {}
bool note_audio(int) { return false; }
void note_buttons(bool) {}
void update() {}
bool active() {
    return false;
}
} // namespace mhp2g::fast_loading
#endif

#if !defined(MHP1_HAS_VULKAN)
namespace mh::fast_forward {
void note_bind(bool) {}
bool active() { return false; }
double speed() {
    return 1.0;
}
} // namespace mhp2g::fast_forward
#endif

#if !defined(MHP1_HAS_VULKAN)
namespace mh::perf {
void add_pacing_time(Clock::duration) {}
bool alternate_off(NewPath) {
    return false;
}
bool split_enabled() noexcept {
    return false;
}
std::uint64_t split_ticks() noexcept {
    return 0;
}
void add_split(Split, std::uint64_t) noexcept {}
} // namespace mhp2g::perf
#endif

namespace mh::gpu {} // namespace mhp2g::gpu

namespace mh::mods {
void attach_disc(IsoImage *) {}
std::optional<DiscRange> data_bin_on_disc() {
    return std::nullopt;
}
bool serving() {
    return false;
}
std::uint64_t data_bin_size() {
    return 0u;
}
std::size_t read_data_bin(std::uint64_t, std::span<std::uint8_t>) {
    return 0u;
}
} // namespace mhp2g::mods

#if !defined(MHP1_HAS_VULKAN)
namespace mh::install {
std::filesystem::path user_data_directory() {
    return {};
}
}
namespace mh::gpu::ui {
void note_direct_glyph(const psprecomp::GuestMemory &, std::uint32_t, std::uint32_t) {}
}
#endif
