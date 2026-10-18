# Independent map and HUD sizes

Video > HUD size controls the recognized health/name and normal item groups.
Video > Map size controls the recognized quest map and its markers separately.
Both offer Original (100%), Medium (50%), and Small (33.33%), and both default
to Original. Restore video defaults restores both. Existing HUD size choices
are preserved; the newly introduced map choice starts at Original.

`video.hud_size` and `video.map_size` are saved independently in settings.ini.
Quest map upscaling remains a separate image-quality toggle and does not decide
how large the map appears. Texture packs, sampling quality, and rendering
resolution are unchanged by either display-size choice.

The top-left selection now checks the traced clock, gauge, sharpness, and
name-plate UV regions. Menu corners from the same atlas are excluded instead
of accepting all main-sheet fragments in the upper-left screen rectangle.
Recognition remains limited to the observed normal quest HUD layout; test
other menus, weapons, and areas before considering broader coverage verified.

Build Poke, mhp2g_hud_layout_tests, and mhp2g_settings_tests, run both tests,
then check equipment details at each HUD size and each map size. In particular,
check Small HUD + Original map and Original HUD + Small map. Shader changes
are not required by this follow-up. No overlays need rebuilding.
