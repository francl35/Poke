// MHP1 host policy for the shared, independently tested load detector.
#include "kernel/fast_loading.hpp"
#include "kernel/fast_forward.hpp"

#include "hle/hle_common.hpp"
#include "settings/settings.hpp"
#if defined(MHP1_HAS_VULKAN)
#include "ui/ui.hpp"
#include "ui/text_input.hpp"
#endif

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace mh::fast_loading {

namespace {

using Clock = std::chrono::steady_clock;

struct State {
    Detector detector;
    bool buttons_held{};
    // The episode running fast now: when it started, in both clocks.
    Clock::time_point real_start{};
    std::uint64_t emulated_start{};
    // Everything saved in this run.
    double saved_ms{};
    std::uint64_t episodes{};
};

State &state() {
    static State instance;
    return instance;
}

bool windowed() {
    const char *renderer = std::getenv("MHP1_RENDERER");
    const bool value = renderer != nullptr && std::string_view(renderer) == "1";
    return value;
}

Guards sample_guards() {
    const settings::Settings &s = settings::current();
    Guards guards;
    guards.enabled = s.fast_loading && !s.unthrottled && windowed() && !fast_forward::active();
    guards.buttons_held = state().buttons_held;
    guards.movie = mpeg_active();
    // MHP1 networking is not implemented. Conservatively block acceleration
    // whenever the shared wireless setting is enabled; a future networking
    // backend must also supply its live initialization/session guards here.
    guards.online = s.adhoc;
#if defined(MHP1_HAS_VULKAN)
    guards.menu = ui::menu_over_game() || ui::text_input_open();
#endif
    return guards;
}

void begin_episode() {
    State &s = state();
    s.real_start = Clock::now();
    s.emulated_start = kernel().now_us();
    std::printf("[load] fast loading started (MHP1)\n");
    std::fflush(stdout);
}

void end_episode(Reason reason) {
    State &s = state();
    const double real_ms = std::chrono::duration<double, std::milli>(Clock::now() - s.real_start).count();
    const double emulated_ms = static_cast<double>(kernel().now_us() - s.emulated_start) / 1000.0;
    const double saved = emulated_ms > real_ms ? emulated_ms - real_ms : 0.0;
    s.saved_ms += saved;
    ++s.episodes;
    std::printf("[load] fast %.0f ms of game time in %.0f ms real, %.0f ms saved (%s; %.1f s saved so far)\n",
        emulated_ms, real_ms, saved, reason_name(reason), s.saved_ms / 1000.0);
    std::fflush(stdout);
}

} // namespace

void note_disc_read() {
    state().detector.disc_read(kernel().now_us());
}

bool note_audio(int peak) {
    Detector &detector = state().detector;
    const bool was_fast = detector.fast();
    const bool drop = detector.audio(kernel().now_us(), peak);
    if (was_fast && !detector.fast()) end_episode(detector.reason());
    return drop;
}

void note_buttons(bool held) {
    state().buttons_held = held;
}

void update() {
    Detector &detector = state().detector;
    const bool was_fast = detector.fast();
    const bool fast = detector.update(kernel().now_us(), sample_guards());
    if (fast && !was_fast)
        begin_episode();
    else if (!fast && was_fast)
        end_episode(detector.reason());
}

bool active() {
    return state().detector.fast();
}

} // namespace mhp2g::fast_loading
