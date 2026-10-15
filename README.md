# Poke

**[English](README.md) | [日本語](README.jp.md)**


**Poke** is an experimental native recompilation project focused on **Monster Hunter Portable 2nd G** (MHP2G, known internationally as *Monster Hunter Freedom Unite*) for PSP.

Poke is a **derivative of [TeamGDB/Yakumo](https://github.com/TeamGDB/Yakumo)**. The PSP runtime, recompilation framework, and substantial portions of the host implementation build on work done by Yakumo's original contributors. **Poke is not the original Yakumo project, is not maintained by the Yakumo team, and is not an official successor.** Please refer to the upstream project for its own releases, development, and support. Credit for the original work belongs to its respective authors and contributors.

This fork focuses on adapting that technology to MHP2G. The inherited MHP3rd profile remains in the repository; its presence does not mean that Poke's MHP2G build is a release of the original Yakumo project.

## Current status

**Work in progress — not a general release.**

- The MHP2G executable has been analyzed and its main code generated as native C++.
- The Linux Vulkan build has booted successfully and village gameplay has been tested.
- The Apple Silicon macOS Vulkan/MoltenVK build has also been tested in gameplay, including GPU vertex decoding and 60 FPS frame interpolation in the tested house scene.
- Quest, combat, zone-transition, and overlay compatibility testing is still in progress.
- The MHP2G CMake target and executable are both named `Poke`.
- Some guest code can fall back to an interpreter; the project is not exclusively ahead-of-time execution.

These observations are development test results, not a claim of full game compatibility or platform support.

## Game files and legal notice

**No copyrighted game content is included in this repository.** You must supply files from your own legally obtained copy of *Monster Hunter Portable 2nd G*. Game disc images, decrypted executables, extracted assets, game-derived generated C++ code, and compiled game overlays are not distributed here and must not be committed.

Poke is an unofficial fan-made compatibility project and is not affiliated with, authorized, endorsed, or sponsored by CAPCOM, Sony, or their affiliates. *Monster Hunter*, *Monster Hunter Portable 2nd G*, *Monster Hunter Freedom Unite*, PSP, and related names and assets belong to their respective owners. Users are responsible for complying with applicable laws when preparing and using their own game files.

## Building MHP2G on Linux (development)

The Linux renderer build needs a C++20 compiler, CMake, Python 3, SDL3 development files, Vulkan loader/header development files, shader compilation tools, and standard build tools. The renderer also uses FFmpeg for media support.

### Linux requirements

Install the required build tools before configuring Poke.

On Fedora, Bazzite, or Fedora-like containers:

```bash
sudo dnf install \
  gcc-c++ \
  cmake \
  python3 \
  SDL3-devel \
  vulkan-loader-devel \
  vulkan-headers \
  glslang \
  make \
  ninja-build \
  git
```

On Debian or Ubuntu-like environments, package names may differ by release. A typical starting point is:

```bash
sudo apt update
sudo apt install \
  build-essential \
  cmake \
  python3 \
  libsdl3-dev \
  libvulkan-dev \
  glslang-tools \
  make \
  ninja-build \
  git
```

Check the tools before building:

```bash
command -v cmake
command -v python3
command -v glslangValidator || command -v glslang
c++ --version
cmake --version
```

If `cmake` is not available, install it before continuing. The examples below detect CMake through `PATH` instead of assuming `/usr/bin/cmake` exists:

```bash
CMAKE_BIN="$(command -v cmake)"
test -n "$CMAKE_BIN" || {
  echo "error: cmake is not installed or not in PATH" >&2
  exit 1
}
```

### Steam Deck / SteamOS using Distrobox

For Steam Deck, the recommended workflow is to build Poke inside a Distrobox container instead of installing development packages directly into SteamOS. SteamOS is immutable, and packages installed directly with `pacman` may be removed by system updates.

Check whether Distrobox is already available:

```bash
command -v distrobox
```

If it is missing, install a Distrobox frontend such as BoxBuddy from Flathub:

```bash
flatpak install flathub io.github.dvlv.boxbuddyrs
```

BoxBuddy provides a graphical interface for creating and entering containers. If the `distrobox` command-line tool is available, create a Fedora build container:

```bash
distrobox create \
  --name poke-fedora \
  --image registry.fedoraproject.org/fedora:latest \
  --home "$HOME/distrobox-homes/poke-fedora"
```

Enter the container:

```bash
distrobox enter poke-fedora
```

Inside the Fedora Distrobox, install the build dependencies:

```bash
sudo dnf install \
  gcc-c++ \
  cmake \
  python3 \
  SDL3-devel \
  vulkan-loader-devel \
  vulkan-headers \
  glslang \
  make \
  ninja-build \
  git
```

Verify the tools inside the container:

```bash
command -v cmake
command -v python3
command -v glslangValidator || command -v glslang
c++ --version
cmake --version
```

Build Poke from inside the container using the Linux build commands below. Your Steam Deck home directory and project files are still accessible from the container.

To leave the container:

```bash
exit
```

To re-enter later:

```bash
distrobox enter poke-fedora
```

If Japanese text does not render correctly, install a CJK font inside the container:

```bash
sudo dnf install google-noto-sans-cjk-fonts
```

Then either choose the font from Poke's menu or run with `MHP2G_FONT` set to a readable CJK font path. The exact path may vary; find it with:

```bash
fc-match :lang=ja
fc-list :lang=ja file | head
```

Direct package installation on SteamOS is not the recommended workflow. If you choose to do it anyway, SteamOS is Arch-based and requires disabling the read-only system partition before using `pacman`; this is more intrusive and may be undone by SteamOS updates.

### Configure and build

Configure and build the development tools and renderer:

```bash
CMAKE_BIN="$(command -v cmake)"
test -n "$CMAKE_BIN" || {
  echo "error: cmake is not installed or not in PATH" >&2
  exit 1
}

"$CMAKE_BIN" -S . -B out/mhp2g-renderer -G "Unix Makefiles" \
  -DPSPRECOMP_PROFILE=mhp2g -DMHP2G_RENDERER=ON

"$CMAKE_BIN" --build out/mhp2g-renderer --target Poke -j"$(nproc)"
```

Generate the main ahead-of-time code locally from your **own decrypted MHP2G `EBOOT.ELF`**:

```bash
profiles/mhp2g/scripts/generate.sh \
  "$PWD/out/mhp2g-renderer" \
  "/path/to/your/EBOOT.ELF"
```

Rebuild the executable after generation:

```bash
"$CMAKE_BIN" --build out/mhp2g-renderer --target Poke -j"$(nproc)"
```

Run it with the directory containing your local game files:

```bash
MHP2G_GPU_DECODE=1 \
./out/mhp2g-renderer/bin/Poke "/path/to/your/game-directory"
```

**Overlay generation is not yet documented as a verified, standalone Poke workflow.** The profile includes an overlay build script, but its game-image path and output assumptions are being adapted. Until that is completed, some code may execute through the interpreter. Do not copy or commit generated game code or overlay libraries into tracked source directories.

## macOS (Apple Silicon) — Experimental Build

Poke has been built with Vulkan rendering on an Apple Silicon Mac running macOS 15.7.7.

The renderer uses **SDL3 + Vulkan + MoltenVK**, with MoltenVK translating Vulkan commands to Apple's Metal graphics API.

### Dependencies

Install Homebrew and the required graphics dependencies:

```bash
brew install sdl3 molten-vk vulkan-loader vulkan-headers glslang
```

A working configuration was tested with SDL3 3.4.18, MoltenVK 1.4.2, Vulkan 1.4.363.0, and glslang 16.6.0.

Additional build dependencies, including the C++ compiler, CMake, Python, and the PSP recompilation toolchain, are required for a clean build.

### Configure the renderer

From the Poke repository root:

```bash
cmake -S . -B out/mhp2g-renderer \
  -DPSPRECOMP_PROFILE=mhp2g \
  -DMHP2G_RENDERER=ON \
  -DSDL3_DIR=/opt/homebrew/opt/sdl3/lib/cmake/SDL3 \
  -DVulkan_INCLUDE_DIR=/opt/homebrew/opt/vulkan-headers/include \
  -DVulkan_LIBRARY=/opt/homebrew/opt/vulkan-loader/lib/libvulkan.dylib \
  -DMHP2G_GLSLANG=/opt/homebrew/bin/glslangValidator
```

CMake should report:

```text
mhp2g: Vulkan renderer enabled
```

**Important:** Setting `MHP2G_RENDERER=ON` is not sufficient by itself. If SDL3, Vulkan, or glslang cannot be found, CMake may disable the renderer and build a headless executable instead.

### Build

```bash
cmake --build out/mhp2g-renderer --target Poke -j 4
```

### Prepare game files

Poke requires game files obtained from your own copy of the Japanese *Monster Hunter Portable 2nd G*.

Create the following directory:

```text
mhp2g-test-game/
├── disc.iso
└── EBOOT.ELF
```

`disc.iso` is the Japanese game disc image. `EBOOT.ELF` must be a decrypted PSP MIPS executable.

A decrypted `PSP_GAME/SYSDIR/BOOT.BIN` that is a valid MIPS ELF may be used as `EBOOT.ELF`.

Verify its format with:

```bash
file mhp2g-test-game/EBOOT.ELF
```

The tested executable was reported as a 32-bit little-endian MIPS ELF.

Game files must not be committed or redistributed with the repository.

### Generate the main game code

Preparing the game directory does not generate the native game code. A clean checkout also needs the main ahead-of-time C++ code generated from your decrypted `EBOOT.ELF`, followed by a rebuild. The shared generation command is documented in the Linux development section above; the generation script itself has not been verified here as a standalone macOS workflow. If the matching main code has already been generated locally, it does not need regenerating for the renderer fixes below.

### Run

The following GPU vertex decoding configuration was tested on the Apple Silicon development Mac:

```bash
MHP2G_GPU_DECODE=1 \
./out/mhp2g-renderer/bin/Poke "$PWD/mhp2g-test-game"
```

The renderer should initialize through Vulkan/MoltenVK. GPU vertex decoding is opt-in; omit `MHP2G_GPU_DECODE` to use the default decoding path when comparing behavior.

macOS can use the system Hiragino Sans font, so the Linux-specific `MHP2G_FONT=/run/host/...` setting is not needed.

### Frame rate and validated rendering fixes

The game simulation runs at 30 FPS. Selecting 60 FPS in Poke enables the extra-frame rendering path when interpolation is active; it does not change the game's simulation to 60 FPS.

Use a revision containing both of these fixes when testing GPU decoding and interpolation:

- **HUD vertex alignment:** Correct alignment of expanded vertices after raw GPU vertex data resolves intermittent HUD disappearance with GPU decoding enabled.
- **Interpolation camera estimation:** Skinned character draws are excluded from camera-motion sampling, while their matching and animation interpolation remain enabled. This resolved movement-dependent environment shaking in the tested house scene at 60 FPS.

For troubleshooting, select 30 FPS first, then compare the same movement at 60 FPS. The house scene was confirmed working at 60 FPS after the camera-sampling fix; other scenes and camera behaviors still need broader testing.

### Troubleshooting

**`Renderer: not built`**

Check that CMake reported `mhp2g: Vulkan renderer enabled` and rebuild the `Poke` target.

**SDL3 architecture or linking problems**

Ensure CMake uses the ARM64 Homebrew SDL3 installation at `/opt/homebrew/opt/sdl3/lib/cmake/SDL3`, rather than an older `/usr/local` installation.

**`disc.iso not found` or `Missing EBOOT.ELF`**

Check symlink targets with:

```bash
ls -lhL mhp2g-test-game/
```

Symlink targets are resolved relative to the symlink's directory. Absolute symlinks can avoid confusion.

**HUD flickers or disappears with GPU decoding**

Check that the expanded-vertex alignment fix is present in `profiles/mhp2g/host/gpu/vulkan_renderer.cpp`. Disabling GPU decoding can mask the old defect; the fix allows the tested GPU-decoding path to remain enabled.

**Environment shakes while moving at 60 FPS**

Check that the skinned-draw exclusion is present in the camera-sampling loop in `profiles/mhp2g/host/gpu/frame_interpolation.cpp`. A 30 FPS setting is a useful comparison or temporary workaround. The tested house shaking stopped after applying the camera-sampling fix.

**A diagnostic run becomes slow or behaves differently**

`MHP2G_TRACE_SPRITES` disables GPU vertex decoding even when `MHP2G_GPU_DECODE=1` is set. Do not use that trace to judge normal GPU-decoding behavior. Remove diagnostic variables for normal play; several switches test whether the variable exists, so setting them to `0` may still enable them. The temporary HUD tracing and buffered capture patches used during development are not required for either rendering fix.

**No overlay libraries**

Gameplay overlays are built separately. A working renderer does not necessarily mean that all game content or quests are supported.

### Current status

The native executable and Vulkan renderer have been built and tested in gameplay on Apple Silicon. The observed GPU-decoding HUD flicker and the movement-dependent house shaking at 60 FPS were resolved by the fixes described above. These results do not establish full gameplay compatibility, a verified clean-checkout overlay workflow on macOS, Intel Mac support, or compatibility across other Mac configurations.

## Project layout

- `profiles/mhp2g/` — MHP2G host code, tools, shaders, scripts, and tests.
- `profiles/mhp3rd/` — inherited MHP3rd profile from the upstream project.
- `include/psprecomp/`, `src/`, `tools/` — shared PSP runtime and recompilation framework.
- `docs/` — project documentation; some inherited documents still describe upstream MHP3rd behavior and should not be treated as verified MHP2G instructions.

## Credits and licensing

Poke is based on **[TeamGDB/Yakumo](https://github.com/TeamGDB/Yakumo)**, an independent project developed by contributors whom the maintainer of this fork does not represent. Thank you to the Yakumo team and all upstream contributors for the original runtime, tooling, and implementation work.

The upstream project is distributed under the **MIT License**. Retain the original copyright and permission notices in `LICENSE` and any applicable source files. Third-party components may have their own licenses and notices; consult their accompanying license files. The software license does **not** grant rights to distribute CAPCOM or Sony game content.

For upstream Yakumo's own documentation and releases, visit [TeamGDB/Yakumo](https://github.com/TeamGDB/Yakumo). For this fork, use [francl35/Poke](https://github.com/francl35/Poke).
