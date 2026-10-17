# Build and check MHP2G on Steam Deck

This guide checks the UI rendering and KTX2 texture-pack checkpoint on Linux.
Use Steam Deck Desktop Mode and Konsole. Run commands in your existing Linux
build environment; if you previously built inside Distrobox, enter that same
container before continuing. Build and launch there so dependency libraries
are available.

## 1. Update the checkout

Open your Poke repository directory, then inspect its state:

```bash
cd /absolute/path/to/Poke
git status --short
git branch --show-current
```

If the checkpoint is on your current branch's upstream and you have no
conflicting local changes:

```bash
git pull --ff-only
git log -1 --oneline
```

If you transferred the checkout another way, confirm it contains the checkpoint
instead. This guide does not switch branches or discard local changes.

## 2. Choose your directories

Set these variables in the same terminal used for subsequent commands:

```bash
BUILD_DIR="$PWD/out/mhp2g"
GAME_DIR="$PWD/game"
PACK_DIR="$HOME/path/to/extracted-texture-pack"
```

Edit `PACK_DIR` to the real pack location. It must directly contain
`textures.ini`, with its referenced `ui/`, `equipment/`, `hunter/`, `areas/`,
and other folders alongside it. Copy the complete pack and your adjusted
`ui/hud/ddeab148.ktx2` from the Mac.

If your existing Deck build is `out/mhp2g-renderer`, use this instead:

```bash
BUILD_DIR="$PWD/out/mhp2g-renderer"
```

If your game data is still in `mhp2g-test-game`, use:

```bash
GAME_DIR="$PWD/mhp2g-test-game"
```

The documented defaults are `out/mhp2g` and `game`. Do not rename or copy a
configured CMake build tree from macOS. Linux needs its own executable and
shared overlay libraries. Existing Linux build outputs can be reused.

## 3. Check prerequisites

This update assumes your existing Deck build environment already has the normal
Poke dependencies: a C++20 compiler, CMake, Python 3, a build tool, SDL3, Vulkan,
glslang, and the project's configured media dependencies. Bundled KTX2 needs
CMake 3.22 or newer and a C compiler.

```bash
cmake --version
c++ --version
cc --version
python3 --version
command -v glslangValidator || command -v glslang
```

For a first-ever checkout, complete the Linux dependency and game-code generation
steps in `BUILDING.md` first. The commands below do not extract or regenerate
the main game executable. Do not install Debian `apt` packages directly on
SteamOS; use your existing development container or Linux dependency setup.

## 4. Configure and build

Run this block from the repository root. It stops on the first error:

```bash
(
  set -eu

  cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DPSPRECOMP_PROFILE=mhp2g \
    -DMHP2G_RENDERER=ON \
    -DMHP2G_KTX2=bundled

  cmake --build "$BUILD_DIR" \
    --target Poke mhp2g_texture_pack_tests mhp2g_texture_pack_codec_tests \
    --parallel 4
)
```

For a new build directory, you can add `-G Ninja` to the configure command.
Keep the existing generator when reusing a configured directory.

The first bundled KTX2 configuration downloads approximately 203 MiB of pinned
KTX-Software sources and builds the dependency. Later builds reuse it. No
separate KTX shared library is required in bundled mode.

Start with four jobs on the Deck. Reduce `--parallel 4` to `--parallel 2` if
compilation runs out of memory. A project-defined Ninja job pool can still
limit generated-code compilation; `--parallel` is an upper bound.

Check the configure output confirms the Vulkan renderer is enabled. If SDL3,
Vulkan, or glslang was not found, resolve dependency discovery before gameplay
testing; a successful headless build is insufficient for this check.

## 5. Run texture tests

```bash
(
  set -eu
  "$BUILD_DIR/bin/mhp2g_texture_pack_tests"
  "$BUILD_DIR/bin/mhp2g_texture_pack_codec_tests"
  "$BUILD_DIR/bin/mhp2g_texture_pack_codec_tests" \
    "$PACK_DIR/ui/hud/ddeab148.ktx2"
)
```

All three commands should succeed. The last command checks your actual HUD
replacement, in addition to the synthetic codec/hash tests.

## 6. Overlays: rebuild only when needed

These renderer/font/texture changes do not require regenerating the main game
code or rebuilding unchanged Linux overlays. Reuse your working Deck overlays.

For a fresh Linux build, missing overlays, or changed overlay sources:

