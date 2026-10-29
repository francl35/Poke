#pragma once
#include "process_memory.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
namespace mh::perf {
// Same thread as the renderer/performance overlay. Session peaks/counters
// survive restart_measurement(), which only excludes pauses from timings.
struct ResourceUsage {
    bool renderer_known{}, rss_valid{}, interpolating{};
    std::uint64_t rss_bytes{}, rss_peak_bytes{};
    std::uint64_t vertex_bytes{}, vertex_capacity{}, vertex_peak{};
    std::uint64_t index_bytes{}, index_capacity{}, index_peak{};
    std::uint64_t pack_bytes{}, vertex_failures{}, index_failures{};
};
namespace resource_detail {
struct ResourceState {
    ResourceUsage usage;
    bool sampled{};
    std::chrono::steady_clock::time_point last_sample{};
};
inline ResourceState &resource_state() { static ResourceState state; return state; }
}
inline void note_resource_peak(std::uint64_t vertices, std::uint64_t indices) {
    auto &u = resource_detail::resource_state().usage;
    u.vertex_peak = std::max(u.vertex_peak, vertices);
    u.index_peak = std::max(u.index_peak, indices);
}
inline void note_resource_usage(std::uint64_t vertices, std::uint64_t indices,
    std::uint64_t vertex_capacity, std::uint64_t index_capacity,
    std::uint64_t pack_bytes, bool interpolating) {
    auto &u = resource_detail::resource_state().usage;
    u.renderer_known = true;
    u.vertex_bytes = vertices; u.index_bytes = indices;
    u.vertex_capacity = vertex_capacity; u.index_capacity = index_capacity;
    u.pack_bytes = pack_bytes; u.interpolating = interpolating;
    note_resource_peak(vertices, indices);
}
inline void note_resource_failure(const char *arena) {
    auto &u = resource_detail::resource_state().usage;
    if (std::strcmp(arena, "vertex") == 0) ++u.vertex_failures;
    else if (std::strcmp(arena, "index") == 0) ++u.index_failures;
}
inline const ResourceUsage &resource_usage() {
    auto &s = resource_detail::resource_state();
    const auto now = std::chrono::steady_clock::now();
    if (!s.sampled || now - s.last_sample >= std::chrono::seconds(1)) {
        s.sampled = true; s.last_sample = now;
        s.usage.rss_valid = resource_detail::process_rss(s.usage.rss_bytes);
        if (s.usage.rss_valid)
            s.usage.rss_peak_bytes = std::max(s.usage.rss_peak_bytes, s.usage.rss_bytes);
    }
    return s.usage;
}
inline double memory_mib(std::uint64_t bytes) { return static_cast<double>(bytes) / (1024.0 * 1024.0); }
} // namespace mhp2g::perf
