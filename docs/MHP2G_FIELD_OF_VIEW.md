# Field of view

Video settings now contain Field of view: Original, -15 to +30 degrees in
5-degree steps. Original is the default. Changes are saved immediately in the
existing per-user settings.ini as video.fov_offset_degrees. Restore video
defaults also restores Original.

These are offsets from the live vertical perspective angle, not absolute camera
angles or percentages. The renderer derives the angle from the game's projection
matrix; it does not assume the MHP3rd camera constants apply to MHP2G. The resulting
angle is limited to 5–120 degrees. Unrecognized projections remain unchanged.

The adjustment covers normal and interpolated frames. It preserves depth and
projection center. Through-mode HUD, map size, texture quality and offscreen
render targets retain their existing behavior. Displayed perspective draws,
including perspective equipment previews and cutscenes, receive the adjustment.

This changes projection only. The game's original visibility culling is unchanged;
check for objects disappearing at wider screen edges. Test a quest, indoor scenes,
equipment previews, camera zooms, and interpolated and non-interpolated modes.
Check Original against the previous build, then try +10 and -10 degrees, and restart
to verify persistence. HUD and map settings should retain their existing sizes.

From the repository root, using the existing configured build directory:

```sh
c++ -std=c++20 -O2 -Iprofiles/mhp2g/host profiles/mhp2g/tests/fov_tests.cpp -o /tmp/mhp2g-fov-tests
/tmp/mhp2g-fov-tests
cmake --build out/mhp2g-renderer --target Poke --parallel 4
./out/mhp2g-renderer/bin/Poke
```

No overlay regeneration is required. Use your actual configured build directory
if different; do not rename a configured CMake build tree.

To report the first recognized native projection for each selected offset:

```sh
MHP2G_TRACE_FOV=1 ./out/mhp2g-renderer/bin/Poke > /tmp/poke-fov.log 2>&1
```

Look for [fov] entries with native vertical, offset and rendered angle. This trace
is optional; the menu and persistence work without environment variables.
