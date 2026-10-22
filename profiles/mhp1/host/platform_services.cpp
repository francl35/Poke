// PSP platform services, independent of game addresses and overlays.
#include "hle/hle_common.hpp"
#include "psprecomp/common.hpp"
#include <chrono>
#include <ctime>
#include <memory>
namespace mhp1 {
void register_platform_services(mhp2g::HleRegistrar &hle) {
    using namespace mhp2g;
    hle.add("sceUtility", "sceUtilityLoadNetModule",[](Runtime &rt, AllegrexContext &ctx) {
        const auto id = arg(ctx, 0);
        if (id != 1u && id != 2u) {
            rt.stop("Unsupported sceUtilityLoadNetModule id=" +
                    std::to_string(id));
            return;
        }
        kernel().finish(ctx, 0);
    });
    hle.add("sceWlanDrv", "sceWlanGetSwitchState", [](Runtime &, AllegrexContext &ctx) { kernel().finish(ctx, 0); });
    hle.add("sceWlanDrv", "sceWlanGetEtherAddr", [](Runtime &rt, AllegrexContext &ctx) {
        const auto out = arg(ctx, 0);
        if (!rt.memory().contains(out, 6u)) {
            kernel().finish(ctx, error::kIllegalArgument);
            return;
        }
        const std::uint8_t mac[] = {0x02, 0x4D, 0x48, 0x50, 0x01, 0x01};
        for (unsigned i = 0; i < 6; ++i) rt.memory().store8(out + i, mac[i]);
        kernel().finish(ctx, 0);
    });
    auto exit_callback = std::make_shared<SceUID>(0);
    hle.add("LoadExecForUser", "sceKernelRegisterExitCallback", [exit_callback](Runtime &, AllegrexContext &ctx) {
        const auto uid = static_cast<SceUID>(arg(ctx, 0));
        if (!kernel().callbacks.contains(uid)) {
            kernel().finish(ctx, error::kIllegalArgument);
            return;
        }
        *exit_callback = uid;
        kernel().finish(ctx, 0);
    });
    hle.add("scePower", "scePowerRegisterCallback", [](Runtime &, AllegrexContext &ctx) {
        const auto uid = static_cast<SceUID>(arg(ctx, 1));
        if (!kernel().callbacks.contains(uid)) {
            kernel().finish(ctx, error::kIllegalArgument);
            return;
        }
        // PSP reports the AC/battery state to the registered guest callback.
        kernel().notify_callback(uid, 0x1084u);
        kernel().finish(ctx, 0);
    });
    const auto epoch = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count());
    hle.add("sceRtc", "sceRtcGetCurrentClockLocalTime", [epoch](Runtime &rt, AllegrexContext &ctx) {
        const auto out = arg(ctx, 0);
        if (!rt.memory().contains(out, 16u)) {
            kernel().finish(ctx, error::kIllegalArgument);
            return;
        }
        const auto us = epoch + kernel().now_us();
        const auto seconds = static_cast<std::time_t>(us / 1000000u);
        std::tm value{};
#if defined(_WIN32)
        localtime_s(&value, &seconds);
#else
        localtime_r(&seconds,&value);
#endif
        const std::uint16_t fields[] = {static_cast<std::uint16_t>(value.tm_year + 1900),
            static_cast<std::uint16_t>(value.tm_mon + 1), static_cast<std::uint16_t>(value.tm_mday),
            static_cast<std::uint16_t>(value.tm_hour), static_cast<std::uint16_t>(value.tm_min),
            static_cast<std::uint16_t>(value.tm_sec)};
        for (unsigned i = 0; i < 6; ++i) rt.memory().store16(out + i * 2u, fields[i]);
        rt.memory().store32(out + 12u, static_cast<std::uint32_t>(us % 1000000u));
        kernel().finish(ctx, 0);
    });
    hle.add("sceOpenPSID", "sceOpenPSIDGetOpenPSID", [](Runtime &rt, AllegrexContext &ctx) {
        const auto out = arg(ctx, 0);
        if (!rt.memory().contains(out, 16u)) {
            kernel().finish(ctx, error::kIllegalArgument);
            return;
        }
        // A deterministic synthetic console identity for this isolated profile.
        for (unsigned i = 0; i < 16; ++i) rt.memory().store8(out + i, static_cast<std::uint8_t>(0x10u + i));
        kernel().finish(ctx, 0);
    });
    hle.add("sceUtility", "sceUtilityGetSystemParamInt", [](Runtime &rt, AllegrexContext &ctx) {
        const auto out = arg(ctx, 1);
        if (!rt.memory().contains(out, 4u)) {
            kernel().finish(ctx, error::kIllegalArgument);
            return;
        }
        const auto id = arg(ctx, 0);
        std::uint32_t value = 0;
        if (id == 2)
            value = 1; // automatic ad hoc channel
        else if (id != 4 && id != 5 && id != 8 && id != 9) {
            kernel().finish(ctx, error::kIllegalArgument);
            return;
        }
        rt.memory().store32(out, value);
        kernel().finish(ctx, 0);
    });
}
}
