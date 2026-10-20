#pragma once
// Resident process memory only; not a separate GPU/texture-memory total.
#include <cstdint>
#include <limits>
#if defined(__APPLE__)
#include <mach/mach.h>
#elif defined(__linux__)
#include <fstream>
#include <unistd.h>
#endif
namespace mhp2g::perf::resource_detail {
#if defined(__linux__)
inline bool parse_statm(std::istream &in, long page_size, std::uint64_t &bytes) {
    bytes = 0u;
    std::uint64_t virtual_pages{}, resident_pages{};
    if (page_size <= 0 || !(in >> virtual_pages >> resident_pages)) return false;
    const auto page = static_cast<std::uint64_t>(page_size);
    if (resident_pages > std::numeric_limits<std::uint64_t>::max() / page) return false;
    bytes = resident_pages * page;
    return true;
}
#endif
inline bool process_rss(std::uint64_t &bytes) {
    bytes = 0u;
#if defined(__APPLE__)
    mach_task_basic_info_data_t info{};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
            reinterpret_cast<task_info_t>(&info), &count) != KERN_SUCCESS)
        return false;
    bytes = static_cast<std::uint64_t>(info.resident_size);
    return true;
#elif defined(__linux__)
    // Fast kernel RSS estimate, in pages; deliberately avoid scanning smaps.
    std::ifstream in("/proc/self/statm");
    return parse_statm(in, sysconf(_SC_PAGESIZE), bytes);
#else
    return false; // Explicit N/A on platforms not implemented here.
#endif
}
} // namespace mhp2g::perf::resource_detail
