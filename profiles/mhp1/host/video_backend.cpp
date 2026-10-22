#include "video_backend.hpp"
#include "psprecomp/common.hpp"
#if defined(MHP1_HAS_VULKAN)
#include "gpu/vulkan_renderer.hpp"
#include "settings/settings.hpp"
#include "ui/layer.hpp"
#include "ui/text_input.hpp"
#include <iostream>
#include "ui/ui.hpp"

namespace mhp1 {
// The shared renderer's caches are isolated from the original application's data.
std::filesystem::path &renderer_data() {
    static std::filesystem::path path;
    return path;
}
namespace {
class VulkanVideo final : public VideoBackend {
    mhp2g::gpu::VulkanRenderer renderer_;

public:
    explicit VulkanVideo(const std::filesystem::path &data) {
        renderer_data() = std::filesystem::absolute(data);
        std::filesystem::create_directories(renderer_data());
        std::string error;
        if (!renderer_.initialize({"Monster Hunter Portable / FreedomDX (experimental)"}, error))
            throw psprecomp::Error("MHP1 Vulkan initialization failed: " + error);
        renderer_.set_texture_pack(
            mhp2g::settings::current().texture_pack);
        if (!mhp2g::ui::Layer::get().attach(renderer_))
            throw psprecomp::Error("MHP1 could not initialize the shared UI layer");
        renderer_.begin_frame();
        std::cout << "[mhp1-video] Vulkan renderer initialized; MHP2G camera/HUD/font enhancement hooks disabled\n";
    }
    void begin_list() override { renderer_.begin_display_list(); }
    void submit(const mhp2g::gpu::DrawCall &draw, psprecomp::GuestMemory &memory) override {
        renderer_.submit(draw, memory);
    }
    void read_back(std::uint32_t source, psprecomp::GuestMemory &memory) override {
        renderer_.read_back_framebuffer(source, memory);
    }
    void present(
        std::uint32_t address, std::uint32_t stride, std::uint32_t format, psprecomp::GuestMemory &memory) override {
        if (!address) {
            static const std::vector<std::uint32_t> black(480u * 272u, 0xFF000000u);
            renderer_.upload_frame(0, reinterpret_cast<const std::uint8_t *>(black.data()), 480, 272, 480);
        } else if (memory.canonical(address) >= 0x08000000u) {
            const auto *pixels = memory.raw_pointer(address, static_cast<std::size_t>(stride) * 272u * 4u);
            if (format != 3u || !pixels)
                throw psprecomp::Error("CPU movie presentation requires a mapped RGBA8888 framebuffer");
            renderer_.upload_frame(address, pixels, 480, 272, stride);
        } else
            renderer_.write_back_frame(memory);
        if (mhp2g::ui::text_input_open()) {
            auto &layer = mhp2g::ui::Layer::get();
            layer.begin_frame();
            mhp2g::ui::text_input_frame();
            layer.end_frame();
        } else {
            renderer_.set_ui_draw_data(nullptr);
        }
        renderer_.set_ui_draw_data(nullptr);
        mhp2g::ui::draw_over_game();
        renderer_.present(address);
        renderer_.begin_frame();
    }
    bool poll() override {
        if (!renderer_.pump_events())
            return false;

        if (mhp2g::ui::take_quit_request())
            return false;

        if (mhp2g::ui::menu_requested())
            mhp2g::ui::open_menu_over_game();

        return true;
    }
    Pad pad() override {
        renderer_.sample_pad();
        const auto value = renderer_.pad();
        return {value.buttons, value.analog_x, value.analog_y};
    }
    bool capture(const std::filesystem::path &path) override { return renderer_.capture_frame(path); }
};
}
std::unique_ptr<VideoBackend> make_video_backend(const std::filesystem::path &data) {
    return std::make_unique<VulkanVideo>(data);
}
}
// namespace mhp2g::install {
// std::filesystem::path user_data_directory() {
//     return mhp1::renderer_data();
// }
// }
namespace mhp2g::gpu::hud {
bool hides(std::uint32_t, std::uint32_t) {
    return false;
}
}
// namespace mhp2g::settings {
// const char *overridden_by(const char *) {
//     return nullptr;
// }
// }
#else
namespace mhp1 {
std::unique_ptr<VideoBackend> make_video_backend(const std::filesystem::path &) {
    throw psprecomp::Error("MHP1_RENDERER=1 requires configuring -DMHP1_VULKAN=ON and rebuilding");
}
}
#endif
