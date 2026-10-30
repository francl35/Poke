# Building MHP1 / FreedomDX on macOS

Use the same cleanup source branch and preserved generated corpus as Linux.
Rebuild all host code and overlay libraries on Mac. Linux `.so` files, CMake
caches and dependency outputs cannot be reused. Flatpak packaging is Linux-only.

## 1. Prepare the toolchain

Install Apple's Command Line Tools (or full Xcode), Python 3.9+, Git, CMake,
Ninja, pkg-config, SDL3, shader compilation tools and a Vulkan loader/MoltenVK
combination known to work with this project. Retain your previously working Mac
Vulkan configuration. Do not mix incompatible Homebrew/MacPorts architectures or
stale Xcode SDK paths.

```bash
xcode-select -p
xcrun --find clang
xcrun --find clang++
xcrun --sdk macosx --show-sdk-path
uname -m
cmake --version
ninja --version
pkg-config --modversion sdl3
```

If full Xcode was removed and Command Line Tools are installed:

```bash
sudo xcode-select --switch /Library/Developer/CommandLineTools
```

Only run that switch when appropriate for your installation. The Vulkan device
must satisfy the renderer's Vulkan 1.1 requirement. A stale loader/ICD previously
advertised Vulkan 1.0 on the M1 Pro. Correct the selected loader/ICD; do not lower
the renderer requirement to hide it. Record any working Vulkan environment
settings rather than guessing an SDK path.

## 2. Bring the private inputs

Transfer unchanged:

- `profiles/mhp1/generated/`: generated C++ and headers, including registry and
  existing FreedomDX-specific adaptations.
- `profiles/mhp1/overlays/`: generated C++, headers, metadata and build inputs for
  all 105 overlays.
- Your reviewed game ELF/ISO and a backup/copy of your saves, kept out of Git.

Do not substitute the encrypted BOOT.BIN for reviewed EBOOT.ELF. Do not blindly
regenerate the adapted main corpus. Runtime code hashes and overlay ABI remain
subject to the normal validation.

## 3. Audit and configure a new build directory

```bash
python3 tools/check_mhp1_project.py --root . \
  --json-out "$HOME/Downloads/mhp1-mac-audit.json"
```

Choose a new report filename on subsequent runs. Resolve missing corpora and
active cross-profile dependencies before building.

```bash
CC="$(xcrun --find clang)" CXX="$(xcrun --find clang++)" \
cmake -S . -B out/mhp1-mac-clean -G Ninja \
  -DCMAKE_C_COMPILER="$(xcrun --find clang)" \
  -DCMAKE_CXX_COMPILER="$(xcrun --find clang++)" \
  -DCMAKE_OSX_SYSROOT="$(xcrun --sdk macosx --show-sdk-path)" \
  -DPSPRECOMP_PROFILE=mhp1 -DCMAKE_BUILD_TYPE=Release \
  -DMHP1_BUILD_BOOT=ON -DMHP1_BUILD_PROBE=OFF \
  -DMHP1_BUILD_NATIVE_MAIN=ON \
  -DMHP1_VULKAN=ON -DMHP1_CODECS=ON \
  -DMHP2G_FFMPEG=bundled -DMHP2G_KTX2=bundled \
  -DPSPRECOMP_BUILD_TESTS=ON \
  -DPSPRECOMP_BUILD_PROFILE_TESTS=ON \
  -DPSPRECOMP_GENERATED_JOBS=2 \
  -DPSPRECOMP_GENERATED_OPT_LEVEL=2
```

The compiler `-D` options are arguments to CMake, not standalone shell commands.
Use a new build directory rather than reusing the deleted-Xcode cache. If bundled
FFmpeg still selects a different compiler, inspect its configure/ffbuild logs and
the helper's compiler overrides; top-level CMake settings alone do not prove its
selected compiler. Do not copy Linux bundled dependency outputs.

```bash
cmake --build out/mhp1-mac-clean --parallel 2
ctest --test-dir out/mhp1-mac-clean --output-on-failure
cmake --build out/mhp1-mac-clean --target help
find out/mhp1-mac-clean/bin/overlays -maxdepth 1 -type f
```

Mac module suffix depends on the project CMake configuration; inspect output and
loader logs instead of assuming `.dylib`. Confirm all 105 corpora load. If overlay
targets are excluded from the default build, use the existing overlay helper or
listed targets. This guide does not invent a target name.

## 4. Run and validate

```bash
MHP1_NATIVE_MAIN=1 MHP1_NATIVE_OVERLAYS=1 \
MHP1_OVERLAY_DIR="$PWD/out/mhp1-mac-clean/bin/overlays" \
MHP1_PRESENT_ON_FLIP=1 \
python3 profiles/mhp1/scripts/run_headless.py \
  --renderer --build-dir out/mhp1-mac-clean \
  --elf profiles/mhp1/game/EBOOT.ELF \
  --iso "/absolute/path/MH Portable DX.ISO" \
  --ms0 "$PWD/out/mhp1-headless/ms0" --seconds 0
```

Use actual local paths. The memory-stick parent should retain your working
`renderer-data/` folder; this command uses `out/mhp1-headless/`. If needed, set
`MHP2G_FONT` to an existing local `.ttf`, `.otf` or `.ttc`, not a Linux font path.
Keep performance tracing optional, not a forced launcher default.

Inspect `out/mhp1-mac-clean/boot.log` for native-main+fallback, overlays, real GPU,
font selection and fatal errors. Diagnostic exit 4 alone is not proof of a crash.
Test controller/keyboard, menu pause, font selection, textures/KTX2, save/load,
character creation, village and a monster quest. Start at 30 game FPS with frame
interpolation off because NPC/particle interpolation remains a known issue.

## 5. Acceptance before cleanup is final

A fresh Mac build must work without Downloads helper paths or references into
excluded profiles. Preserve attribution, licenses and vendor notices. Keep
compatibility option/environment/export names until deliberately migrated.
Commit documentation/source changes only after reviewing the audit and local test
results. These instructions have not been executed as a native Mac build by the
assistant.
