// Corpus/link and executable-layout probe. This intentionally executes no guest code.
#include "target.hpp"

#include "psprecomp/elf32.hpp"
#include "psprecomp/runtime.hpp"
#include "psprecomp/sha256.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <vector>

int main(int argc, char **argv) {
    try {
        if (argc != 2 && argc != 3) {
            std::cerr << "Usage: PokeFreedomDXProbe EBOOT.ELF [CONFIG.BIN]\n";
            return 2;
        }
        const auto elf = psprecomp::Elf32Image::from_file(argv[1]);
        // Validate the same bytes that will be loaded, avoiding a second file read.
        const auto hash = psprecomp::sha256_bytes(elf.bytes());
        mhp1::require_executable_hash(hash);
        if (elf.runtime_entry() != mhp1::kEntry || elf.required_ram_size() != mhp1::kRamBytes)
            throw psprecomp::Error("Executable entry or RAM requirement differs from the analyzed target");
        psprecomp::Runtime runtime(mhp1::kRamBytes);
        const auto relocations = elf.load_and_relocate(runtime.memory());
        if (relocations.invalid || relocations.unsupported)
            throw psprecomp::Error("Invalid or unsupported ELF relocations");
        const auto module = elf.find_module_info(runtime.memory());
        if (!module || module->name != "MonsterHunterPSP" || module->gp != mhp1::kGp)
            throw psprecomp::Error("Unexpected PSP module identity");
        psprecomp::register_generated_functions(runtime);
        if (!runtime.has_function(mhp1::kEntry))
            throw psprecomp::Error("Generated corpus does not register the executable entry");
        std::cout << "[mhp1-probe] sha256=" << hash << "\n"
                  << "[mhp1-probe] module=" << module->name << " entry=" << psprecomp::hex32(elf.runtime_entry())
                  << " gp=" << psprecomp::hex32(module->gp) << " registered=" << runtime.function_count() << "\n";
        std::map<std::uint32_t, unsigned> families;
        for (const auto &section : elf.sections()) {
            if (section.type != 1u || !section.name.ends_with(".ovl")) continue;
            const auto expected = mhp1::overlay_base(section.name);
            if (!expected || section.address != expected || section.size != 0u)
                throw psprecomp::Error("Unexpected overlay section: " + section.name);
            ++families[expected];
        }
        if (families.size() != mhp1::kOverlayBases.size()) throw psprecomp::Error("Missing overlay section family");
        for (const auto &[base, count] : families)
            std::cout << "[mhp1-probe] overlay_base=" << psprecomp::hex32(base) << " section_names=" << count
                      << " (no embedded payload)\n";
        const auto imports = elf.scan_imports(runtime.memory(), *module);
        std::map<std::string, unsigned> libraries;
        for (const auto &import : imports) ++libraries[import.library];
        std::cout << "[mhp1-probe] imports=" << imports.size() << "\n";
        for (const auto &[name, count] : libraries)
            std::cout << "[mhp1-probe] import_library=" << name << " count=" << count << "\n";
        if (argc == 3) {
            std::ifstream input(argv[2], std::ios::binary);
            if (!input) throw psprecomp::Error("Cannot open configuration file");
            std::vector<std::uint8_t> bytes;
            // Bounded read: one extra byte distinguishes an oversized file.
            char byte{};
            while (bytes.size() < 49u && input.get(byte)) bytes.push_back(static_cast<std::uint8_t>(byte));
            const auto config = mhp1::inspect_config(bytes);
            std::cout << "[mhp1-probe] config_format=" << config.version
                      << " minimap_percent=" << config.minimap_percent << "\n";
        }
        std::cout << "[mhp1-probe] PASS: executable loaded and generated registry linked; "
                     "guest execution, HLE, renderer and overlay loading are not implemented\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "[mhp1-probe] FAIL: " << error.what() << "\n";
        return 1;
    }
}
