# Quest HUD display size

Video > HUD size changes the displayed size of the recognized normal MHP2G
quest HUD. The choice applies during play and is saved immediately.

| Choice | Display factor | settings.ini value |
| --- | --- | --- |
| Original | 1 (100%) | `video.hud_size=original` |
| Medium | 1/2 (50%) | `video.hud_size=medium` |
| Small | 1/3 (33.33%) | `video.hud_size=small` |

Original is the default, including for existing configurations without this key.
Restore video defaults returns it to Original. No environment option is needed.

## What changes

- Clock, health, stamina, sharpness, and player-name plate share the original
  top-left anchor at PSP coordinates (8, 8).
- Recognized expanded/collapsed maps share a transform with their markers,
  anchored at the original map's right/top edges.
- The normal item selector, its buttons, icons, and associated labels share
  the bottom-right anchor at PSP coordinates (472, 260).
- Scissor rectangles follow the same transform to keep clipped elements aligned.
- Aspect Fill's existing fit is composed with the size transform, preserving
  the current on-screen corner margins.

Source artwork, texture resolution, replacement matching, MMPX, crisp-text
reconstruction, and internal resolution are not changed. The vertex shader
keeps the original coordinates used for sampling and changes only the final
screen position. This preserves the native sample-grid treatment of HUD sheets.
Menus, centered messages, full-screen transitions, and 3D draws are excluded
from the first-pass recognition rules.

## Scope and recognition

The existing Hide HUD classifier contains MHP3rd addresses and is not reused
or weakened by this change. The new helper recognizes the normal MHP2G quest
clock's screen/UV layout, then groups the associated draw patterns recorded
in the user's quest/menu trace. It uses neither hardcoded dynamic display-list
addresses nor the content hash of one map.

Recognition is conservative, but it is still based on observed layout patterns,
not a newly verified guest call-stack classifier. Nonstandard HUD layouts,
ultrawide game-code modifications, other weapons' additional gauges, extended
item panels, village HUDs, and world-space name labels are not guaranteed to
scale. Unrecognized draws retain their original size. The supported map surface
is the observed 256x256 swizzled CLUT8 stride-160 VRAM layout. Further cases
should be added from traces rather than by scaling every UI draw in a corner.

## Build and check

Use your existing configured Linux/macOS build tree. For example:

```bash
cmake --build out/mhp2g-renderer \
  --target Poke mhp2g_hud_layout_tests mhp2g_settings_tests --parallel 4
./out/mhp2g-renderer/bin/mhp2g_hud_layout_tests
./out/mhp2g-renderer/bin/mhp2g_settings_tests
./out/mhp2g-renderer/bin/Poke
```

No overlay or game-code regeneration is needed. The normal Poke build compiles
the changed vertex shader, so a working glslang executable remains required.

Check all three sizes in a quest, with both collapsed and expanded maps, and
with the Start menu open. Verify bar fills against frames, sharpness against
its fill, map-marker positions, player-name glyphs, and item-button alignment.
Test aspect Original and Fill, native and 4x internal resolution, with and
without texture replacements, and restart once to verify the saved choice.
Try another weapon and scene before considering broader HUD coverage verified.
Original can be selected immediately if a layout does not match correctly.

## Validation before delivery

- Standalone HUD tests passed: anchor invariance, long-bar grouping, map/marker
  grouping, item controls, frame/target reset, menu/dialog exclusions, invalid
  coordinates, identity scaling, and composition with aspect fit.
- Standalone settings tests passed, including size parsing and round trips.
- Recorded-trace replay selected 17 top-left, 3 map, and 17 item draws in the
  sampled quest frame. In the sampled Start-menu frame, only the 17 persistent
  top-left HUD draws were selected; menu panels/text remained unselected.
- Full Vulkan/shader compilation and gameplay testing are not available in this
  workspace and remain to be completed on the target machines.
