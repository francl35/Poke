#include "software_video.hpp"
#include "psprecomp/common.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <vector>

namespace mhp1 {
namespace {
using namespace mhp2g::gpu;
std::uint32_t bytes_per_pixel(std::uint32_t format) {
    if (format > 3u) throw psprecomp::Error("Unsupported framebuffer format");
    return format == 3u ? 4u : 2u;
}
std::uint32_t unpack(std::uint32_t p, std::uint32_t format) {
    auto expand = [](std::uint32_t v, unsigned bits) { return (v * 255u) / ((1u << bits) - 1u); };
    if (format == 3u) return p;
    if (format == 0u)
        return expand(p & 31u, 5) | (expand((p >> 5) & 63u, 6) << 8) | (expand((p >> 11) & 31u, 5) << 16) | 0xFF000000u;
    if (format == 1u)
        return expand(p & 31u, 5) | (expand((p >> 5) & 31u, 5) << 8) | (expand((p >> 10) & 31u, 5) << 16) |
            ((p & 0x8000u) ? 0xFF000000u : 0u);
    return (p & 15u) * 17u | (((p >> 4) & 15u) * 17u << 8) | (((p >> 8) & 15u) * 17u << 16) |
        (((p >> 12) & 15u) * 17u << 24);
}
std::uint32_t pack(std::uint32_t c, std::uint32_t format) {
    if (format == 3u) return c;
    const auto r = c & 255u, g = (c >> 8) & 255u, b = (c >> 16) & 255u, a = c >> 24;
    if (format == 0u) return (r >> 3) | ((g >> 2) << 5) | ((b >> 3) << 11);
    if (format == 1u) return (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10) | ((a >> 7) << 15);
    return (r >> 4) | ((g >> 4) << 4) | ((b >> 4) << 8) | ((a >> 4) << 12);
}
void validate_buffer(
    const psprecomp::GuestMemory &memory, std::uint32_t address, std::uint32_t stride, std::uint32_t bpp) {
    if (stride < 480u || stride > 2048u || !address || (address & (bpp - 1u)) ||
        !memory.contains(address, ((271u * stride) + 480u) * bpp))
        throw psprecomp::Error("Unsupported or unmapped 480x272 framebuffer");
}
}

bool retain_video_register(std::uint32_t cmd, std::uint32_t value) {
    // Register IDs from PSPSDK guInternal.h; values stay in GeState's raw bank.
    // They are interpreted (or rejected) by the reference rasterizer per draw.
    if (cmd == 0x15u || cmd == 0x16u || cmd == 0x1Cu || cmd == 0x20u || (cmd >= 0x24u && cmd <= 0x28u) ||
        (cmd >= 0x2Cu && cmd <= 0x33u) || (cmd >= 0x36u && cmd <= 0x38u) || cmd == 0x50u ||
        (cmd >= 0xA1u && cmd <= 0xA7u) || (cmd >= 0xA9u && cmd <= 0xAFu) || (cmd >= 0xB9u && cmd <= 0xC1u) ||
        cmd == 0xC8u || cmd == 0xCAu || cmd == 0xCCu || cmd == 0xD0u || (cmd >= 0xD8u && cmd <= 0xDAu) ||
        cmd == 0xDCu || cmd == 0xDDu || (cmd >= 0xE2u && cmd <= 0xE6u) || cmd == 0xE8u || cmd == 0xE9u)
        return true;
    // Only the zero reset writes observed in the legacy initialization range.
    // Nonzero writes remain unsupported, rather than inferring their meaning.
    return cmd >= 0xF0u && cmd <= 0xF9u && value == 0u;
}

void rasterize_2d(
    psprecomp::GuestMemory &memory, const mhp2g::gpu::DrawCall &d, const std::array<std::uint32_t, 256> &r) {
    using psprecomp::Error;
    if (!d.through || d.texture.enabled || d.raw_vertices || !d.indices.empty() ||
        (d.primitive != PrimitiveType::Sprites && d.primitive != PrimitiveType::Triangles))
        throw Error("Reference renderer supports only untextured, nonindexed through-mode sprites/triangles");
    if (!d.clear_mode &&
        (d.blend.enabled || d.depth.test_enabled || d.depth.write_enabled || d.alpha_test.enabled ||
            d.culling_enabled || d.lighting_enabled || d.fog.enabled || r[0x20] || r[0x24] || r[0x25] || r[0x27] ||
            r[0x28]))
        throw Error("Reference renderer does not support the active fragment/depth pipeline");
    if (d.clear_mode && d.primitive != PrimitiveType::Sprites) throw Error("Reference renderer clear requires sprites");
    const auto bpp = bytes_per_pixel(d.target.color_format);
    validate_buffer(memory, d.target.color_address, d.target.color_stride, bpp);
    if (d.clear_mode && (d.clear_flags & 4u))
        validate_buffer(memory, d.target.depth_address, d.target.depth_stride, 2u);
    const auto group = d.primitive == PrimitiveType::Sprites ? 2u : 3u;
    if (d.vertices.size() > 8192u) throw Error("Reference renderer vertex budget exceeded");
    if (d.vertices.size() % group) throw Error("Incomplete reference primitive");
    // Validate every primitive before mutating guest memory.
    for (std::size_t i = 0; i < d.vertices.size(); i += group) {
        for (unsigned v = 0; v < group; ++v) {
            const auto &p = d.vertices[i + v].position;
            if (!std::isfinite(p[0]) || !std::isfinite(p[1]) || !std::isfinite(p[2]) || std::abs(p[0]) > 65536.0f ||
                std::abs(p[1]) > 65536.0f || p[2] < 0 || p[2] > 65535)
                throw Error("Invalid reference vertex coordinate");
            if (!d.clear_mode && d.has_vertex_color && group == 3u && d.vertices[i + v].color != d.vertices[i].color)
                throw Error("Reference triangles require a constant vertex color");
        }
    }
    const auto bounds = [&](std::size_t i) {
        auto min_x = d.vertices[i].position[0], max_x = min_x;
        auto min_y = d.vertices[i].position[1], max_y = min_y;
        for (unsigned v = 1; v < group; ++v) {
            const auto &p = d.vertices[i + v].position;
            min_x = std::min(min_x, p[0]);
            max_x = std::max(max_x, p[0]);
            min_y = std::min(min_y, p[1]);
            max_y = std::max(max_y, p[1]);
        }
        return std::array<unsigned, 4>{static_cast<unsigned>(std::clamp(std::floor(min_x), 0.0f, 480.0f)),
            static_cast<unsigned>(std::clamp(std::floor(min_y), 0.0f, 272.0f)),
            static_cast<unsigned>(std::clamp(std::ceil(max_x), 0.0f, 480.0f)),
            static_cast<unsigned>(std::clamp(std::ceil(max_y), 0.0f, 272.0f))};
    };
    std::uint64_t pixels = 0;
    for (std::size_t i = 0; i < d.vertices.size(); i += group) {
        const auto box = bounds(i);
        pixels += static_cast<std::uint64_t>(box[2] - box[0]) * (box[3] - box[1]);
        if (pixels > 16'000'000u) throw Error("Reference renderer pixel budget exceeded");
    }
    const auto edge = [](const Vertex &a, const Vertex &b, float x, float y) {
        return (b.position[0] - a.position[0]) * (y - a.position[1]) -
            (b.position[1] - a.position[1]) * (x - a.position[0]);
    };
    for (std::size_t i = 0; i < d.vertices.size(); i += group) {
        const auto &a = d.vertices[i], &b = d.vertices[i + 1];
        const auto &last = d.vertices[i + group - 1u];
        const auto color = d.has_vertex_color ? last.color : d.material_color;
        const auto box = bounds(i);
        for (unsigned y = box[1]; y < box[3]; ++y)
            for (unsigned x = box[0]; x < box[2]; ++x) {
                const float px = x + 0.5f, py = y + 0.5f;
                bool inside = false;
                if (group == 2u) {
                    inside = px >= std::min(a.position[0], b.position[0]) &&
                        px < std::max(a.position[0], b.position[0]) && py >= std::min(a.position[1], b.position[1]) &&
                        py < std::max(a.position[1], b.position[1]);
                } else {
                    const auto &c = d.vertices[i + 2];
                    const auto area = edge(a, b, c.position[0], c.position[1]);
                    const auto e0 = edge(a, b, px, py), e1 = edge(b, c, px, py), e2 = edge(c, a, px, py);
                    inside = area != 0 && ((e0 >= 0 && e1 >= 0 && e2 >= 0) || (e0 <= 0 && e1 <= 0 && e2 <= 0));
                }
                if (!inside) continue;
                if (!d.clear_mode &&
                    (x < d.viewport.scissor_x1 || x > d.viewport.scissor_x2 || y < d.viewport.scissor_y1 ||
                        y > d.viewport.scissor_y2 || x < (r[0x15] & 1023u) || y < ((r[0x15] >> 10) & 1023u) ||
                        x > (r[0x16] & 1023u) || y > ((r[0x16] >> 10) & 1023u)))
                    continue;
                const auto address = d.target.color_address + (y * d.target.color_stride + x) * bpp;
                const auto old = bpp == 4u ? memory.load32(address) : memory.load16(address);
                auto mask = 0xFFFFFFFFu;
                if (d.clear_mode) {
                    mask = ((d.clear_flags & 1u) ? 0x00FFFFFFu : 0u) | ((d.clear_flags & 2u) ? 0xFF000000u : 0u);
                    if (d.target.color_format == 0u) mask |= 0xFF000000u;
                } else
                    mask = ~(r[0xE8] | ((r[0xE9] & 255u) << 24));
                const auto result =
                    pack((unpack(old, d.target.color_format) & ~mask) | (color & mask), d.target.color_format);
                if (bpp == 4u)
                    memory.store32(address, result);
                else
                    memory.store16(address, static_cast<std::uint16_t>(result));
                if (d.clear_mode && (d.clear_flags & 4u))
                    memory.store16(d.target.depth_address + (y * d.target.depth_stride + x) * 2u,
                        static_cast<std::uint16_t>(last.position[2]));
            }
    }
}

void capture_framebuffer(const psprecomp::GuestMemory &memory, std::uint32_t address, std::uint32_t stride,
    std::uint32_t format, const std::filesystem::path &path) {
    const auto bpp = bytes_per_pixel(format);
    validate_buffer(memory, address, stride, bpp);
    std::vector<std::uint8_t> bmp(54u + 480u * 272u * 3u);
    const auto put32 = [&](unsigned at, std::uint32_t value) {
        for (unsigned i = 0; i < 4; ++i) bmp[at + i] = static_cast<std::uint8_t>(value >> (i * 8u));
    };
    bmp[0] = 'B';
    bmp[1] = 'M';
    put32(2, static_cast<std::uint32_t>(bmp.size()));
    put32(10, 54);
    put32(14, 40);
    put32(18, 480);
    put32(22, 272);
    bmp[26] = 1;
    bmp[28] = 24;
    put32(34, 480u * 272u * 3u);
    for (unsigned y = 0; y < 272; ++y)
        for (unsigned x = 0; x < 480; ++x) {
            const auto at = address + (y * stride + x) * bpp;
            const auto c = unpack(bpp == 4u ? memory.load32(at) : memory.load16(at), format);
            const auto out = 54u + ((271u - y) * 480u + x) * 3u;
            bmp[out] = static_cast<std::uint8_t>(c >> 16);
            bmp[out + 1] = static_cast<std::uint8_t>(c >> 8);
            bmp[out + 2] = static_cast<std::uint8_t>(c);
        }
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char *>(bmp.data()), static_cast<std::streamsize>(bmp.size()));
    if (!file) throw psprecomp::Error("Cannot write framebuffer capture");
}
}
