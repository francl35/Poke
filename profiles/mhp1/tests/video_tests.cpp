#include "software_video.hpp"
#include "psprecomp/common.hpp"
#include <bit>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace {
void check(bool value, const char *what) {
    if (!value) throw std::runtime_error(what);
}
template <class F> void rejects(F f) {
    bool rejected = false;
    try {
        f();
    } catch (const psprecomp::Error &) {
        rejected = true;
    }
    check(rejected, "Unsupported draw was accepted");
}
using namespace mhp2g::gpu;
constexpr std::uint32_t fb = 0x04000000u;
constexpr std::uint32_t zb = 0x04090000u;
Vertex vertex(float x, float y, std::uint32_t color) {
    Vertex v;
    v.position = {x, y, 123, 1};
    v.color = color;
    return v;
}
DrawCall sprite(float x, float y, float width, float height, std::uint32_t color) {
    DrawCall d;
    d.through = true;
    d.has_vertex_color = true;
    d.primitive = PrimitiveType::Sprites;
    d.target = {fb, 512, 3, zb, 512};
    d.depth.write_enabled = false;
    d.vertices = {vertex(x, y, color), vertex(x + width, y + height, color)};
    return d;
}
void test_video(const std::filesystem::path &capture) {
    psprecomp::GuestMemory memory;
    std::array<std::uint32_t, 256> registers{};
    registers[0x16] = 479u | (271u << 10);
    auto draw = sprite(0, 0, 480, 272, 0xFF201008u);
    draw.clear_mode = true;
    draw.clear_flags = 7;
    mhp1::rasterize_2d(memory, draw, registers);
    check(memory.load32(fb) == 0xFF201008u && memory.load32(fb + (271 * 512 + 479) * 4) == 0xFF201008u,
        "Clear color missing");
    check(memory.load16(zb) == 123 && memory.load16(zb + (271 * 512 + 479) * 2) == 123, "Clear depth missing");
    draw = sprite(20, 20, 100, 80, 0xFF2020FFu);
    mhp1::rasterize_2d(memory, draw, registers);
    check(memory.load32(fb + (20 * 512 + 20) * 4) == 0xFF2020FFu, "Sprite missing");
    check(memory.load32(fb + (100 * 512 + 120) * 4) == 0xFF201008u, "Sprite covered exclusive edge");
    auto triangle = draw;
    triangle.primitive = PrimitiveType::Triangles;
    triangle.vertices = {vertex(180, 30, 0xFF20FF20u), vertex(300, 30, 0xFF20FF20u), vertex(240, 150, 0xFF20FF20u)};
    mhp1::rasterize_2d(memory, triangle, registers);
    check(memory.load32(fb + (60 * 512 + 240) * 4) == 0xFF20FF20u, "Triangle missing");
    std::swap(triangle.vertices[0], triangle.vertices[2]);
    mhp1::rasterize_2d(memory, triangle, registers);
    check(memory.load32(fb + (60 * 512 + 240) * 4) == 0xFF20FF20u, "Triangle winding changed coverage");
    triangle.vertices[2].color = 0xFFFFFFFFu;
    rejects([&] { mhp1::rasterize_2d(memory, triangle, registers); });
    auto clipped = sprite(-10, -10, 30, 30, 0xFFFFFF00u);
    clipped.viewport.scissor_x1 = 5;
    clipped.viewport.scissor_y1 = 5;
    mhp1::rasterize_2d(memory, clipped, registers);
    check(memory.load32(fb) == 0xFF201008u && memory.load32(fb + (5 * 512 + 5) * 4) == 0xFFFFFF00u,
        "Scissor clipping failed");
    registers[0x15] = 10u | (10u << 10);
    clipped = sprite(5, 5, 20, 20, 0xFFFF00FFu);
    mhp1::rasterize_2d(memory, clipped, registers);
    check(
        memory.load32(fb + (5 * 512 + 5) * 4) == 0xFFFFFF00u && memory.load32(fb + (10 * 512 + 10) * 4) == 0xFFFF00FFu,
        "Region clipping failed");
    registers[0x15] = 0;
    for (std::uint32_t format = 0; format < 4; ++format) {
        auto pixel = sprite(0, 0, 1, 1, 0xFFFFFFFFu);
        pixel.target.color_format = format;
        mhp1::rasterize_2d(memory, pixel, registers);
        const auto value = format == 3u ? memory.load32(fb) : memory.load16(fb);
        check(value == (format == 3u ? 0xFFFFFFFFu : 0xFFFFu), "Framebuffer format packing failed");
        if (!capture.empty()) {
            auto p = capture.parent_path() / ("format_" + std::to_string(format) + ".bmp");
            mhp1::capture_framebuffer(memory, fb, 512, format, p);
            std::ifstream file(p, std::ios::binary);
            std::string bytes{std::istreambuf_iterator<char>(file), {}};
            check(bytes.size() == 54u + 480u * 272u * 3u && bytes.substr(0, 2) == "BM", "Invalid capture header");
            const auto at = 54u + 271u * 480u * 3u;
            check(static_cast<unsigned char>(bytes[at]) == 255u && static_cast<unsigned char>(bytes[at + 2]) == 255u,
                "Capture pixel conversion failed");
        }
    }
    draw = sprite(0, 0, 1, 1, 0x11223344u);
    memory.store32(fb, 0xAABBCCDDu);
    registers[0xE8] = 0x0000FFu;
    registers[0xE9] = 255u;
    mhp1::rasterize_2d(memory, draw, registers);
    check(memory.load32(fb) == 0xAA2233DDu, "Channel write masks failed");
    registers[0xE8] = registers[0xE9] = 0;
    for (unsigned case_id = 0; case_id < 10; ++case_id) {
        auto bad = draw;
        switch (case_id) {
        case 0:
            bad.through = false;
            break;
        case 1:
            bad.texture.enabled = true;
            break;
        case 2:
            bad.blend.enabled = true;
            break;
        case 3:
            bad.depth.write_enabled = true;
            break;
        case 4:
            bad.vertices[0].position[0] = std::numeric_limits<float>::quiet_NaN();
            break;
        case 5:
            bad.target.color_address = 0x07000000u;
            break;
        case 6:
            bad.target.color_format = 4;
            break;
        case 7:
            bad.vertices.pop_back();
            break;
        case 8:
            bad.target.color_stride = 1;
            break;
        case 9:
            bad.indices = {0, 1};
            break;
        }
        const auto before = memory.load32(fb);
        rejects([&] { mhp1::rasterize_2d(memory, bad, registers); });
        check(memory.load32(fb) == before, "Rejected draw changed framebuffer");
    }
    auto oversized = draw;
    oversized.vertices.resize(8193, vertex(0, 0, 0));
    rejects([&] { mhp1::rasterize_2d(memory, oversized, registers); });
    oversized = sprite(0, 0, 480, 272, 0xFFFFFFFFu);
    const auto first = oversized.vertices;
    for (unsigned i = 0; i < 128; ++i) oversized.vertices.insert(oversized.vertices.end(), first.begin(), first.end());
    rejects([&] { mhp1::rasterize_2d(memory, oversized, registers); });
    rejects([&] { mhp1::capture_framebuffer(memory, 0, 512, 3, "unused.bmp"); });
    rejects([&] { mhp1::capture_framebuffer(memory, fb, 512, 4, "unused.bmp"); });
    // Restore an easily inspected synthetic reference image.
    draw = sprite(0, 0, 480, 272, 0xFF201008u);
    draw.clear_mode = true;
    draw.clear_flags = 3;
    mhp1::rasterize_2d(memory, draw, registers);
    mhp1::rasterize_2d(memory, sprite(20, 20, 100, 80, 0xFF2020FFu), registers);
    triangle.vertices = {vertex(180, 30, 0xFF20FF20u), vertex(300, 30, 0xFF20FF20u), vertex(240, 150, 0xFF20FF20u)};
    mhp1::rasterize_2d(memory, triangle, registers);
    if (!capture.empty()) mhp1::capture_framebuffer(memory, fb, 512, 3, capture);
}
void test_decoder_extension() {
    psprecomp::GuestMemory memory;
    constexpr auto pc = 0x08830000u;
    memory.store32(pc, 0x15000001u);
    memory.store32(pc + 4, 0x0C000000u);
    bool done = false;
    GeState original;
    original.execute(memory, pc, 0, done);
    check(done && original.unhandled_command_count() == 1, "Default decoder behavior changed");
    GeState extended;
    extended.set_state_command_sink(mhp1::retain_video_register);
    extended.execute(memory, pc, 0, done);
    check(done && extended.unhandled_command_count() == 0 && extended.command_registers()[0x15] == 1,
        "Extended register was not retained");
    memory.store32(pc, 0xF0000001u);
    extended.execute(memory, pc, 0, done);
    check(extended.unhandled_command_count() == 1, "Unknown nonzero legacy command accepted");
    memory.store32(pc, 0x03000000u);
    extended.execute(memory, pc, 0, done);
    check(extended.unhandled_command_count() == 2, "Unknown opcode accepted");
    memory.store32(pc, 0x05000000u);
    extended.execute(memory, pc, 0, done);
    check(extended.unhandled_command_count() == 3, "Unsupported geometry bypassed extension");
    for (unsigned cmd = 0xF0; cmd <= 0xF9; ++cmd) check(mhp1::retain_video_register(cmd, 0), "Legacy reset rejected");
    check(!mhp1::retain_video_register(0x08, 0), "Control flow claimed as register state");
}
}
int main(int argc, char **argv) {
    try {
        if (argc != 1 && !(argc == 3 && std::string_view(argv[1]) == "--capture"))
            throw std::runtime_error("Usage: mhp1_video_tests [--capture output.bmp]");
        const auto temp = std::filesystem::temp_directory_path() /
            ("mhp1-video-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        struct Cleanup {
            std::filesystem::path path;
            ~Cleanup() {
                std::error_code e;
                std::filesystem::remove_all(path, e);
            }
        } cleanup{temp};
        test_decoder_extension();
        test_video(argc == 3 ? std::filesystem::path(argv[2]) : temp / "reference.bmp");
        std::cout << "MHP1 reference video tests passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
