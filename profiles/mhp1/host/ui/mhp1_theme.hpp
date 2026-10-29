#pragma once
#include "ui/theme_palette.hpp"

// Kokoto's brown panels, bronze borders and cream text. This palette belongs
// to the MHP1 profile; shared UI code and Complete keep their own selection.
namespace mh::ui {
inline constexpr ThemePalette kSelectedTheme{
    IM_COL32(15, 9, 5, 170), // Backdrop
    IM_COL32(47, 31, 21, 246), // Panel
    IM_COL32(151, 111, 65, 255), // PanelEdge
    IM_COL32(255, 235, 202, 10), // Row
    IM_COL32(214, 167, 102, 38), // RowHover
    IM_COL32(214, 167, 102, 78), // RowFocus
    IM_COL32(220, 174, 105, 255), // Accent
    IM_COL32(255, 226, 179, 255), // AccentBright
    IM_COL32(250, 235, 208, 255), // Text
    IM_COL32(205, 181, 147, 255), // TextDim
    IM_COL32(146, 125, 101, 255), // TextDisabled
    IM_COL32(235, 131, 108, 255), // Danger
    IM_COL32(166, 205, 131, 255), // Good
    IM_COL32(255, 235, 202, 36), // Track
    IM_COL32(40, 26, 18, 252), // Popup
    IM_COL32(12, 7, 3, 150), // ModalDim
    IM_COL32(255, 226, 179, 20), // Frame
    IM_COL32(220, 174, 105, 110), // Selection
    IM_COL32(151, 111, 65, 160), // ScrollGrab
    IM_COL32(214, 167, 102, 200), // ScrollHover
    IM_COL32(151, 111, 65, 120), // Separator
};
} // namespace mhp2g::ui
