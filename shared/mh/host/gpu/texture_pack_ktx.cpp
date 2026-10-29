#include "texture_pack_ktx.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <memory>

#if defined(MHP2G_HAS_KTX2) && MHP2G_HAS_KTX2
#include <ktx.h>
#endif

namespace mh::gpu {

bool is_ktx2_image(std::span<const std::uint8_t> bytes) {
    constexpr std::array<std::uint8_t, 12> magic{0xABu, 0x4Bu, 0x54u, 0x58u, 0x20u, 0x32u,
        0x30u, 0xBBu, 0x0Du, 0x0Au, 0x1Au, 0x0Au};
    return bytes.size() >= magic.size() && std::equal(magic.begin(), magic.end(), bytes.begin());
}

bool decode_ktx2_image(std::span<const std::uint8_t> bytes, KtxPackImage &out, std::string &error) {
    out = {};
    error.clear();
    if (!is_ktx2_image(bytes)) {
        error = "not a KTX2 image";
        return false;
    }
#if defined(MHP2G_HAS_KTX2) && MHP2G_HAS_KTX2
    constexpr std::uint32_t max_side = 8192u;
    constexpr std::uint64_t max_storage = 512ull * 1024u * 1024u;
    ktxTexture2 *raw = nullptr;
    const auto checked = [&](KTX_error_code code, const char *operation) {
        if (code == KTX_SUCCESS) return true;
        error = std::string(operation) + ": " + ktxErrorString(code);
        return false;
    };
    // Read metadata first, then reject oversized/unhandled layouts before
    // image allocation and decompression. Input memory stays alive throughout.
    if (!checked(ktxTexture2_CreateFromMemory(bytes.data(), bytes.size(), KTX_TEXTURE_CREATE_NO_FLAGS, &raw),
            "KTX2 header"))
        return false;
    const auto destroy = [](ktxTexture2 *p) { ktxTexture_Destroy(ktxTexture(p)); };
    const std::unique_ptr<ktxTexture2, decltype(destroy)> texture(raw, destroy);
    if (raw->numDimensions != 2u || raw->isArray || raw->numFaces != 1u || raw->numLayers != 1u ||
        raw->baseDepth != 1u || raw->baseWidth == 0u || raw->baseHeight == 0u ||
        raw->baseWidth > max_side || raw->baseHeight > max_side || raw->numLevels == 0u ||
        raw->numLevels > static_cast<std::uint32_t>(std::bit_width(std::max(raw->baseWidth, raw->baseHeight))) ||
        ktxTexture_GetDataSizeUncompressed(ktxTexture(raw)) > max_storage) {
        error = "unsupported KTX2 layout or size (requires a 2D image up to 8192x8192)";
        return false;
    }
    if (!checked(ktxTexture2_LoadImageData(raw, nullptr, 0u), "KTX2 image data")) return false;
    if (ktxTexture2_NeedsTranscoding(raw) &&
        !checked(ktxTexture2_TranscodeBasis(raw, KTX_TTF_RGBA32, 0u), "KTX2 Basis-to-RGBA8 transcode"))
        return false;

    // Vulkan's registry values, without making the CPU decoder depend on a
    // Vulkan SDK. Basis LDR transcoding produces RGBA8; raw RGBA/BGRA is also
    // accepted. HDR and other GPU block formats deliberately fall back.
    const bool bgra = raw->vkFormat == 44u || raw->vkFormat == 50u;
    if (!bgra && raw->vkFormat != 37u && raw->vkFormat != 43u) {
        error = "KTX2 format is not supported: expected Basis LDR or RGBA8/BGRA8";
        return false;
    }
    ktx_size_t offset = 0u;
    if (!checked(ktxTexture2_GetImageOffset(raw, 0u, 0u, 0u, &offset), "KTX2 base-level offset")) return false;
    const auto *data = ktxTexture_GetData(ktxTexture(raw));
    const ktx_size_t available = ktxTexture_GetDataSize(ktxTexture(raw));
    const ktx_size_t pitch = ktxTexture_GetRowPitch(ktxTexture(raw), 0u);
    const ktx_size_t row_bytes = static_cast<ktx_size_t>(raw->baseWidth) * 4u;
    const std::uint64_t span = std::uint64_t(raw->baseHeight - 1u) * pitch + row_bytes;
    if (data == nullptr || pitch < row_bytes || offset > available || span > available - offset) {
        error = "KTX2 decoded image is truncated";
        return false;
    }
    KtxPackImage image;
    image.width = raw->baseWidth;
    image.height = raw->baseHeight;
    image.pixels.resize(static_cast<std::size_t>(row_bytes) * image.height);
    const bool flip_x = raw->orientation.x == KTX_ORIENT_X_LEFT;
    const bool flip_y = raw->orientation.y == KTX_ORIENT_Y_UP;
    const bool premultiplied = ktxTexture2_GetPremultipliedAlpha(raw) != 0;
    for (std::uint32_t y = 0u; y < image.height; ++y) {
        const auto *row = data + offset + (flip_y ? image.height - 1u - y : y) * pitch;
        for (std::uint32_t x = 0u; x < image.width; ++x) {
            const auto *p = row + (flip_x ? image.width - 1u - x : x) * 4u;
            auto *q = image.pixels.data() + (static_cast<std::size_t>(y) * image.width + x) * 4u;
            q[0] = p[bgra ? 2u : 0u]; q[1] = p[1]; q[2] = p[bgra ? 0u : 2u]; q[3] = p[3];
            if (premultiplied) {
                for (unsigned c = 0; c < 3u; ++c)
                    q[c] = q[3] == 0u ? 0u : static_cast<std::uint8_t>(
                        std::min(255u, (unsigned(q[c]) * 255u + unsigned(q[3]) / 2u) / unsigned(q[3])));
            }
        }
    }
    out = std::move(image);
    return true;
#else
    error = "KTX2 decoding is disabled; configure with -DMHP2G_KTX2=bundled or system";
    return false;
#endif
}

} // namespace mhp2g::gpu