```bash
bash profiles/mhp2g/scripts/build_overlays.sh "$BUILD_DIR" 4
```

The updated script reads `game/disc.iso`, independently of the `GAME_DIR`
variable above. Ensure that path points to your own ISO before running it.
Passing `BUILD_DIR` explicitly selects where compiled overlay outputs go.
Inspect its final summary and runtime loading; the script's keep-going build
can continue after individual failures.

## 7. Launch with diagnostics

Check the input paths first:

```bash
test -d "$GAME_DIR" && echo "Game directory found"
test -r "$PACK_DIR/textures.ini" && echo "Texture INI found"
test -r "$PACK_DIR/ui/hud/ddeab148.ktx2" && echo "HUD file found"
```

Launch from the same terminal/environment:

```bash
MHP2G_GPU_DECODE=1 \
MHP2G_CRISP_TEXT=1 \
MHP2G_UI_TEXTURES=off \
MHP2G_SELECTIVE_UI=1 \
MHP2G_TEXTURE_PACK="$PACK_DIR" \
MHP2G_TRACE_TEXTURE_PACK=1 \
"$BUILD_DIR/bin/Poke" "$GAME_DIR" \
2>&1 | tee /tmp/poke-deck-check.log
```

`MHP2G_UI_TEXTURES=off` disables global UI enhancement. Crisp text and explicitly
selected UI enhancements remain enabled by their separate switches. These
commands do not persist settings to your configuration file.

The font used for crisp text must be available on Linux. If your previous
configuration points to a macOS system font, select an installed Linux font
with the needed characters, or set `MHP2G_FONT` to an appropriate font file.

## 8. Gameplay checklist

Test both native and 4× internal resolution, and 30/60 FPS where applicable.

- Opening playback: music, movie audio, and normal transition into gameplay.
- Title screen and main menu: background and icon replacements appear.
- House, chest, and equipment menus: text is crisp without stale/missing glyphs.
- Item and equipment icons: replacements load and transparency is correct.
- HUD: health/stamina bars have no new seams or outlines.
- Sharpness: adjusted blade aligns with its fill; inspect other sharpness states.
- Smith weapon preview: weapon and character overlap correctly.
- Character, armor, weapons, village, and field textures: replacements appear.
- Camera and movement: no new 60 FPS wobble, missing faces, or disappearing meshes.
- Controller: buttons work; use the existing D-pad right-stick behavior for camera.
- Texture loading: no sustained stutter, crash, or unexpected memory growth.

Repeat representative UI and smith checks without texture replacement. Unset
the pack variable explicitly so a value inherited by the terminal is removed:

```bash
env -u MHP2G_TEXTURE_PACK -u MHP2G_TRACE_TEXTURE_PACK \
  MHP2G_GPU_DECODE=1 \
  MHP2G_CRISP_TEXT=1 \
  MHP2G_UI_TEXTURES=off \
  MHP2G_SELECTIVE_UI=1 \
  "$BUILD_DIR/bin/Poke" "$GAME_DIR"
```

If the application enables a pack through another configuration path, disable
it there too and verify the replacement HUD is absent.

## 9. Measure normal performance

After diagnostic testing, run without texture-pack tracing:

```bash
env -u MHP2G_TRACE_TEXTURE_PACK -u MHP2G_TRACE_UI \
  MHP2G_GPU_DECODE=1 \
  MHP2G_CRISP_TEXT=1 \
  MHP2G_UI_TEXTURES=off \
  MHP2G_SELECTIVE_UI=1 \
  MHP2G_TEXTURE_PACK="$PACK_DIR" \
  "$BUILD_DIR/bin/Poke" "$GAME_DIR"
```

Keep resolution, FPS, and Deck power settings consistent when comparing runs.
The earlier macOS log showed about 424 MiB of GPU replacement storage for 122
images in the tested session; that is an observation, not a Deck memory limit
or expected usage for every scene.

## 10. Report a problem

Keep `/tmp/poke-deck-check.log` and copy it somewhere permanent before rebooting.
Include the commit (`git rev-parse --short HEAD`), resolution, FPS, pack enabled
or disabled, scene, and a screenshot. For build failures, include the first
compiler/linker error as well as the final failure message.

A matched file followed by `missing or unreadable` usually requires checking
the pack path and extracted assets. A codec error is a separate issue. A key
`not in the pack` can be normal for dynamic glyph pages or uncovered textures.
Restart Poke after correcting missing replacement files, because failed
replacements remain cached during that session.
