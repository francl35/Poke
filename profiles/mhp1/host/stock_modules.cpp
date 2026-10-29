// Virtual stock PSP modules whose exports are supplied by this profile's HLE.
// Unknown modules and all title-specific executable loading remain unsupported.
#include "hle/hle_common.hpp"
#include "audio/atrac_decoder.hpp"
#include "movie/avc_decoder.hpp"
#include "psprecomp/common.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <iostream>
#include <memory>
#include <set>
namespace mhp1 {
void register_stock_modules(mh::HleRegistrar &hle) {
    using namespace mh;
    auto modules = std::make_shared<std::set<std::uint32_t>>();
    hle.add("ModuleMgrForUser", "sceKernelLoadModuleByID", [modules](Runtime &rt, AllegrexContext &ctx) {
        std::array<std::uint8_t, 256> header{};
        const auto count = read_open_file(arg(ctx, 0), 0, header.data(), header.size());
        std::string name;
        for (std::size_t i = 0; i + 0x2Au < count; ++i)
            if (std::memcmp(header.data() + i, "~PSP", 4) == 0) {
                for (unsigned j = 0; j < 28 && header[i + 10 + j]; ++j)
                    name.push_back(static_cast<char>(header[i + 10 + j]));
                break;
            }
        auto lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
        const bool font = lower == "scefont_library";
        const bool sas = lower == "scesascore";
        const bool codec = lower == "sceatrac3plus_library" || lower == "scempeg_library" ||
            lower == "scempegbase_library" || lower == "scempegbase_driver" || lower == "sceaudiocodec_driver" || lower == "scevideocodec_driver";
        if (!font && !codec && !sas) {
            rt.stop("Unsupported stock/module image in sceKernelLoadModuleByID: " + name);
            return;
        }
        if (codec && (!audio::AtracDecoder::available() || !movie::AvcDecoder::available())) {
            rt.stop("Stock codec module requires -DMHP1_CODECS=ON: " + name);
            return;
        }
        const auto uid = static_cast<std::uint32_t>(kernel().allocate_uid());
        modules->insert(uid);
        std::cout << "[mhp1-module] HLE stock module=" << name << " uid=" << uid << "\n";
        kernel().finish(ctx, uid);
    });
    hle.add("ModuleMgrForUser", "sceKernelStartModule", [modules](Runtime &rt, AllegrexContext &ctx) {
        const auto uid = arg(ctx, 0), out = arg(ctx, 3);
        if (!modules->contains(uid) || (out && !rt.memory().contains(out, 4u))) {
            kernel().finish(ctx, error::kIllegalArgument);
            return;
        }
        if (out) rt.memory().store32(out, 0);
        kernel().finish(ctx, uid);
    });
    hle.add("ModuleMgrForUser", "sceKernelStopModule", [modules](Runtime &rt, AllegrexContext &ctx) {
        const auto uid = arg(ctx, 0), out = arg(ctx, 3);
        if (!modules->contains(uid) || (out && !rt.memory().contains(out, 4u))) {
            kernel().finish(ctx, error::kIllegalArgument);
            return;
        }
        if (out) rt.memory().store32(out, 0);
        kernel().finish(ctx, 0);
    });
    hle.add("ModuleMgrForUser", "sceKernelUnloadModule", [modules](Runtime &, AllegrexContext &ctx) {
        kernel().finish(ctx, modules->erase(arg(ctx, 0)) ? 0u : error::kIllegalArgument);
    });
}
}
