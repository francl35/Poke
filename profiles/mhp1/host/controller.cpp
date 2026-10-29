#include "video_backend.hpp"
#include "hle/hle_common.hpp"
#include "kernel/fast_loading.hpp"
#include <array>
#include <memory>
namespace mhp1 {
void register_controller(mh::HleRegistrar &hle, std::function<Pad(psprecomp::Runtime &)> sample) {
    using namespace mh;
    auto cycle = std::make_shared<std::uint32_t>(0u), mode = std::make_shared<std::uint32_t>(0u);
    hle.add("sceCtrl", "sceCtrlSetSamplingCycle", [cycle](Runtime &, AllegrexContext &ctx) {
        const auto value = arg(ctx, 0), previous = *cycle;
        if (value != 0u && (value < 5555u || value > 20000u)) {
            kernel().finish(ctx, error::kIllegalArgument);
            return;
        }
        *cycle = value;
        kernel().finish(ctx, previous);
    });
    hle.add("sceCtrl", "sceCtrlSetSamplingMode", [mode](Runtime &, AllegrexContext &ctx) {
        const auto value = arg(ctx, 0), previous = *mode;
        if (value > 1u) {
            kernel().finish(ctx, error::kIllegalArgument);
            return;
        }
        *mode = value;
        kernel().finish(ctx, previous);
    });
    hle.add("sceCtrl", "sceCtrlReadBufferPositive", [cycle, mode, sample](Runtime &rt, AllegrexContext &ctx) {
        const auto address = arg(ctx, 0), count = arg(ctx, 1);
        if (count == 0 || count > 64 || !rt.memory().contains(address, static_cast<std::size_t>(count) * 16u)) {
            kernel().finish(ctx, error::kIllegalArgument);
            return;
        }
        const auto pad = sample(rt);
        if (rt.stopped()) return;
        fast_loading::note_buttons(pad.buttons != 0u);
        fast_loading::update();
        for (std::uint32_t i = 0; i < count; ++i) {
            const auto entry = address + i * 16u;
            rt.memory().store32(entry, static_cast<std::uint32_t>(kernel().now_us()));
            rt.memory().store32(entry + 4, pad.buttons);
            rt.memory().store8(entry + 8, *mode ? pad.x : 128u);
            rt.memory().store8(entry + 9, *mode ? pad.y : 128u);
            for (unsigned j = 10; j < 16; ++j) rt.memory().store8(entry + j, 0);
        }
        if (*cycle)
            kernel().delay_current(ctx, *cycle, count);
        else {
            WaitState wait{};
            wait.type = WaitType::VBlank;
            kernel().block(ctx, wait, count);
        }
    });
}
}
