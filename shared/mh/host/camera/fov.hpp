#pragma once

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace mh::camera::fov {
using Matrix = std::array<float, 16>; // column-major, as supplied by the GE
inline constexpr std::array<std::int32_t, 10> kOffsets{-15, -10, -5, 0, 5, 10, 15, 20, 25, 30};
inline constexpr float kRadiansToDegrees = 57.29577951308232f;
inline constexpr float kDegreesToRadians = 0.017453292519943295f;

inline bool parse_offset(std::string_view text, std::int32_t &out) {
    if (text.empty()) return false;
    std::int32_t value{};
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
        std::find(kOffsets.begin(), kOffsets.end(), value) == kOffsets.end()) return false;
    out = value;
    return true;
}

struct Adjustment {
    bool valid{};
    float scale{1.0f}, center_x{}, center_y{};
    float native_degrees{}, adjusted_degrees{};
};

inline Adjustment analyze(const Matrix &p, std::int32_t offset) {
    Adjustment result;
    if (std::find(kOffsets.begin(), kOffsets.end(), offset) == kOffsets.end()) return result;
    for (float value : p) if (!std::isfinite(value)) return result;
    // A perspective matrix has clip-w proportional to eye-z. Orthographic
    // matrices, 2D projections, shear/rotation, and unknown layouts stay native.
    constexpr float epsilon = 0.00001f;
    if (std::abs(p[3]) > epsilon || std::abs(p[7]) > epsilon || std::abs(p[15]) > epsilon ||
        std::abs(p[11]) < epsilon || std::abs(p[0]) < epsilon || std::abs(p[5]) < epsilon ||
        std::abs(p[1]) > epsilon || std::abs(p[4]) > epsilon ||
        std::abs(p[12]) > epsilon || std::abs(p[13]) > epsilon) return result;
    const float half_tangent = std::abs(p[11] / p[5]);
    const float native = 2.0f * std::atan(half_tangent) * kRadiansToDegrees;
    if (!std::isfinite(native) || native < 5.0f || native > 120.0f) return result;
    const float adjusted = std::clamp(native + static_cast<float>(offset), 5.0f, 120.0f);
    result.valid = true;
    result.native_degrees = native;
    result.adjusted_degrees = offset == 0 ? native : adjusted;
    result.center_x = p[8] / p[11];
    result.center_y = p[9] / p[11];
    if (offset != 0)
        result.scale = half_tangent / std::tan(0.5f * adjusted * kDegreesToRadians);
    return result;
}

inline Matrix apply_adjustment(const Matrix &transform, const Adjustment &a) {
    if (!a.valid || a.scale == 1.0f) return transform;
    Matrix out = transform;
    // Scale clip-space x/y about the projection's own center. For off-center
    // projections the clip-w correction preserves the principal point. Depth
    // and clip-w are copied exactly, so depth testing and fog are unchanged.
    for (std::size_t column = 0; column < 4u; ++column) {
        const std::size_t at = column * 4u;
        out[at] = a.scale * transform[at] + (1.0f - a.scale) * a.center_x * transform[at + 3u];
        out[at + 1u] = a.scale * transform[at + 1u] + (1.0f - a.scale) * a.center_y * transform[at + 3u];
    }
    return out;
}

class Cache {
public:
    Matrix apply(const Matrix &transform, const Matrix &projection, std::int32_t offset) {
        if (offset == 0) return transform; // bit-identical Original, without trigonometry
        if (!known_ || offset != offset_ || projection != projection_) {
            projection_ = projection;
            offset_ = offset;
            adjustment_ = analyze(projection, offset);
            known_ = true;
        }
        return apply_adjustment(transform, adjustment_);
    }
private:
    Matrix projection_{};
    std::int32_t offset_{};
    Adjustment adjustment_{};
    bool known_{};
};
} // namespace mhp2g::camera::fov
