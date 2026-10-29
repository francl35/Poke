#include "gpu/ui_textures.hpp"

#include "fonts/game_font.hpp"
#include "gpu/mmpx.hpp"
#include "gpu/texture_decode.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <iostream>

namespace mh::gpu::ui {
namespace {

// A page's texels in rows, one ink value (the palette index) per texel: the
// game keeps its pages swizzled, which decoding a copy undoes.
bool page_indices(const GuestMemory &memory, const TextureState &texture, std::vector<std::uint8_t> &indices) {
    TextureSnapshot snapshot;
    if (!snapshot_texture(memory, texture, snapshot)) return false;
    std::vector<std::uint32_t> colours;
    if (!decode_snapshot(snapshot, colours)) return false;
    const std::uint32_t width = texture.width, height = texture.height;
    if (snapshot.texels.size() < static_cast<std::size_t>(snapshot.row_bytes) * height) return false;
    indices.resize(static_cast<std::size_t>(width) * height);
    for (std::uint32_t y = 0; y < height; ++y)
        for (std::uint32_t x = 0; x < width; ++x) {
            const std::uint8_t byte = snapshot.texels[static_cast<std::size_t>(y) * snapshot.row_bytes + x / 2u];
            indices[static_cast<std::size_t>(y) * width + x] =
                (x & 1u) != 0u ? static_cast<std::uint8_t>(byte >> 4u) : static_cast<std::uint8_t>(byte & 0x0Fu);
        }
    return true;
}

// The page's sixteen colours, through the decoder itself: a 16x1 CLUT4
// picture of the indices 0..15 with the page's palette.
bool palette_of(const GuestMemory &memory, const TextureState &texture, std::array<std::uint32_t, 16> &palette) {
    TextureSnapshot snapshot;
    if (!snapshot_texture(memory, texture, snapshot)) return false;
    snapshot.texture.width = 16u;
    snapshot.texture.height = 1u;
    snapshot.texture.buffer_width = 16u;
    snapshot.texture.swizzled = false;
    snapshot.row_bytes = 8u;
    snapshot.texels = {0x10u, 0x32u, 0x54u, 0x76u, 0x98u, 0xBAu, 0xDCu, 0xFEu};
    std::vector<std::uint32_t> colours;
    if (!decode_snapshot(snapshot, colours) || colours.size() < 16u) return false;
    std::copy_n(colours.begin(), 16u, palette.begin());
    return true;
}


// Renderer/HLE access is serialized by the host, as with the existing font
// pass recorder. Store only bounded metadata, not an upscaled copy per page.
struct DirectPass {
    std::uint32_t code{};
    std::int32_t x64{}, y64{};
};
struct DirectPage {
    std::vector<std::uint8_t> base;
    std::vector<std::uint8_t> expected;
    std::vector<DirectPass> passes;
    std::uint64_t font_generation{};
    std::uint64_t revision{};
};
std::unordered_map<std::uint32_t, DirectPage> direct_pages;
std::uint64_t next_direct_revision{};
constexpr std::uint32_t kDirectSide = 256u;

bool read_direct_page(const GuestMemory &memory, std::uint32_t address, std::vector<std::uint8_t> &out) {
    if (!memory.contains(address, 32768u)) return false;
    out.resize(65536u);
    for (std::uint32_t i = 0; i < 32768u; ++i) {
        const auto b = memory.load8(address + i);
        out[i * 2u] = b & 15u;
        out[i * 2u + 1u] = b >> 4u;
    }
    return true;
}

void merge_direct(std::vector<std::uint8_t> &image, int side, const fonts::GlyphBitmap &glyph) {
    for (int y = 0; y < glyph.height; ++y) {
        const int py = glyph.y + y;
        if (py < 0 || py >= side) continue;
        for (int x = 0; x < glyph.width; ++x) {
            const int px = glyph.x + x;
            if (px < 0 || px >= side) continue;
            auto &pixel = image[static_cast<std::size_t>(py) * side + px];
            pixel = std::max(pixel, static_cast<std::uint8_t>(
                glyph.pixels[static_cast<std::size_t>(y) * glyph.width + x] >> 4u));
        }
    }
}

const DirectPage *direct_page(const TextureState &texture) {
    // Direct font writes are linear. Swizzled/copy-to-another-buffer paths
    // keep the original until explicitly supported and verified.
    if (texture.format != TextureFormat::Clut4 || texture.swizzled || texture.width != 256u ||
        texture.height != 256u || texture.buffer_width != 256u)
        return nullptr;
    const auto found = direct_pages.find(texture.address);
    return found == direct_pages.end() ? nullptr : &found->second;
}

bool direct_glyph_page(const GuestMemory &memory, const TextureState &texture, int scale,
    std::vector<std::uint32_t> &out, GlyphPageReport &report) {
    const DirectPage *page = direct_page(texture);
    if (page == nullptr || scale < 2 || scale > 4) return false;
    report.cells = static_cast<std::uint32_t>(page->passes.size());
    std::vector<std::uint8_t> current;
    // No stale reconstruction after the guest clears/reuses the atlas, or
    // after an untracked writer changes it. Never alter guest memory here.
    if (page->font_generation != fonts::generation() || !read_direct_page(memory, texture.address, current) ||
        current != page->expected) {
        report.mismatched = report.cells;
        return false;
    }
    std::array<std::uint32_t, 16> palette{};
    if (!palette_of(memory, texture, palette) || page->passes.empty()) return false;
    const int side = static_cast<int>(kDirectSide) * scale;
    std::vector<std::uint8_t> ink(static_cast<std::size_t>(side) * side);
    // Unknown content is preserved at native resolution. Known HLE glyph
    // passes are rasterized again from font outlines at the requested scale.
    for (int y = 0; y < side; ++y)
        for (int x = 0; x < side; ++x)
            ink[static_cast<std::size_t>(y) * side + x] =
                page->base[static_cast<std::size_t>(y / scale) * kDirectSide + x / scale];
    for (const DirectPass &pass : page->passes) {
        const auto glyph = fonts::render_at(pass.code, pass.x64, pass.y64, scale);
        merge_direct(ink, side, glyph);
        if (!glyph.pixels.empty()) ++report.redrawn;
    }
    if (report.redrawn == 0u) return false;
    out.resize(ink.size());
    for (std::size_t i = 0; i < ink.size(); ++i) out[i] = palette[ink[i]];
    return true;
}

} // namespace

void note_direct_glyph(const GuestMemory &memory, std::uint32_t image, std::uint32_t code) {
    if (!memory.contains(image, 24u) || memory.load32(image) != 0u ||
        memory.load16(image + 12u) != 256u || memory.load16(image + 14u) != 256u ||
        memory.load16(image + 16u) != 128u)
        return;
    const std::uint32_t address = memory.load32(image + 20u);
    if (address == 0u) return;
    std::vector<std::uint8_t> before;
    if (!read_direct_page(memory, address, before)) return;
    if (!direct_pages.contains(address) && direct_pages.size() >= 8u) direct_pages.clear();
    DirectPage &page = direct_pages[address];
    // A clear, a different writer, a font reload, or the metadata limit starts
    // a new recording from the actual guest pixels. No old passes survive.
    if (page.expected != before || page.font_generation != fonts::generation() || page.passes.size() >= 1024u) {
        page.base = before;
        page.expected = before;
        page.passes.clear();
        page.font_generation = fonts::generation();
    }
    DirectPass pass{code, static_cast<std::int32_t>(memory.load32(image + 4u)),
                         static_cast<std::int32_t>(memory.load32(image + 8u))};
    const auto native = fonts::render_at(pass.code, pass.x64, pass.y64, 1);
    if (native.pixels.empty()) return;
    page.passes.push_back(pass);
    merge_direct(page.expected, 256, native);
    page.revision = ++next_direct_revision;
    if (page.passes.size() == 1u && std::getenv("MHP2G_TRACE_UI") != nullptr)
        std::cout << "[ui] recording direct font page 0x" << std::hex << address << std::dec
                  << " (256x256 CLUT4, native pixels verified before reconstruction)\n";
}

std::uint64_t direct_glyph_revision(const TextureState &texture) {
    const auto *page = direct_page(texture);
    return page == nullptr ? 0u : page->revision;
}

bool is_glyph_page(const TextureState &texture) {
    if (const auto *page = direct_page(texture); page != nullptr && !page->passes.empty()) return true;
    const std::uint32_t atlas = fonts::game_atlas();
    if (atlas == 0u || texture.format != TextureFormat::Clut4 || texture.width != 256u || texture.height != 256u ||
        texture.buffer_width != 256u)
        return false;
    const std::uint32_t first = atlas + kAtlasPages;
    if (texture.address < first) return false;
    const std::uint32_t offset = texture.address - first;
    return offset % kAtlasPageBytes == 0u && offset / kAtlasPageBytes < kAtlasPageCount;
}

bool glyph_page(const GuestMemory &memory, const TextureState &texture, int scale, std::vector<std::uint32_t> &out,
    GlyphPageReport &report) {
    report = {};
    if (direct_page(texture) != nullptr)
        return direct_glyph_page(memory, texture, scale, out, report);
    if (!is_glyph_page(texture) || scale < 2) return false;
    const std::uint32_t atlas = fonts::game_atlas();
    const std::uint32_t page = (texture.address - atlas - kAtlasPages) / kAtlasPageBytes;
    std::array<std::uint32_t, 16> palette{};
    std::vector<std::uint8_t> indices;
    if (!memory.contains(atlas + kCodeToCell, kCodeToCellEntries * 2u) || !palette_of(memory, texture, palette) ||
        !page_indices(memory, texture, indices))
        return false;
    const auto index_at = [&](std::uint32_t x, std::uint32_t y) { return indices[y * 256u + x]; };

    // Which character is in each cell of this page.
    std::array<std::uint32_t, kCellsPerPage> codes{};
    codes.fill(~0u);
    for (std::uint32_t code = 0; code < kCodeToCellEntries; ++code) {
        const std::uint32_t cell = memory.load16(atlas + kCodeToCell + code * 2u);
        if (cell != 0xFFFFu && cell / kCellsPerPage == page) codes[cell % kCellsPerPage] = code;
    }

    const auto size = static_cast<std::uint32_t>(scale);
    const std::uint32_t side = 256u * size;
    out.assign(static_cast<std::size_t>(side) * side, 0u);
    // Everything as it is, texel for texel, then the known cells drawn again.
    for (std::uint32_t y = 0; y < 256u; ++y)
        for (std::uint32_t x = 0; x < 256u; ++x) {
            const std::uint32_t colour = palette[index_at(x, y)];
            for (std::uint32_t dy = 0; dy < size; ++dy)
                std::fill_n(out.begin() +
                        static_cast<std::ptrdiff_t>(
                            (static_cast<std::size_t>(y) * size + dy) * side + static_cast<std::size_t>(x) * size),
                    size, colour);
        }
    for (std::uint32_t cell = 0; cell < kCellsPerPage; ++cell) {
        if (codes[cell] == ~0u) continue;
        ++report.cells;
        const std::uint32_t left = cell % kCellsPerRow * kCellWidth;
        const std::uint32_t top = cell / kCellsPerRow * kCellPitch;
        // The cell must hold what the noted passes give at scale 1; one that
        // does not (drawn before the passes were noted, or by other code) keeps
        // the game's texels.
        const std::vector<std::uint8_t> check = fonts::glyph_cell(codes[cell], 1);
        if (check.empty()) continue;
        bool same = true;
        for (std::uint32_t y = 0; y < kCellWidth && same; ++y)
            for (std::uint32_t x = 0; x < kCellWidth && same; ++x)
                same = check[y * kCellWidth + x] == index_at(left + x, top + y);
        if (!same) {
            ++report.mismatched;
            continue;
        }
        const std::vector<std::uint8_t> ink = fonts::glyph_cell(codes[cell], scale);
        const std::uint32_t cell_side = kCellWidth * size;
        for (std::uint32_t y = 0; y < cell_side; ++y)
            for (std::uint32_t x = 0; x < cell_side; ++x)
                out[(static_cast<std::size_t>(top) * size + y) * side + static_cast<std::size_t>(left) * size + x] =
                    palette[ink[static_cast<std::size_t>(y) * cell_side + x]];
        ++report.redrawn;
    }
    // A page that is not what it was traced to be is left alone.
    return report.redrawn != 0u &&
       report.mismatched * 4u <= report.cells;
}

Upscaler::~Upscaler() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    wake_.notify_all();
    if (thread_.joinable()) thread_.join();
}

