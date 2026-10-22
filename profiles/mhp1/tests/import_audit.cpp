// Inspect all imports without running private guest code.
#include "boot_host.hpp"
#include "psprecomp/common.hpp"
#include <charconv>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
namespace {
std::uint32_t hex(std::string text) {
    if (text.starts_with("0x")) text.erase(0, 2);
    std::uint32_t value{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value, 16);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
        throw psprecomp::Error("Invalid import CSV hex value");
    return value;
}
}
int main(int argc, char **argv) {
    try {
        if (argc != 3) {
            std::cerr << "Usage: mhp1_import_audit report_imports.csv output.csv\n";
            return 2;
        }
        std::ifstream file(argv[1]);
        if (!file) throw psprecomp::Error("Cannot open imports CSV");
        std::string line;
        std::getline(file, line);
        std::vector<psprecomp::PspImport> imports;
        while (std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;
            std::istringstream row(line);
            std::string library, nid, name, address;
            if (!std::getline(row, library, ',') || !std::getline(row, nid, ',') || !std::getline(row, name, ',') ||
                !std::getline(row, address))
                throw psprecomp::Error("Invalid analyzer import CSV row");
            imports.push_back({library, hex(nid), hex(address)});
        }
        const auto output = std::filesystem::absolute(argv[2]);
        std::filesystem::create_directories(output.parent_path());
        psprecomp::Runtime runtime;
        mhp1::BootHost host(runtime, imports, 0x0892A100u, 0x09AF7F00u, {}, output.parent_path() / "audit-ms0");
        host.write_import_audit(output);
        std::cout << "Import audit: " << output.string() << "\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
