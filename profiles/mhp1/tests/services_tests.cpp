#include "boot_host.hpp"
#include "hle/hle_common.hpp"
#include "psprecomp/common.hpp"
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
namespace {
void check(bool ok, const char *msg) {
    if (!ok) throw psprecomp::Error(msg);
}
constexpr auto data = 0x08840000u;
}
int main() {
    try {
        const auto folder = std::filesystem::temp_directory_path() /
            ("mhp1-services-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        struct Cleanup {
            std::filesystem::path p;
            ~Cleanup() {
                std::error_code e;
                std::filesystem::remove_all(p, e);
            }
        } cleanup{folder};
        std::filesystem::create_directories(folder);
        std::ofstream(folder / "fixture.bin", std::ios::binary) << "abcdef";
        psprecomp::Runtime rt;
        mhp1::BootHost host(rt, {}, 0, 0x08900000u, {}, folder);
        auto &ctx = rt.cpu();
        auto call = [&](const char *library, std::uint32_t nid, std::array<std::uint32_t, 5> args = {}) {
            for (unsigned i = 0; i < args.size(); ++i) ctx.gpr[4 + i] = args[i];
            rt.invoke_import(library, nid, ctx);
            check(!rt.stopped(), "Service stopped runtime");
            return ctx.gpr[2];
        };
        check(call("sceCtrl", 0x6A2774F3, {0}) == 0, "Default controller cycle");
        check(call("sceCtrl", 0x6A2774F3, {6000}) == 0, "Controller cycle setup");
        check(call("sceCtrl", 0x6A2774F3, {1}) >= 0x80000000u, "Invalid controller cycle accepted");
        check(call("sceCtrl", 0x6A2774F3, {0}) == 6000, "Invalid cycle changed state");
        check(call("sceCtrl", 0x1F4011E6, {1}) == 0, "Analog controller setup");
        check(call("sceCtrl", 0x1F4011E6, {2}) >= 0x80000000u, "Invalid sampling mode accepted");
        check(call("sceCtrl", 0x1F803938, {data, 0}) >= 0x80000000u, "Zero controller count accepted");
        check(call("sceCtrl", 0x1F803938, {data, 65}) >= 0x80000000u, "Large controller count accepted");
        check(call("sceCtrl", 0x1F803938, {0x07000000, 1}) >= 0x80000000u, "Unmapped controller buffer accepted");
        mhp2g::kernel().start_loader_thread(ctx, 0x08810000u, 0);
        ctx.gpr[31] = 0x08810004u;
        check(call("sceCtrl", 0x1F803938, {data, 1}) == 1, "Controller read did not complete at vblank");
        check(mhp2g::kernel().now_us() > 0 && rt.memory().load32(data + 4) == 0 && rt.memory().load8(data + 8) == 128,
            "Neutral pad or vblank timing failed");
        for (unsigned i = 10; i < 16; ++i)
            check(rt.memory().load8(data + i) == 0, "PSP controller reserved bytes not zero");
        call("sceCtrl", 0x6A2774F3, {6000});
        const auto before = mhp2g::kernel().now_us();
        check(call("sceCtrl", 0x1F803938, {data, 1}) == 1 && mhp2g::kernel().now_us() >= before + 6000,
            "Timed controller read failed");
        check(call("sceRtc", 0xE7C27D1B, {data}) == 0 && rt.memory().load16(data) >= 2020, "RTC output invalid");
        check(call("sceRtc", 0xE7C27D1B, {0x07000000}) >= 0x80000000u, "Invalid RTC output accepted");
        check(call("sceOpenPSID", 0xC69BEBCE, {data}) == 0 && rt.memory().load8(data) == 0x10, "Console ID missing");
        check(call("sceUtility", 0xA5DA2406, {9, data}) == 0 && rt.memory().load32(data) == 0,
            "Confirm-button parameter missing");
        check(call("sceUtility", 0xA5DA2406, {999, data}) >= 0x80000000u, "Unknown parameter accepted");
        check(call("sceWlanDrv", 0xD7763699) == 0, "Wireless switch should be off");
        check(call("sceWlanDrv", 0x0C622081, {data}) == 0 && rt.memory().load8(data) == 2, "Synthetic MAC missing");
        check(call("sceWlanDrv", 0x0C622081, {0x07000000}) >= 0x80000000u, "Bad MAC output accepted");
        check(call("scePower", 0x04B7766E, {0, 999}) >= 0x80000000u, "Unknown power callback accepted");
        check(call("LoadExecForUser", 0x4AC57943, {999}) >= 0x80000000u, "Unknown exit callback accepted");
        mhp2g::write_cstring(rt.memory(), data, "callback", 32);
        const auto callback = call("ThreadManForUser", 0xE81CAF8F, {data, 0x08811000u, 0});
        check(call("LoadExecForUser", 0x4AC57943, {callback}) == 0, "Exit callback registration failed");
        check(call("scePower", 0x04B7766E, {0, callback}) == 0 && mhp2g::kernel().callbacks.at(callback).pending,
            "Power state notification missing");
        std::array<char, 256> prx{};
        prx[0] = '~';
        prx[1] = 'P';
        prx[2] = 'S';
        prx[3] = 'P';
        const std::string module_name = "sceFont_Library";
        std::copy(module_name.begin(), module_name.end(), prx.begin() + 10);
        {
            std::ofstream module(folder / "stock.prx", std::ios::binary);
            module.write(prx.data(), prx.size());
        }
        mhp2g::write_cstring(rt.memory(), data, "ms0:/stock.prx", 64);
        const auto module_fd = call("IoFileMgrForUser", 0x109F50BC, {data, 1, 0});
        const auto module = call("ModuleMgrForUser", 0xB7F46618, {module_fd});
        check(module < 0x80000000u, "Supported stock module rejected");
        check(call("ModuleMgrForUser", 0x50F0C1EC, {module, 0, 0, data + 128}) == module &&
                rt.memory().load32(data + 128) == 0,
            "Stock module start result missing");
        check(call("ModuleMgrForUser", 0x50F0C1EC, {module, 0, 0, 0x07000000}) >= 0x80000000u,
            "Bad module status output accepted");
        check(call("ModuleMgrForUser", 0xD1FF982A, {module, 0, 0, data + 128}) == 0, "Module stop failed");
        check(call("ModuleMgrForUser", 0x2E0911AA, {module}) == 0, "Module unload failed");
        check(call("ModuleMgrForUser", 0x50F0C1EC, {module}) >= 0x80000000u, "Unloaded module was started");
        call("IoFileMgrForUser", 0x810C4BC3, {module_fd});
        // Real async read and 64-bit result polling use the same fd/position as sync I/O.
        mhp2g::write_cstring(rt.memory(), data, "ms0:/fixture.bin", 64);
        const auto fd = call("IoFileMgrForUser", 0x109F50BC, {data, 1, 0});
        check(fd < 0x80000000u, "Cannot open fixture");
        check(call("IoFileMgrForUser", 0xA0B5A7C2, {fd, data + 128, 3}) == 0, "Async read rejected");
        check(call("IoFileMgrForUser", 0x3251EA56, {fd, data + 256}) == 0 && rt.memory().load32(data + 256) == 3 &&
                rt.memory().load8(data + 128) == 'a',
            "Async read result missing");
        check(call("IoFileMgrForUser", 0x3251EA56, {fd, data + 256}) >= 0x80000000u, "Consumed async result repeated");
        check(call("IoFileMgrForUser", 0x71B19E77, {fd, 0, 1, 0, 0}) == 0, "Async seek rejected");
        check(call("IoFileMgrForUser", 0x3251EA56, {fd, data + 256}) == 0 && rt.memory().load32(data + 256) == 1,
            "64-bit async seek result missing");
        check(call("IoFileMgrForUser", 0xA0B5A7C2, {999, data + 128, 3}) >= 0x80000000u, "Invalid async fd accepted");
        call("IoFileMgrForUser", 0x810C4BC3, {fd});
        const auto channel = call("sceAudio", 0x5EC81C55, {0xFFFFFFFFu, 64, 0});
        check(channel < 8, "Audio reserve failed");
        check(call("sceAudio", 0x5EC81C55, {channel, 64, 0}) >= 0x80000000u, "Busy audio reserve accepted");
        call("sceAudio", 0x13F592BC, {channel, 0x8000, 0x8000, data + 512});
        call("sceAudio", 0x13F592BC, {channel, 0x8000, 0x8000, data + 512});
        check(call("sceAudio", 0xB011922F, {999}) >= 0x80000000u, "Invalid audio channel accepted");
        call("sceAudio", 0xCB2E439E, {channel, 32});
        call("sceAudio", 0x95FD0C2D, {channel, 0x10});
        call("sceAudio", 0xB7E1D8E7, {channel, 0x1000, 0x1000});
        call("sceAudio", 0x6FC46853, {channel});
        call("sceSasCore", 0x42778A9F, {data, 32, 32, 0, 44100});
        call("sceSasCore", 0x99944089, {data, 0, data + 1024, 16, 0});
        call("sceSasCore", 0xAD84D37F, {data, 0, 0x1000});
        call("sceSasCore", 0x440CA7D8, {data, 0, 0x1000, 0x1000});
        call("sceSasCore", 0xCBCD4F79, {data, 0, 0, 0});
        call("sceSasCore", 0x76F01ACA, {data, 0});
        call("sceSasCore", 0xA3589D81, {data, data + 2048});
        call("sceSasCore", 0xA0CF2FA4, {data, 0});
        check(call("sceSasCore", 0x68A46B95, {data}) != 0, "SAS end flag missing");
        for (auto nid : {0x267A6DD2u, 0x33D4AB37u, 0xD5A229C9u, 0xF983B186u}) call("sceSasCore", nid, {data, 0});
        // Unknown title modules must not be acknowledged as a stock HLE library.
        ctx.gpr[4] = 999;
        rt.invoke_import("ModuleMgrForUser", 0xB7F46618, ctx);
        check(rt.stop_reason().find("Unsupported stock/module image") != std::string::npos,
            "Unknown module was acknowledged");
        std::cout << "MHP1 controller/platform/I/O/audio contracts passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