bool Upscaler::submit(
    std::uint64_t key, const GuestMemory &memory, const TextureState &texture, std::uint32_t doublings) {
    auto job = std::make_shared<Job>();
    job->key = key;
    job->doublings = doublings;
    if (!snapshot_texture(memory, texture, job->snapshot)) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    if (jobs_.count(key) != 0u) return true;
    // Copies made and never asked for again (a texture seen for a moment)
    // are dropped rather than kept.
    if (jobs_.size() >= 32u) std::erase_if(jobs_, [](const auto &entry) { return entry.second->done; });
    jobs_.emplace(key, job);
    queue_.push_back(std::move(job));
    if (!thread_.joinable()) thread_ = std::thread([this] { run(); });
    wake_.notify_one();
    return true;
}

bool Upscaler::pending(std::uint64_t key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return jobs_.count(key) != 0u;
}

std::size_t Upscaler::queued() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

bool Upscaler::take(std::uint64_t key, std::vector<std::uint32_t> &pixels, std::uint32_t &width, std::uint32_t &height,
    double &milliseconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = jobs_.find(key);
    if (found == jobs_.end() || !found->second->done) return false;
    pixels = std::move(found->second->pixels);
    width = found->second->width;
    height = found->second->height;
    milliseconds = found->second->milliseconds;
    jobs_.erase(found);
    return true;
}

void Upscaler::run() {
    for (;;) {
        std::shared_ptr<Job> job;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            wake_.wait(lock, [this] { return stop_ || !queue_.empty(); });
            if (stop_) return;
            job = std::move(queue_.front());
            queue_.pop_front();
        }
        const auto start = std::chrono::steady_clock::now();
        std::vector<std::uint32_t> image, doubled;
        std::uint32_t width = job->snapshot.texture.width, height = job->snapshot.texture.height;
        if (decode_snapshot(job->snapshot, image) && !image.empty()) {
            for (std::uint32_t i = 0; i < job->doublings; ++i) {
                mmpx_2x(image.data(), width, height, doubled);
                image.swap(doubled);
                width *= 2u;
                height *= 2u;
            }
        } else {
            image.clear();
        }
        const double milliseconds =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        std::lock_guard<std::mutex> lock(mutex_);
        job->pixels = std::move(image);
        job->width = width;
        job->height = height;
        job->milliseconds = milliseconds;
        job->done = true;
    }
}

} // namespace mhp2g::gpu::ui
