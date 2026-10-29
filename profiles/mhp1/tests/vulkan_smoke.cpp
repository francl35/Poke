// Public, bounded GPU smoke scene: no game executable, ISO or assets.
#include "video_backend.hpp"
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <set>
#include <thread>
namespace {
using namespace mh::gpu;
Vertex vertex(float x, float y, float z, std::uint32_t color, float u = 0, float v = 0) {
    Vertex out;
    out.position = {x, y, z, 1};
    out.color = color;
    out.texcoord = {u, v};
    return out;
}
}
int main(int argc, char **argv) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: mhp1_vulkan_smoke output-directory\n";
            return 2;
        }
        const auto folder = std::filesystem::absolute(argv[1]);
        std::filesystem::create_directories(folder);
        auto video = mhp1::make_video_backend(folder / "cache");
        psprecomp::GuestMemory memory;
        DrawCall clear;
        clear.primitive = PrimitiveType::Sprites;
        clear.through = true;
        clear.clear_mode = true;
        clear.clear_flags = 7;
        clear.has_vertex_color = true;
        clear.target = {0x04000000u, 512, 3, 0x04090000u, 512};
        clear.vertices = {vertex(0, 0, 65535, 0xFF201008u), vertex(480, 272, 65535, 0xFF201008u)};
        auto textured = clear;
        textured.clear_mode = false;
        textured.depth.write_enabled = false;
        textured.texture.enabled = true;
        textured.texture.address = 0x08850000u;
        textured.texture.width = 8;
        textured.texture.height = 8;
        textured.texture.buffer_width = 8;
        textured.texture.format = TextureFormat::Rgba8888;
        textured.texture.function = 3;
        textured.texture.alpha_from_texture = true;
        textured.vertices = {vertex(20, 20, 0, 0xFFFFFFFFu, 0, 0), vertex(148, 148, 0, 0xFFFFFFFFu, 8, 8)};
        for (unsigned y = 0; y < 8; ++y)
            for (unsigned x = 0; x < 8; ++x)
                memory.store32(0x08850000u + (y * 8 + x) * 4, ((x ^ y) & 1) ? 0xFF20FFFFu : 0xFFFF4020u);
        DrawCall triangle;
        triangle.primitive = PrimitiveType::Triangles;
        triangle.target = clear.target;
        triangle.has_vertex_color = true;
        triangle.depth.write_enabled = false;
        triangle.vertices = {vertex(-0.1f, -0.7f, 0.5f, 0xFF2020FFu), vertex(0.9f, -0.7f, 0.5f, 0xFF20FF20u),
            vertex(0.4f, 0.8f, 0.5f, 0xFFFF2020u)};
        for (unsigned i = 0; i < 16; ++i)
            triangle.world[i] = triangle.view[i] = triangle.projection[i] = (i % 5 == 0) ? 1.0f : 0.0f;
        triangle.viewport.x_scale = 240;
        triangle.viewport.y_scale = -136;
        triangle.viewport.z_scale = 32767;
        triangle.viewport.x_offset = 240;
        triangle.viewport.y_offset = 136;
        triangle.viewport.z_offset = 32767;
        for (unsigned frame = 0; frame < 90 && video->poll(); ++frame) {
            video->begin_list();
            video->submit(clear, memory);
            video->submit(textured, memory);
            video->submit(triangle, memory);
            video->present(clear.target.color_address, 512u, 3u, memory);
            if (frame == 2 && !video->capture(folder / "vulkan-smoke.bmp"))
                throw std::runtime_error("GPU framebuffer capture failed");
            std::this_thread::sleep_for(std::chrono::milliseconds(33));
        }
        std::ifstream file(folder / "vulkan-smoke.bmp", std::ios::binary);
        std::string pixels{std::istreambuf_iterator<char>(file), {}};
        if (pixels.size() < 54 || pixels.substr(0, 2) != "BM")
            throw std::runtime_error("No Vulkan smoke capture was produced");
        auto word = [&](std::size_t at, unsigned n) {
            std::uint32_t value = 0;
            for (unsigned i = 0; i < n; ++i)
                value |= static_cast<std::uint32_t>(static_cast<std::uint8_t>(pixels[at + i])) << (i * 8u);
            return value;
        };
        const auto offset = word(10, 4), bpp = word(28, 2);
        if (offset >= pixels.size() || (bpp != 24u && bpp != 32u))
            throw std::runtime_error("Unsupported smoke BMP layout");
        std::set<std::array<std::uint8_t, 3>> colors;
        for (std::size_t at = offset; at + 2 < pixels.size(); at += bpp / 8u)
            colors.insert({static_cast<std::uint8_t>(pixels[at]), static_cast<std::uint8_t>(pixels[at + 1]),
                static_cast<std::uint8_t>(pixels[at + 2])});
        if (colors.size() < 16u)
            throw std::runtime_error("GPU smoke did not produce the expected textured/gradient scene");
        video->present(0, 0, 0, memory); // Exercise host-side display blanking without touching guest VRAM.
        std::cout << "Vulkan smoke completed; inspect " << (folder / "vulkan-smoke.bmp").string()
                  << " for checkerboard and colored triangle\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
