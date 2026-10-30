#include <SDL3/SDL.h>
#include "app_branding.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;
static constexpr const char *app_id = MH_APP_ID;
static int fail(const std::string &text) {
    std::cerr << text << '\n';
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, MH_APP_NAME, text.c_str(), nullptr);
    return 2;
}

int main(int argc, char **argv) {
    const char *xdg = std::getenv("XDG_DATA_HOME");
    if (!xdg || !*xdg) return fail("XDG_DATA_HOME is missing; start the application through Flatpak.");
    const fs::path data = fs::path(xdg)/MH_APP_DATA_SUBDIR;
    const fs::path elf = data/"game/EBOOT.ELF", iso = data/"game/disc.iso", ms0 = data/"ms0";
    if (argc == 2 && std::string(argv[1]) == "--print-paths") {
        std::cout << "data=" << data << "\nelf=" << elf << "\niso=" << iso
                  << "\nms0=" << ms0 << "\nrenderer-data=" << data/"renderer-data"
                  << "\noverlays=/app/bin/overlays\n";
        return 0;
    }
    if (argc != 1) return fail("Usage: " MH_APP_COMMAND " [--print-paths]");
    if (!fs::is_regular_file(elf) || !fs::is_regular_file(iso))
        return fail("Game data has not been imported. Run import_game.py from the packaging kit on the host.\n\nExpected folder:\n"+data.string()+"/game");
    try {
        fs::create_directories(ms0);
        fs::create_directories(data/"renderer-data");
        fs::create_directories(data/"logs");
        const auto settings = data/"renderer-data/settings.ini";
        if (!fs::exists(settings)) {
            std::ofstream f(settings);
            f << "settings.version=3\nvideo.gpu_decode=1\nvideo.frame_rate=30\n";
            if (!f) return fail("Cannot initialize settings in "+settings.string());
        }
        std::error_code ec;
        fs::rename(data/"logs/boot.log", data/"logs/boot.previous.log", ec);
        std::FILE *log = std::fopen((data/"logs/boot.log").c_str(), "w");
        if (!log) return fail("Cannot open the application log folder.");
        if (dup2(fileno(log), STDOUT_FILENO) < 0 || dup2(fileno(log), STDERR_FILENO) < 0)
            return fail("Cannot redirect the application log.");
        std::fclose(log);
        setenv("MHP1_NATIVE_MAIN", "1", 1);
        setenv("MHP1_NATIVE_OVERLAYS", "1", 1);
        setenv("MHP1_OVERLAY_DIR", "/app/bin/overlays", 1);
        setenv("MHP1_RENDERER", "1", 1);
        setenv("MHP1_PRESENT_ON_FLIP", "1", 1);
        // Font selection and timing come from the app's own saved settings.
        // Explicit flatpak --env overrides remain available for diagnostics.
        setenv("SDL_APP_ID", app_id, 0);
        const std::string a = elf.string(), b = iso.string(), c = ms0.string();
        char *const args[] = {const_cast<char *>("/app/bin/PokeFreedomDXBoot"),
            const_cast<char *>(a.c_str()), const_cast<char *>(b.c_str()),
            const_cast<char *>(c.c_str()), const_cast<char *>("0"), nullptr};
        execv(args[0], args);
        return fail("Could not start PokeFreedomDXBoot; see logs/boot.log in "+data.string());
    } catch (const std::exception &e) { return fail(e.what()); }
}
