#include "video_backend.hpp"
#include "app_branding.hpp"
#include "psprecomp/common.hpp"
#if defined(MHP1_HAS_VULKAN)
#include "gpu/vulkan_renderer.hpp"
#include "settings/settings.hpp"
#include "kernel/fast_loading.hpp"
#include "kernel/fast_forward.hpp"
#include "kernel/kernel.hpp"
#include "perf/frame_stats.hpp"
#include "ui/layer.hpp"
#include "ui/text_input.hpp"
#include <iostream>
#include "ui/ui.hpp"
#include "audio/audio_sink.hpp"

namespace mhp1 {
// The shared renderer's caches are isolated from the original application's data.
std::filesystem::path &renderer_data() {
    static std::filesystem::path path;
    return path;
}
namespace {
class VulkanVideo final : public VideoBackend {
    mh::gpu::VulkanRenderer renderer_;
    bool pause_menu_pending_{};
    bool quit_{};

    void update_speed_controls() {
        renderer_.sample_pad();
        const auto value = renderer_.pad();
        mh::fast_forward::note_bind(value.fast_forward);
        mh::fast_loading::note_buttons(value.buttons != 0u);
        mh::fast_loading::update();
        const bool accelerated = mh::fast_loading::active() || mh::fast_forward::active();
        renderer_.set_fast_forward(accelerated);
        if (accelerated) renderer_.pause_interpolation();
    }

public:
    explicit VulkanVideo(const std::filesystem::path &data) {
        renderer_data() = std::filesystem::absolute(data);
        std::filesystem::create_directories(renderer_data());
        std::string error;
        if (!renderer_.initialize({MH_APP_NAME " — Monster Hunter Portable / FreedomDX"}, error))
            throw psprecomp::Error("MHP1 Vulkan initialization failed: " + error);
        renderer_.set_texture_pack(
            mh::settings::current().texture_pack);
        if (!mh::ui::Layer::get().attach(renderer_))
            throw psprecomp::Error("MHP1 could not initialize the shared UI layer");
        renderer_.set_frame_rate(mh::settings::current().frame_rate);
        renderer_.set_frame_rate_auto(mh::settings::current().frame_rate_auto);
        renderer_.begin_frame();
        mh::perf::restart_measurement();
        // Service host-only presents while the guest's unchanged clock waits.
        mh::kernel().set_idle_hook([this](std::chrono::steady_clock::time_point wake) {
            renderer_.present_until(wake);
        });
        std::cout << "[mhp1-video] Vulkan renderer initialized; MHP2G camera/HUD/font enhancement hooks disabled\n";
    }
    ~VulkanVideo() override {
        mh::kernel().set_idle_hook({});
    }
    bool gpu_decode() const override { return renderer_.gpu_decode(); }
    bool check_gpu_decode() const override { return renderer_.check_gpu_decode(); }
    void begin_list() override { renderer_.begin_display_list(); }
    void submit(const mh::gpu::DrawCall &draw, psprecomp::GuestMemory &memory) override {
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
        // draw_over_game handles both the menu and the game's text input.
        renderer_.set_ui_draw_data(nullptr);
        update_speed_controls();
        mh::ui::draw_over_game();
        const auto moment = mh::kernel().real_time_of(mh::kernel().now_us());
        const bool presented = renderer_.present(address, moment);
        if (presented) mh::perf::count_present();
        // Polling can occur inside GE recording: wait for a submitted frame.
        if (pause_menu_pending_) {
            pause_menu_pending_ = false;
            renderer_.pause_interpolation();
            renderer_.set_fast_forward(false);
            auto &audio = mh::audio::AudioSink::instance();
            audio.set_paused(true);
            const auto resume = [&] {
                audio.set_paused(false);
                renderer_.pause_interpolation();
                mh::kernel().resync_real_time();
                mh::perf::restart_measurement();
            };
            try {
                std::cout << "[mhp1-menu] guest paused\n";
                quit_ = !mh::ui::run_menu();
            } catch (...) {
                resume();
                throw;
            }
            resume();
            renderer_.set_ui_draw_data(nullptr);
            std::cout << "[mhp1-menu] guest resumed\n";
        }
        renderer_.begin_frame();
    }
    bool poll() override {
        if (quit_ || !renderer_.pump_events())
            return false;

        if (mh::ui::take_quit_request())
            return false;

        if (!pause_menu_pending_ && !mh::ui::menu_over_game() && mh::ui::menu_requested()) {
            if (mh::ui::menu_pauses())
                pause_menu_pending_ = true;
            else
                mh::ui::open_menu_over_game();
        }

        update_speed_controls();
        renderer_.present_due();
        return true;
    }
    Pad pad() override {
        update_speed_controls();
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
namespace mh::gpu::hud {
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
