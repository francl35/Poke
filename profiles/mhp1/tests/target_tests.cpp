#include "target.hpp"

#include <iostream>
#include <stdexcept>

namespace {
void check(bool condition) {
    if (!condition) throw std::runtime_error("MHP1 target contract regression");
}
template <typename Function> void rejects(Function function) {
    bool rejected = false;
    try {
        function();
    } catch (const psprecomp::Error &) {
        rejected = true;
    }
    check(rejected);
}
}
int main() {
    try {
        mhp1::require_executable_hash(mhp1::kExecutableSha256);
        rejects([] { mhp1::require_executable_hash(""); });
        rejects(
            [] { mhp1::require_executable_hash("17b823aebe4810d4bf57fb5b4f601a33c67f51ecdec538fa653c786a41682abc"); });
        check(mhp1::overlay_base("game_task.ovl") == 0x098D4700u);
        check(mhp1::overlay_base("show_select_task.ovl") == 0x098D4700u);
        check(mhp1::overlay_base("demo_sub.ovl") == 0x09A5A180u);
        check(mhp1::overlay_base("game_sub.ovl") == 0x09A5A180u);
        check(mhp1::overlay_base("stage00.ovl") == 0x09AF6900u);
        check(mhp1::overlay_base("stage91.ovl") == 0x09AF6900u);
        for (auto name : {"em15.ovl", ".relgame_task.ovl", "stageXX.ovl", "stage1.ovl", "debug.elf"})
            check(mhp1::overlay_base(name) == 0u);
        std::array<std::uint8_t, 48> bytes{};
        const std::string version = "1.8.0";
        for (std::size_t i = 0; i < version.size(); ++i) bytes[i] = version[i];
        bytes[0x13] = 100;
        const auto config = mhp1::inspect_config(bytes);
        check(config.version == version && config.minimap_percent == 100u);
        bytes[0x13] = 75;
        check(mhp1::inspect_config(bytes).minimap_percent == 75u);
        rejects([&] { mhp1::inspect_config(std::span(bytes).first(47)); });
        std::array<std::uint8_t, 49> oversized{};
        rejects([&] { mhp1::inspect_config(oversized); });
        bytes[0] = 'x';
        rejects([&] { mhp1::inspect_config(bytes); });
        bytes[0] = '2';
        rejects([&] { mhp1::inspect_config(bytes); });
        std::cout << "MHP1 target contract tests passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
