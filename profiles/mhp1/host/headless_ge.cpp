#include "headless_ge.hpp"
#include "video_backend.hpp"
#include "software_video.hpp"
#include <cstdlib>
#include "gpu/ge_state.hpp"
#include "hle/hle_common.hpp"
#include "kernel/kernel.hpp"
#include "psprecomp/common.hpp"
#if defined(MHP1_HAS_VULKAN)
#include "perf/frame_stats.hpp"
#include "settings/settings.hpp"
#endif
#include <array>
#include <cstring>
#include <deque>
#include <iostream>
#include <map>
#include <memory>

namespace mhp1 {
namespace {
using namespace mhp2g;
using psprecomp::Runtime;
using psprecomp::AllegrexContext;
// Time CPU-side GE and presentation work, including the waits the shared
// renderer already measures. The shared summary subtracts those waits once.
struct RenderMeasurement {
#if defined(MHP1_HAS_VULKAN)
    perf::Clock::time_point start{perf::Clock::now()};
    ~RenderMeasurement() { perf::add_render_time(perf::Clock::now() - start); }
#endif
};
struct List {
    std::uint32_t pc{}, stall{};
    std::int32_t callback{};
    bool done{};
};
struct Graphics {
    gpu::GeState ge;
    std::map<std::uint32_t, List> lists;
    std::map<std::int32_t, std::array<std::uint32_t, 4>> callbacks;
    std::deque<std::uint32_t> pending;
    std::uint32_t next_list{1};
    std::int32_t next_callback{1};
    bool announced{};
    bool decode_state_announced{};
    bool last_gpu_decode{};
    bool last_decode_check{};
    std::filesystem::path capture_dir;
    unsigned captures{};
    std::uint64_t next_gpu_capture_vblank{};
    bool vulkan_requested{};
    std::filesystem::path renderer_data;
    std::unique_ptr<VideoBackend> video;
    void ensure_video() {
        if (vulkan_requested && !video) video = make_video_backend(renderer_data);
    }
    bool poll(Runtime &rt) {
        if (video && !video->poll()) {
            rt.stop("MHP1 window closed");
            return false;
        }
        return true;
    }
    void present(Runtime &rt, const char *kind, bool guest_flip) {
        if (video) {
            {
                const RenderMeasurement measurement;
                video->present(displayed.address, displayed.stride, displayed.format, rt.memory());
            }
#if defined(MHP1_HAS_VULKAN)
            // Present FPS includes repeated vblank pictures. Game FPS counts
            // only framebuffer updates requested by the guest, not every vblank.
            // The backend counts only pictures actually shown; fast loads
            // can submit GPU work without presenting to the swapchain.
            if (guest_flip) perf::end_frame(kernel().now_us(), false);
#endif
            if (!capture_dir.empty() && displayed.address && captures < 8u &&
                kernel().vblank_count() >= next_gpu_capture_vblank) {
                std::filesystem::create_directories(capture_dir);
                const auto path = capture_dir / (std::string(kind) + "_" + std::to_string(captures) + ".bmp");
                if (video->capture(path)) {
                    ++captures;
                    next_gpu_capture_vblank = kernel().vblank_count() + 60u;
                    std::cout << "[mhp1-video] capture=" << path.string() << "\n";
                }
            }
            poll(rt);
        } else
            capture(rt, displayed, kind);
    }
    struct Framebuffer {
        std::uint32_t address{}, stride{}, format{};
    } displayed, pending_display;
    bool display_pending{};
    void capture(Runtime &rt, Framebuffer frame, const char *kind) {
        if (capture_dir.empty() || !frame.address || captures >= 8u) return;
        const auto path = capture_dir / (std::string(kind) + "_" + std::to_string(captures++) + ".bmp");
        capture_framebuffer(rt.memory(), frame.address, frame.stride, frame.format, path);
        std::cout << "[mhp1-video] capture=" << path.string() << "\n";
    }
    void pump(Runtime &rt) {
        ensure_video();
        if (!poll(rt)) return;
        const RenderMeasurement measurement;
        // Match MHP2G: the GE must receive the renderer's raw-vertex policy.
        // Query per pump so menu changes and renderer compatibility fallbacks
        // take effect; unsupported formats still decode on the CPU in GeState.
        const bool raw = video && video->gpu_decode();
        const bool check = raw && video->check_gpu_decode();
        ge.set_raw_vertices(raw, check);
        if (video) {
            if (!decode_state_announced || raw != last_gpu_decode || check != last_decode_check) {
                std::cout << "[mhp1-ge] vertex_decode=" << (raw ? "gpu-eligible" : "cpu")
                          << " cpu_comparison=" << (check ? "on" : "off") << "\n";
                decode_state_announced = true;
                last_gpu_decode = raw;
                last_decode_check = check;
            }
            video->begin_list();
        }
        while (!pending.empty()) {
            auto &list = lists.at(pending.front());
            ge.set_signal_sink([this, &list](std::uint32_t signal, std::uint32_t pc) {
                const auto cb = callbacks.find(list.callback);
                if (cb == callbacks.end()) return;
                const bool finish = (signal & 0x10000u) != 0u;
                const auto index = finish ? 2u : 0u;
                if (cb->second[index] == 0u) return;
                InterruptCall call{};
                call.function = cb->second[index];
                call.arguments = {signal & 0xFFFFu, cb->second[index + 1u], pc, 0u};
                kernel().queue_interrupt(std::move(call));
            });
            ge.set_transfer_sink([this, &rt](const gpu::BlockTransfer &t) {
                if (video) video->read_back(t.source, rt.memory());
                const auto bytes = static_cast<std::size_t>(t.width) * t.bytes_per_pixel;
                for (std::uint32_t row = 0; row < t.height; ++row) {
                    const auto from =
                        t.source + ((t.source_y + row) * t.source_stride + t.source_x) * t.bytes_per_pixel;
                    const auto to = t.destination +
                        ((t.destination_y + row) * t.destination_stride + t.destination_x) * t.bytes_per_pixel;
                    auto *source = rt.memory().raw_pointer(from, bytes);
                    auto *destination = rt.memory().raw_pointer(to, bytes);
                    if (!source || !destination) throw psprecomp::Error("GE transfer outside mapped memory");
                    std::memmove(destination, source, bytes);
                }
            });
            ge.set_draw_sink([this, &rt](const gpu::DrawCall &draw) {
                if (video) {
#if defined(MHP1_HAS_VULKAN)
                    perf::count_draw();
#endif
                    video->submit(draw, rt.memory());
                    return;
                }
                if (capture_dir.empty()) return;
                rasterize_2d(rt.memory(), draw, ge.command_registers());
                capture(rt, {draw.target.color_address, draw.target.color_stride, draw.target.color_format}, "draw");
            });
            const auto unhandled = ge.unhandled_command_count();
            try {
                list.pc = ge.execute(rt.memory(), list.pc, list.stall, list.done);
            } catch (const psprecomp::Error &error) {
                rt.stop(std::string("Headless GE execution failed: ") + error.what());
                return;
            }
            ge.set_draw_sink(nullptr);
            ge.set_signal_sink(nullptr);
            ge.set_transfer_sink(nullptr);
            if (ge.unhandled_command_count() != unhandled) {
                rt.stop("Headless GE encountered unsupported commands");
                return;
            }
            if (!list.done) {
                if (list.stall == 0u || list.pc != list.stall)
                    rt.stop("Headless GE list did not reach END or its stall (invalid address or command budget)");
                return;
            }
            if (!announced) {
                std::cout << "[mhp1-ge] first list completed; reference rasterization is opt-in via MHP1_RENDER_DIR\n";
                announced = true;
            }
            pending.pop_front();
        }
    }
};
}
void register_headless_ge(mhp2g::HleRegistrar &hle, const std::filesystem::path &data) {
    auto state = std::make_shared<Graphics>();
    state->renderer_data = data;
    if (const char *enabled = std::getenv("MHP1_RENDERER")) state->vulkan_requested = std::string_view(enabled) == "1";
    register_controller(hle, [state](Runtime &rt) {
        state->ensure_video();
        state->poll(rt);
        return state->video ? state->video->pad() : Pad{};
    });
    if (const char *dir = std::getenv("MHP1_RENDER_DIR"); dir && *dir) {
        state->capture_dir = std::filesystem::absolute(dir);
        std::cout << "[mhp1-video] captures enabled; GPU mode samples at most once per 60 vblanks; limit=8\n";
    }
    if (!std::getenv("MHP1_STRICT_GE")) state->ge.set_state_command_sink(retain_video_register);
    // Hooks are process-scoped, as is this one-session diagnostic kernel.
    auto hooked = std::make_shared<bool>(false);
    const auto attach = [state, hooked](Runtime &rt) {
        if (*hooked) return;
        *hooked = true;
        kernel().set_poll_hook([state, &rt] { state->poll(rt); });
        kernel().add_vblank_hook([state, &rt] {
            const bool guest_flip = state->display_pending;
            if (state->display_pending) {
                state->displayed = state->pending_display;
                state->display_pending = false;
            }
            // Diagnostic opt-in: preserve every guest vblank, but do not submit
            // the same displayed picture again without a guest buffer update.
            // Event polling must continue even when no picture is presented.
            bool skip_repeat = std::getenv("MHP1_PRESENT_ON_FLIP") != nullptr;
#if defined(MHP1_HAS_VULKAN)
            // Interpolation consumes true guest flips. A repeat at each vblank
            // would reset its pair/cycle as if the guest had supplied 60 frames.
            skip_repeat = skip_repeat || mhp2g::settings::current().frame_rate != mhp2g::settings::FrameRate::Fps30;
#endif
            if (!guest_flip && skip_repeat) {
                state->poll(rt);
                return;
            }
            try {
                state->present(rt, "vblank", guest_flip);
            } catch (const psprecomp::Error &e) {
                rt.stop(e.what());
            }
        });
    };
    hle.add("sceDisplay", "sceDisplaySetFrameBuf", [state, attach](Runtime &rt, AllegrexContext &ctx) {
        attach(rt);
        Graphics::Framebuffer fb{arg(ctx, 0), arg(ctx, 1), arg(ctx, 2)};
        const auto sync = arg(ctx, 3);
        if (sync > 1u ||
            (fb.address &&
                (fb.format > 3u || fb.stride < 480u || fb.stride > 2048u || (fb.stride & (fb.stride - 1u)) ||
                    (fb.address & 15u) ||
                    !rt.memory().contains(fb.address, (271u * fb.stride + 480u) * (fb.format == 3u ? 4u : 2u))))) {
            kernel().finish(ctx, error::kIllegalArgument);
            return;
        }
        if (sync == 0u) {
            state->displayed = fb;
            state->display_pending = false;
            try {
                state->ensure_video();
                state->present(rt, "display", true);
            } catch (const psprecomp::Error &e) {
                rt.stop(e.what());
                return;
            }
        } else {
            state->pending_display = fb;
            state->display_pending = true;
        }
        kernel().finish(ctx, 0u);
    });
    hle.add("sceGe_user", "sceGeSetCallback", [state](Runtime &rt, AllegrexContext &ctx) {
        const auto address = arg(ctx, 0);
        if (!address || !rt.memory().contains(address, 16u)) {
            kernel().finish(ctx, error::kIllegalArgument);
            return;
        }
        std::array<std::uint32_t, 4> cb{};
        for (unsigned i = 0; i < 4; ++i) cb[i] = rt.memory().load32(address + i * 4u);
        const auto id = state->next_callback++;
        state->callbacks.emplace(id, cb);
        kernel().finish(ctx, id);
    });
    hle.add("sceGe_user", "sceGeUnsetCallback", [state](Runtime &, AllegrexContext &ctx) {
        const auto removed = state->callbacks.erase(static_cast<std::int32_t>(arg(ctx, 0)));
        kernel().finish(ctx, removed ? 0u : error::kIllegalArgument);
    });
    const auto enqueue = [state, attach](bool head, Runtime &rt, AllegrexContext &ctx) {
        attach(rt);
        const auto pc = arg(ctx, 0) & 0x0FFFFFFFu;
        const auto stall = arg(ctx, 1) & 0x0FFFFFFFu;
        const auto callback = static_cast<std::int32_t>(arg(ctx, 2));
        if (arg(ctx, 3) != 0u) {
            rt.stop("Headless GE enqueue options/context are not supported yet");
            return;
        }
        if ((pc & 3u) || !rt.memory().contains(pc, 4u) || (stall & 3u) ||
            (callback != -1 && !state->callbacks.contains(callback))) {
            kernel().finish(ctx, error::kIllegalArgument);
            return;
        }
        // Inserting ahead of a partially executed list needs GE context save/restore.
        if (head && !state->pending.empty()) {
            rt.stop("Headless GE head insertion into an active queue is not supported yet");
            return;
        }
#if defined(MHP1_HAS_VULKAN)
        perf::count_display_list();
#endif
        const auto id = state->next_list++;
        state->lists.emplace(id, List{pc, stall, callback, false});
        state->pending.push_back(id);
        state->pump(rt);
        if (!rt.stopped()) kernel().finish(ctx, id);
    };
    hle.add(
        "sceGe_user", "sceGeListEnQueue", [enqueue](Runtime &rt, AllegrexContext &ctx) { enqueue(false, rt, ctx); });
    hle.add(
        "sceGe_user", "sceGeListEnQueueHead", [enqueue](Runtime &rt, AllegrexContext &ctx) { enqueue(true, rt, ctx); });
    hle.add("sceGe_user", "sceGeListUpdateStallAddr", [state](Runtime &rt, AllegrexContext &ctx) {
        const auto list = state->lists.find(arg(ctx, 0));
        if (list == state->lists.end() || (arg(ctx, 1) & 3u)) {
            kernel().finish(ctx, error::kIllegalArgument);
            return;
        }
        list->second.stall = arg(ctx, 1) & 0x0FFFFFFFu;
        state->pump(rt);
        if (!rt.stopped()) kernel().finish(ctx, 0u);
    });
    hle.add("sceGe_user", "sceGeListSync", [state](Runtime &rt, AllegrexContext &ctx) {
        const auto list = state->lists.find(arg(ctx, 0));
        const auto mode = arg(ctx, 1);
        if (list == state->lists.end() || mode > 1u) {
            kernel().finish(ctx, error::kIllegalArgument);
            return;
        }
        if (mode == 0u && !list->second.done) {
            const auto id = arg(ctx, 0);
            kernel().wait_host(ctx, std::nullopt, [state, id](bool) -> std::optional<std::uint32_t> {
                const auto found = state->lists.find(id);
                if (found == state->lists.end()) return error::kIllegalArgument;
                if (found->second.done) return 0u;
                return std::nullopt;
            });
            return;
        }
        const bool queued = !state->pending.empty() && state->pending.front() != arg(ctx, 0);
        kernel().finish(ctx, list->second.done ? 0u : queued ? 1u : 3u);
    });
    hle.add("sceGe_user", "sceGeDrawSync", [state](Runtime &rt, AllegrexContext &ctx) {
        if (arg(ctx, 0) > 1u) {
            kernel().finish(ctx, error::kIllegalArgument);
            return;
        }
        if (arg(ctx, 0) == 0u && !state->pending.empty()) {
            kernel().wait_host(ctx, std::nullopt, [state](bool) -> std::optional<std::uint32_t> {
                if (state->pending.empty()) return 0u;
                return std::nullopt;
            });
            return;
        }
        kernel().finish(ctx, state->pending.empty() ? 0u : 3u);
    });
}
}
