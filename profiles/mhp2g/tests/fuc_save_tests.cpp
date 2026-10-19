#include "save_data/fuc_save_keys.hpp"
#include "save_data/savedata_store.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace mhp2g::savedata;
namespace {
void require(bool condition, const char *message) { if (!condition) throw std::runtime_error(message); }
}
int main() {
    try {
        const auto key = effective_save_key("ULUS10391", "MHP2NDG.BIN", kFucPreviousKey);
        if (!reviewed_fuc_save_variant()) {
            require(key == kFucPreviousKey, "stock build key changed");
            require(!previous_fuc_save_key("ULUS10391", "MHP2NDG.BIN", &kFucUsKey), "stock fallback enabled");
            std::cout << "Stock save-key isolation checks passed\n";
            return 0;
        }
        require(key == kFucUsKey, "reviewed US save key not selected");
        require(effective_save_key("ULJM05500", "MHP2NDG.BIN", kFucPreviousKey) == kFucPreviousKey, "other region changed");
        require(effective_save_key("FUCDAT", "CONFIG.BIN", kFucPreviousKey) == kFucPreviousKey, "mod config changed");
        Block custom{}; custom[0] = 7;
        require(effective_save_key("ULUS10391", "MHP2NDG.BIN", custom) == custom, "custom key changed");
        const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto root = std::filesystem::temp_directory_path() / ("poke-fuc-save-test-" + std::to_string(nonce));
        struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code ec; std::filesystem::remove_all(path, ec); } } cleanup{root};
        SaveFiles regional{"ULUS10391", "", "MHP2NDG.BIN", key};
        SaveFiles previous = regional; previous.key = kFucPreviousKey;
        SaveContents contents; contents.data.resize(64);
        for (unsigned i = 0; i < contents.data.size(); ++i) contents.data[i] = static_cast<unsigned char>(i * 3 + 1);
        std::string error;
        require(write_save(root, regional, contents, error), "regional save write failed");
        auto loaded = load_save(root, regional);
        require(loaded.status == LoadStatus::Ok && loaded.contents.data == contents.data, "regional round trip failed");
        require(write_save(root, previous, contents, error), "previous-key fixture write failed");
        loaded = load_save(root, regional);
        require(loaded.status == LoadStatus::Ok && loaded.contents.data == contents.data, "authenticated legacy save failed");
        SaveFiles unknown = regional; unknown.key = custom;
        require(load_save(root, unknown).status == LoadStatus::Broken, "unknown-key save accepted");
        // A normal resave uses the regional key. No file is migrated on load.
        require(write_save(root, regional, contents, error), "regional resave failed");
        require(load_save(root, previous).status == LoadStatus::Broken, "resave still uses previous key");
        const auto file = save_folder(root, regional) / regional.file_name;
        {
            std::fstream stream(file, std::ios::binary | std::ios::in | std::ios::out);
            char byte{}; stream.seekg(20); stream.read(&byte, 1); byte ^= 1;
            stream.seekp(20); stream.write(&byte, 1);
        }
        require(load_save(root, regional).status == LoadStatus::Broken, "tampered file accepted");
        require(previous_fuc_save_key("ULJM05500", "MHP2NDG.BIN", &key) == std::nullopt, "other region fallback enabled");
        require(previous_fuc_save_key("ULUS10391", "OTHER.BIN", &key) == std::nullopt, "other file fallback enabled");
        std::cout << "FUComplete regional/legacy/tamper save checks passed\n";
        return 0;
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
