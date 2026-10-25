#include "boot_host.hpp"
#include "main_native.hpp"
#include "app_paths.hpp"
#include "target.hpp"

#include "kernel/iso_image.hpp"
#include "psprecomp/common.hpp"
#include "psprecomp/sha256.hpp"

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <iostream>
#include <string_view>
#include <vector>

namespace {
unsigned parse_seconds(std::string_view text) {
    unsigned result{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
    if (parsed.ec != std::errc{} ||
        parsed.ptr != text.data() + text.size())
        throw psprecomp::Error(
            "Seconds must be a non-negative integer; 0 means unlimited");
    return result;
}

void verify_disc(const std::filesystem::path &path) {
    mhp2g::IsoImage disc(path);
    const auto entry = disc.find("PSP_GAME/SYSDIR/EBOOT.BIN");
    if (!entry || entry->directory || entry->size > 16u * 1024u * 1024u)
        throw psprecomp::Error("Disc has no supported EBOOT.BIN");
    std::vector<std::uint8_t> bytes(entry->size);
    if (disc.read(static_cast<std::uint64_t>(entry->lba) * mhp2g::IsoImage::kSectorSize, bytes) != bytes.size())
        throw psprecomp::Error("Short read of disc EBOOT.BIN");
    mhp1::require_executable_hash(psprecomp::sha256_bytes(bytes));
}
} // namespace

int main(int argc, char **argv) {
    // Keep startup/error information available even if a local run is interrupted.
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;
    try {
        if (argc != 4 && argc != 5) {
            std::cerr << "Usage: PokeFreedomDXBoot EBOOT.ELF patched.iso diagnostic-ms0 [seconds=20]\n";
            return 2;
        }
        const unsigned seconds = argc == 5 ? parse_seconds(argv[4]) : 20u;
        const auto elf = psprecomp::Elf32Image::from_file(argv[1]);
        mhp1::require_executable_hash(psprecomp::sha256_bytes(elf.bytes()));
        if (elf.runtime_entry() != mhp1::kEntry || elf.required_ram_size() != mhp1::kRamBytes)
            throw psprecomp::Error("Unexpected MHP1 entry address or RAM requirement");
        verify_disc(argv[2]);
        const auto memory_stick = std::filesystem::absolute(argv[3]).lexically_normal();
        if (std::filesystem::exists(memory_stick) && !std::filesystem::is_directory(memory_stick))
            throw psprecomp::Error("Memory-stick path is not a directory");
        std::filesystem::create_directories(memory_stick);
        psprecomp::Runtime runtime(mhp1::kRamBytes);
        const auto relocations = elf.load_and_relocate(runtime.memory());
        if (relocations.invalid || relocations.unsupported)
            throw psprecomp::Error("Invalid or unsupported relocations");
        const auto module = elf.find_module_info(runtime.memory());
        if (!module || module->name != "MonsterHunterPSP" || module->gp != mhp1::kGp)
            throw psprecomp::Error("Unexpected PSP module identity");
        std::uint32_t image_end = 0u;
        for (std::size_t i = 0; i < elf.segments().size(); ++i) {
            const auto &segment = elf.segments()[i];
            if (segment.type != 1u) continue;
            image_end = std::max(image_end, elf.segment_runtime_address(i) + segment.memory_size);
        }
        const auto imports = elf.scan_imports(runtime.memory(), *module);
        std::cout << "[mhp1-boot] target=FreedomDX entry=" << psprecomp::hex32(elf.runtime_entry())
                  << " gp=" << psprecomp::hex32(module->gp) << " image_end=" << psprecomp::hex32(image_end)
                  << "\n[mhp1-boot] memory_stick=" << memory_stick.string() << "\n";
        mhp1::prepare_main_native(runtime, elf, imports);
        mhp1::BootHost host(runtime, imports, module->gp, image_end, argv[2], memory_stick);
        const auto audit = mhp2g::executable_directory().parent_path() / "import-audit.csv";
        host.write_import_audit(audit);
        std::cout << "[mhp1-boot] import_audit=" << audit.string() << "\n";
        host.run(elf.runtime_entry(), seconds);
        // Reaching a diagnostic boundary is expected. The log describes the
        // actual boundary; exit 4 must never be presented as a successful boot.
        return 4;
    } catch (const std::exception &error) {
        std::cerr << "[mhp1-boot] FAIL: " << error.what() << "\n";
        return 1;
    }
}
