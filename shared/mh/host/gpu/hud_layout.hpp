#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

// Conservative recognition of the normal MHP2G quest HUD from its submitted
// geometry. No guest code patches, MHP3rd addresses, or texture-pack hashes.
// Unknown layouts remain at their original size.
namespace mh::gpu::ui {

struct HudRect {
    float left{}, top{}, right{}, bottom{};
};
struct HudProbe {
    HudRect screen, uv;
    std::uint32_t texture_address{}, width{}, height{};
    bool textured{}, glyph_page{}, map_surface{};
    bool map_area_tint{}; // all visible vertices are yellow-tinted
};
enum class HudGroup { None, TopLeft, Map, Items };
struct HudAnchor {
    HudGroup group{HudGroup::None};
    float x{}, y{};
    explicit operator bool() const { return group != HudGroup::None; }
    float apply_x(float value, float factor) const { return x + (value - x) * factor; }
    float apply_y(float value, float factor) const { return y + (value - y) * factor; }
};

enum class HudProfile { Mhp2g, Mhp1 };

class HudLayout {
public:
    explicit HudLayout(HudProfile profile = HudProfile::Mhp2g) : profile_(profile) {}
    // Diagnostic gate: menus cannot consume the map capture budget before
    // the same clock signature used by HUD scaling has appeared.
    bool quest_hud_active(std::uint64_t frame, std::uint32_t target) const {
        return frame_ == frame && target_ == target && clock_;
    }
    HudAnchor classify(std::uint64_t frame, std::uint32_t target, const HudProbe &draw) {
        if (frame_ != frame || target_ != target) {
            frame_ = frame;
            target_ = target;
            clock_ = items_ = map_ = false;
            hud_texture_ = 0u;
            party_rows_ = 0u;
        }
        const bool mhp1 = profile_ == HudProfile::Mhp1;
        const HudRect clock_bounds = mhp1 ? HudRect{8, 6, 60, 54} : HudRect{8, 8, 60, 56};
        const float top = mhp1 ? 6.0f : 8.0f;
        const float right = mhp1 ? 346.0f : 344.0f;
        const HudRect &b = draw.screen;
        if (!valid(b) || !valid(draw.uv)) return {};
        // The normal quest clock: this combination distinguishes gameplay
        // from title screens, inventory icons, and full-screen transitions.
        if (draw.textured && draw.width == 256u && draw.height == 256u &&
            same(b, clock_bounds) && same(draw.uv, {0, 32, 52, 80})) {
            clock_ = true;
            hud_texture_ = draw.texture_address;
        }
        if (!clock_) return {};
        const bool hud_sheet = draw.textured && draw.texture_address == hud_texture_;
        if (inside(b, {8, top, right, 74})) {
            const bool bar_fill = !draw.textured && inside(b, {56, 12, 344, 29});
            const bool bar_strip = draw.textured && draw.width == 16u && draw.height == 8u &&
                inside(b, {56, 12, 344, 29});
            const bool name = draw.glyph_page && inside(b, {24, 56, 112, 74});
            const bool name_icon = draw.textured && draw.width == 256u && draw.height == 256u &&
                inside(b, {8, 56, 24, 72}) && (mhp1 ? same(b, {8, 56, 20, 72}) &&
       (same(draw.uv, {12, 192, 24, 208}) || same(draw.uv, {24, 192, 36, 208}))
                      : same(draw.uv, {48, 192, 60, 208}));
            // Weapon icons use a separate 256x256 sheet. These are the
            // traced weapon cells in the player-name slot.
            const bool weapon_icon = draw.textured && !draw.glyph_page &&
                draw.width == 256u && draw.height == 256u &&
                same(b, {10, 56, 22, 72}) &&
                (same(draw.uv, {208, 224, 220, 240}) ||
                 same(draw.uv, {232, 224, 244, 240}) ||
                 same(draw.uv, {220, 240, 232, 256}));
            // The longsword spirit gauge is below sharpness. Its two caps
            // come from the HUD sheet; its colour strip is a separate texture.
            // Exact screen/UV matches exclude the nearby menu-panel corners.
            const bool spirit_cap = hud_sheet &&
                (same(b, {62, 44, 66, 50}) || same(b, {162, 44, 166, 50})) &&
                same(draw.uv, {240, 112, 244, 118});
            const bool spirit_strip = draw.textured && !draw.glyph_page &&
                draw.width == 16u && draw.height == 8u &&
                same(b, {66, 44, 162, 50}) && same(draw.uv, {0, 0, 96, 6});
            // The main sheet also contains menu corners. Screen position
            // alone is insufficient: accept only the traced HUD UV regions.
            const bool clock_piece = hud_sheet && inside(b, clock_bounds) &&
                inside(draw.uv, {0, 32, 108, 80});
            const bool gauge_cap = hud_sheet && inside(b, {56, 9, right, 33}) &&
                inside(draw.uv, {52, 32, 76, 56});
            const bool sharpness = hud_sheet &&
                ((inside(b, {56, 30, 112, 48}) &&
                  inside(draw.uv, {108, 32, 152, 68})) ||
                 // Traced colour overlay extends one pixel above the base.
                 (same(b, {60, 29, 104, 41}) &&
                  same(draw.uv, {108, 32, 152, 44})));
            const bool name_plate = hud_sheet && inside(b, {8, 56, 96, 72}) &&
                inside(draw.uv, {120, 16, 156, 32});
            const bool status_icon = hud_sheet && inside(b, {56, 8, 344, 48}) &&
                inside(draw.uv, {184, 0, 256, 80}) &&
                b.right - b.left <= 24.0f && b.bottom - b.top <= 24.0f;
            const bool bowgun_notice = mhp1 && hud_sheet &&
                ((same(b, {60, 32, 116, 44}) &&
                  same(draw.uv, {152, 84, 208, 96})) ||
                 (same(b, {60, 32, 132, 44}) &&
                  same(draw.uv, {152, 72, 224, 84})));
            if (bowgun_notice || clock_piece || gauge_cap || sharpness || name_plate || status_icon ||
                bar_fill || bar_strip || name || name_icon || weapon_icon ||
                spirit_cap || spirit_strip)
                return {HudGroup::TopLeft, 8, top};
        }
        // Recognize additional name rows only after their HUD-sheet opener.
        // The companion trace uses y76, twenty pixels below the first row.
        // Later party slots may use the same spacing; unknown layouts stay put.
        for (unsigned row = 1; row < 4; ++row) {
            const float y = 56.0f + 20.0f * static_cast<float>(row);
            if (hud_sheet && same(b, {8, y, 24, y + 16}) &&
                same(draw.uv, {120, 16, 136, 32}))
                party_rows_ |= 1u << row;
            if ((party_rows_ & (1u << row)) == 0u) continue;
            const bool plate = hud_sheet &&
                ((same(b, {8, y, 24, y + 16}) && same(draw.uv, {120, 16, 136, 32})) ||
                 (same(b, {24, y, 88, y + 16}) && same(draw.uv, {136, 16, 148, 32})) ||
                 (same(b, {88, y, 96, y + 16}) && same(draw.uv, {148, 16, 156, 32})));
            const bool label = draw.glyph_page && inside(b, {24, y, 112, y + 18});
            const bool icon = draw.textured && !draw.glyph_page &&
                draw.width == 256u && draw.height == 256u &&
                same(b, {10, y, 22, y + 16}) &&
                (same(draw.uv, {208, 224, 220, 240}) ||
                 same(draw.uv, {232, 224, 244, 240}) ||
                 same(draw.uv, {220, 240, 232, 256}) ||
                 same(draw.uv, {232, 240, 244, 256}));
            if (plate || label || icon) return {HudGroup::TopLeft, 8, top};
        }
        // Item and ammo panels share the HUD-sheet frame and the same
        // bottom-right anchor. A bowgun's open selector starts much higher
        // than the melee item wheel. Infer the panel extent from its opener
        // instead of recognizing only the two short melee rectangles.
        const bool selector_left = std::abs(b.left - 312.0f) < 0.51f ||
                                   std::abs(b.left - 328.0f) < 0.51f;
        const bool selector_corner = hud_sheet && selector_left &&
            b.top >= 112.0f && b.top <= 216.0f &&
            std::abs((b.right - b.left) - 16.0f) < 0.51f &&
            std::abs((b.bottom - b.top) - 8.0f) < 0.51f &&
            same(draw.uv, {0, 208, 16, 216});
        // The idle ammunition label uses the name/banner atlas strip rather
        // than a framed-panel opener. Its location separates it from player
        // names and companion rows. Admit only banner pieces, not every draw
        // near the bottom right.
        const bool ammo_banner = hud_sheet &&
            (mhp1
                ? same(b, {336, 184, 348, 200}) &&
                  same(draw.uv, {156, 16, 168, 32})
                : inside(b, {312, 180, 472, 204}) &&
                  std::abs((b.bottom - b.top) - 16.0f) < 0.51f &&
                  inside(draw.uv, {120, 16, 156, 32}));
        if (selector_corner || ammo_banner) {
            // Eight pixels of trim admit the selection arrows and controller
            // prompts protruding beyond the frame. Each independent panel can
            // extend the bounds; an ammo banner must not shrink a tall panel.
            const HudRect bounds = selector_corner
                ? HudRect{b.left - 8.0f, b.top - 8.0f, 480, 272}
                : HudRect{304, b.top - 8.0f, 480, b.bottom + 8.0f};
            if (!items_) item_bounds_ = bounds;
            else {
                item_bounds_.left = std::min(item_bounds_.left, bounds.left);
                item_bounds_.top = std::min(item_bounds_.top, bounds.top);
                item_bounds_.right = std::max(item_bounds_.right, bounds.right);
                item_bounds_.bottom = std::max(item_bounds_.bottom, bounds.bottom);
            }
            items_ = true;
        }
        const bool selector_content = !draw.textured || hud_sheet || draw.glyph_page ||
            (draw.width == 16u && draw.height == 8u) ||
            (draw.width == 256u && draw.height == 256u);
        // An established selector owns its contents before map-marker
        // classification. The expanded ammo panel overlaps the map's original
        // coordinate range, but its icons must use the HUD size, not map size.
        if (items_ && inside(b, item_bounds_) && selector_content)
            return {HudGroup::Items, 472, 260};
        if (mhp1) {
            // Captured MHP1 local and expanded maps share one coordinate plane.
            const float screen_width = b.right - b.left;
            const float screen_height = b.bottom - b.top;
            const float uv_width = draw.uv.right - draw.uv.left;
            const float uv_height = draw.uv.bottom - draw.uv.top;

            const bool map_projection =
                draw.map_surface && !draw.glyph_page &&
                screen_width > 0.0f && screen_height > 0.0f &&
                inside(b, {324, 36, 476, 188}) &&
                inside(draw.uv, {0, 0, 152, 152}) &&
                std::abs((b.left - draw.uv.left) - 324.0f) < 0.51f &&
                std::abs((b.top - draw.uv.top) - 36.0f) < 0.51f &&
                std::abs(screen_width - uv_width) < 0.51f &&
                std::abs(screen_height - uv_height) <= 1.01f;

            if (map_projection) {
                map_ = true;
                // Include marker geometry extending beyond the map edges.
                map_bounds_ = {312, 24, 488, 200};
                map_anchor_ = {HudGroup::Map, 476, 36};
                return map_anchor_;
            }

            const bool marker_cell =
                same(draw.uv, {48, 244, 60, 256}) ||
                same(draw.uv, {24, 244, 36, 256});

            if (map_ && draw.textured && !draw.glyph_page && !hud_sheet &&
                draw.width == 256u && draw.height == 256u &&
                marker_cell && inside(b, map_bounds_))
                return map_anchor_;
        } else {
        // Captured detection halo: a textured yellow sprite from the same
        // atlas cell as the monster dot, drawn BEFORE the map surface. It
        // uses the expanded-map coordinate plane (anchor 468,32), even while
        // the cropped local map is shown. Do not borrow a previous-frame
        // cropped-map anchor, or wait for map_ to become true.
        if (draw.textured && !draw.glyph_page && !hud_sheet && draw.map_area_tint &&
            draw.width == 256u && draw.height == 256u &&
            same(draw.uv, {36, 244, 48, 256}) &&
            inside(b, {280, 16, 480, 200}) &&
            b.right - b.left >= 16.0f && b.right - b.left <= 32.0f &&
            b.bottom - b.top >= 16.0f && b.bottom - b.top <= 32.0f)
            return {HudGroup::Map, 468, 32};
        // A map's own right/top edges define its anchor; both expanded and
        // collapsed maps keep their original margins. Following marker draws
        // inherit that exact transform rather than shrinking about themselves.
        if (draw.map_surface && inside(b, {280, 16, 480, 200})) {
            map_ = true;
            map_bounds_ = b;
            map_anchor_ = {HudGroup::Map, b.right, b.top};
            return map_anchor_;
        }
        // Keep the four HUD-sheet dialog corners out of map-marker scaling.
        if (map_ && draw.textured && draw.width == 256u && draw.height == 256u &&
            !draw.glyph_page && !(hud_sheet &&
                (same(draw.uv, {0, 208, 16, 216}) ||
                 same(draw.uv, {32, 208, 48, 216}) ||
                 same(draw.uv, {0, 224, 16, 232}) ||
                 same(draw.uv, {32, 224, 48, 232}))) && inside(b, map_bounds_))
            return map_anchor_;
        }
        // Recognize only traced item-selector openers on the quest HUD sheet.
        // Also recognize the L-held variant (top at 200, bottom up to 264).
        // The wider x=312 forms were confirmed in FUComplete captures.
        // Keep per-layout bounds instead of widening every quest/menu draw.
        if (hud_sheet && same(draw.uv, {0, 208, 16, 216})) {
            if (same(b, {328, 208, 344, 216})) {
                items_ = true;
                item_bounds_ = {328, 200, 472, 260};
            } else if (same(b, {328, 200, 344, 208})) {
                items_ = true;
                item_bounds_ = {328, 200, 472, 264};
            } else if (same(b, {312, 208, 328, 216}) ||
                       same(b, {312, 200, 328, 208})) {
                items_ = true;
                item_bounds_ = {312, 200, 472, 264};
            }
        }
        if (items_ && inside(b, item_bounds_) &&
            (!draw.textured || hud_sheet || draw.glyph_page ||
                (draw.width == 16u && draw.height == 8u) ||
                (draw.width == 256u && draw.height == 256u)))
            return {HudGroup::Items, 472, 260};
        return {};
    }

private:
    static bool valid(const HudRect &r) {
        return std::isfinite(r.left) && std::isfinite(r.top) &&
            std::isfinite(r.right) && std::isfinite(r.bottom) &&
            r.left <= r.right && r.top <= r.bottom;
    }
    static bool same(const HudRect &a, const HudRect &b) {
        return std::abs(a.left - b.left) < 0.51f && std::abs(a.top - b.top) < 0.51f &&
            std::abs(a.right - b.right) < 0.51f && std::abs(a.bottom - b.bottom) < 0.51f;
    }
    static bool inside(const HudRect &a, const HudRect &b) {
        return a.left >= b.left - 0.01f && a.top >= b.top - 0.01f &&
            a.right <= b.right + 0.01f && a.bottom <= b.bottom + 0.01f;
    }
    HudProfile profile_{HudProfile::Mhp2g};
    std::uint64_t frame_{~0ull};
    std::uint32_t target_{}, hud_texture_{};
    bool clock_{}, items_{}, map_{};
    std::uint32_t party_rows_{};
    HudRect map_bounds_{}, item_bounds_{};
    HudAnchor map_anchor_{};
};

} // namespace mhp2g::gpu::ui
