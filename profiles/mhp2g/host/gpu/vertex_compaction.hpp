#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <span>
#include <string_view>
#include <vector>

namespace mhp2g::gpu::vertex_compaction {
inline bool enabled() {
    static const bool on=[] {
        const char *v=std::getenv("MHP2G_COMPACT_DIRECT_VERTICES");
        return !v || std::string_view(v)=="1";
    }();
    return on;
}
inline bool trace_enabled() {
    static const bool on=[] {
        const char *v=std::getenv("MHP2G_VERTEX_UPLOAD_TRACE");
        return v && std::string_view(v)=="1";
    }();
    return on;
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
    std::uint64_t compacted_draws{}, observed_draws{};
    void note(bool direct, bool raw, std::size_t supplied, std::size_t uploaded,
              std::size_t expanded, std::size_t raw_stride, std::size_t decoded_stride,
              bool compacted) {
        ++observed_draws;
        if(raw)raw_bytes+=supplied*raw_stride;
        else if(direct) {
            source_direct_bytes+=supplied*decoded_stride;
            uploaded_direct_bytes+=uploaded*decoded_stride;
            compacted_draws+=compacted;
        } else expanded_bytes+=expanded*decoded_stride;
    }
};
} // namespace mhp2g::gpu::vertex_compaction
