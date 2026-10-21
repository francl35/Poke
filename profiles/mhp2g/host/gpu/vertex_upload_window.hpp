#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <limits>
#include <vector>
#include <string_view>

namespace mhp2g::gpu::vertex_upload {
inline bool trim_enabled() {
    static const bool on = [] {
        const char *v = std::getenv("MHP2G_TRIM_DIRECT_VERTICES");
        return v && std::string_view(v) == "1";
    }();
    return on;
}
inline bool compact_enabled() {
    static const bool on = [] {
        const char *v = std::getenv("MHP2G_COMPACT_DIRECT_VERTICES");
        // Default on; explicit 0 retains the original upload path for diagnosis.
        return v == nullptr || std::string_view(v) == "1";
    }();
    return on;
}
inline bool trace_enabled() {
    static const bool on = [] {
        const char *v = std::getenv("MHP2G_VERTEX_UPLOAD_TRACE");
        return v && std::string_view(v) == "1";
    }();
    return on;
}
struct Window { std::size_t first{}, count{}; };
// Indices are the renderer's already-expanded triangle list. Keep all interior
// vertices, including any unreferenced holes. Invalid input falls back unchanged.
inline Window referenced_window(std::span<const std::uint16_t> indices, std::size_t supplied) {
    if (indices.empty()) return {0u,supplied};
    const auto [lo,hi] = std::minmax_element(indices.begin(),indices.end());
    if (*hi >= supplied) return {0u,supplied};
    return {*lo,static_cast<std::size_t>(*hi)-*lo+1u};
}
inline void rebase(std::span<std::uint16_t> indices, Window window) {
    // window.first comes only from referenced_window and cannot exceed an index.
    if (!window.first) return;
    for (auto &index : indices) index=static_cast<std::uint16_t>(index-window.first);
}
// Build a dense first-use ordering without mutating the triangle indices.
// Scratch buffers are retained by the renderer. The index format bounds the map
// to 65536 entries, even if a malformed decoded span is larger.
inline bool referenced_vertices(std::span<const std::uint16_t> indices, std::size_t supplied,
    std::vector<std::uint32_t> &sources, std::vector<std::uint32_t> &mapping) {
    sources.clear();
    if (indices.empty()) return false;
    constexpr auto unused = std::numeric_limits<std::uint32_t>::max();
    mapping.assign(std::min<std::size_t>(supplied,65536u),unused);
    for (const auto index : indices) {
        if (index >= supplied) { sources.clear(); return false; }
        if (mapping[index] == unused) {
            mapping[index] = static_cast<std::uint32_t>(sources.size());
            sources.push_back(index);
        }
    }
    return true;
}
inline void dense_rebase(std::span<std::uint16_t> indices,
    const std::vector<std::uint32_t> &mapping) {
    for (auto &index : indices) index = static_cast<std::uint16_t>(mapping[index]);
}
struct Stats {
    std::uint64_t source_direct_bytes{}, uploaded_direct_bytes{}, expanded_bytes{}, raw_bytes{};
    std::uint64_t candidate_saving_bytes{}, trimmed_draws{}, observed_draws{};
    std::uint64_t dense_saving_bytes{}, dense_referenced_bytes{}, compacted_draws{};
    void note(bool direct, bool raw, std::size_t supplied, std::size_t uploaded,
              std::size_t possible, std::size_t expanded, std::size_t raw_stride,
              std::size_t decoded_stride,
              std::size_t dense_possible = std::numeric_limits<std::size_t>::max(),
              bool compacted = false) {
        ++observed_draws;
        if (raw) raw_bytes+=supplied*raw_stride;
        else if (direct) {
            source_direct_bytes+=supplied*decoded_stride;
            uploaded_direct_bytes+=uploaded*decoded_stride;
            candidate_saving_bytes+=(supplied-possible)*decoded_stride;
            trimmed_draws+=uploaded<supplied;
            if (dense_possible == std::numeric_limits<std::size_t>::max()) dense_possible = supplied;
            dense_referenced_bytes+=dense_possible*decoded_stride;
            dense_saving_bytes+=(supplied-dense_possible)*decoded_stride;
            compacted_draws+=compacted;
        } else expanded_bytes+=expanded*decoded_stride;
    }
};
} // namespace mhp2g::gpu::vertex_upload
