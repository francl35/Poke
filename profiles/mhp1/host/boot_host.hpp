#pragma once

#include "psprecomp/elf32.hpp"
#include "psprecomp/runtime.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <span>

namespace mhp1 {

struct DisplayMode {
    bool configured{};
    std::uint32_t mode{};
    std::uint32_t width{};
    std::uint32_t height{};
};

// One bounded diagnostic session per process, matching the reused kernel's lifetime.
// Game code is interpreted; only PSP import bridges and kernel trampolines are native.
class BootHost {
public:
    BootHost(psprecomp::Runtime &runtime, std::span<const psprecomp::PspImport> imports, std::uint32_t gp,
        std::uint32_t image_end, const std::filesystem::path &disc, const std::filesystem::path &memory_stick);
    ~BootHost();
    BootHost(const BootHost &) = delete;
    BootHost &operator=(const BootHost &) = delete;

    void write_import_audit(const std::filesystem::path &path) const;
    void run(std::uint32_t entry, unsigned seconds, std::uint64_t max_dispatches = 2'000'000u);
    [[nodiscard]] std::size_t bound_imports() const noexcept { return bound_imports_; }
    [[nodiscard]] std::size_t import_calls() const noexcept { return import_calls_; }
    [[nodiscard]] std::uint32_t compiled_sdk_version() const noexcept { return compiled_sdk_version_; }
    [[nodiscard]] const DisplayMode &display_mode() const noexcept { return display_mode_; }
    [[nodiscard]] std::uint32_t edram_translation_width() const noexcept { return edram_translation_width_; }

private:
    static void import_bridge(psprecomp::Runtime &, psprecomp::AllegrexContext &);
    static bool missing_function(psprecomp::Runtime &, psprecomp::AllegrexContext &, std::uint32_t);
    static void pre_dispatch(psprecomp::Runtime &, psprecomp::AllegrexContext &, std::uint32_t, std::int32_t);
    void check_deadline();
    void invoke(psprecomp::AllegrexContext &);

    psprecomp::Runtime &runtime_;
    std::map<std::uint32_t, psprecomp::PspImport> imports_;
    std::map<std::uint32_t, std::uint64_t> calls_;
    std::map<std::uint32_t, bool> bound_;
    std::chrono::steady_clock::time_point deadline_{};
    bool running_{};
    bool ran_{};
    DisplayMode display_mode_;
    std::uint32_t edram_translation_width_{};
    std::uint32_t compiled_sdk_version_{};
    std::size_t bound_imports_{};
    std::size_t import_calls_{};
    static BootHost *active_;
};

} // namespace mhp1
