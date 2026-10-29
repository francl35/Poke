#pragma once
#include "imgui.h"

// Shared theme contract. A profile may select a palette through
// MHP_UI_THEME_HEADER without adding game-specific branches to widgets.
namespace mh::ui {
struct ThemePalette {
    ImU32 backdrop;
    ImU32 panel;
    ImU32 panelEdge;
    ImU32 row;
    ImU32 rowHover;
    ImU32 rowFocus;
    ImU32 accent;
    ImU32 accentBright;
    ImU32 text;
    ImU32 textDim;
    ImU32 textDisabled;
    ImU32 danger;
    ImU32 good;
    ImU32 track;
    ImU32 popup;
    ImU32 modalDim;
    ImU32 frame;
    ImU32 selection;
    ImU32 scrollGrab;
    ImU32 scrollHover;
    ImU32 separator;
};
inline constexpr ThemePalette kDefaultTheme{
    IM_COL32(12, 8, 5, 170), // Backdrop
    IM_COL32(36, 46, 56, 246), // Panel
    IM_COL32(105, 134, 154, 255), // PanelEdge
    IM_COL32(255, 240, 210, 10), // Row
    IM_COL32(162, 197, 217, 28), // RowHover
    IM_COL32(162, 197, 217, 64), // RowFocus
    IM_COL32(162, 197, 217, 255), // Accent
    IM_COL32(221, 238, 247, 255), // AccentBright
    IM_COL32(246, 220, 174, 255), // Text
    IM_COL32(170, 190, 204, 255), // TextDim
    IM_COL32(116, 135, 149, 255), // TextDisabled
    IM_COL32(214, 102, 76, 255), // Danger
    IM_COL32(150, 196, 120, 255), // Good
    IM_COL32(255, 240, 210, 36), // Track
    IM_COL32(32, 43, 55, 252), // Popup
    IM_COL32(7, 12, 18, 150), // ModalDim
    IM_COL32(221, 238, 247, 20), // Frame
    IM_COL32(162, 197, 217, 110), // Selection
    IM_COL32(105, 134, 154, 160), // ScrollGrab
    IM_COL32(162, 197, 217, 200), // ScrollHover
    IM_COL32(105, 134, 154, 120), // Separator
};
} // namespace mhp2g::ui
