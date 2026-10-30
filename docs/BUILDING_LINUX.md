# Building MHP1 / FreedomDX on Linux

These instructions target the current Kokoto cleanup branch. They do not certify
that the copied packaging kit is self-contained: run the audit first. Use native
Linux builds for development; use Flatpak for a reproducible distributable runtime.
Do not copy CMakeCache.txt or compiled libraries from a different machine.

## 1. Prepare the checkout and inputs

Use Git, Python 3.9+, CMake 3.24+, Ninja, a C++20 compiler, pkg-config and the
platform development dependencies required by the project. The Vulkan build needs
Vulkan headers/loader, working graphics drivers, SDL3 development files and a GLSL
compiler. Bundled FFmpeg also needs its configure/build prerequisites (including
make and any assembler its configuration requests). Package names differ across
Fedora, Debian/Ubuntu and SteamOS; a fixed apt/dnf command would not cover all three.
On Steam Deck, use the known-working development container or the Flatpak SDK
rather than modifying the immutable host system.

Check tools:

```bash
git --version
python3 --version
cmake --version
ninja --version
c++ --version
pkg-config --modversion sdl3
vulkaninfo --summary
```

`vulkaninfo` is optional diagnostic tooling, not a build dependency. A software
Vulkan device alone is not the intended Deck renderer.

Keep your legally obtained ISO and reviewed EBOOT.ELF outside version control.
Keep the existing adapted `profiles/mhp1/generated/` and all 105
`profiles/mhp1/overlays/*/` generated C++/headers/metadata. Those inputs may be
ignored by Git and therefore absent after cloning. Do not blindly regenerate the
main corpus: it includes FreedomDX runtime-patch handling.

## 2. Audit before building

From the checkout root:

```bash
python3 tools/check_mhp1_project.py --root . \
  --json-out /home/deck/Downloads/mhp1-linux-audit.json
```

Choose a new report name if that file already exists. Review legacy paths and
missing corpora. Duplicate hashes do not imply safe removal.

## 3. Configure a fresh native build

Use a new directory for the first cleanup build; do not discard the known-working
old build. This command uses the options established for the current project:

```bash
cmake -S . -B out/mhp1-linux-clean -G Ninja \
  -DPSPRECOMP_PROFILE=mhp1 \
  -DCMAKE_BUILD_TYPE=Release \
  -DMHP1_BUILD_BOOT=ON -DMHP1_BUILD_PROBE=OFF \
  -DMHP1_BUILD_NATIVE_MAIN=ON \
  -DMHP1_VULKAN=ON -DMHP1_CODECS=ON \
  -DMHP2G_FFMPEG=bundled -DMHP2G_KTX2=bundled \
  -DPSPRECOMP_BUILD_TESTS=ON \
  -DPSPRECOMP_BUILD_PROFILE_TESTS=ON \
  -DPSPRECOMP_GENERATED_JOBS=2 \
  -DPSPRECOMP_GENERATED_OPT_LEVEL=2
```

Legacy `MHP2G_*` option names are compatibility interfaces; retain them until an
explicit migration. Stop on unused-option warnings concerning native code or
codecs, rather than assuming they were enabled. Bundled dependencies may download
or compile during configuration; keep the terminal output and inspect their logs
if progress stops.

```bash
cmake --build out/mhp1-linux-clean --parallel 2
ctest --test-dir out/mhp1-linux-clean --output-on-failure
```

The default build should include configured overlay targets. Check the actual
targets and output before assuming all overlays were built:

```bash
cmake --build out/mhp1-linux-clean --target help
find out/mhp1-linux-clean/bin/overlays -maxdepth 1 -type f -name '*.so' | wc -l
```

Expected for the preserved full corpus: 105 libraries. If overlays are excluded
from the default target in your version, use the repository's existing overlay
build helper/targets; do not invent an aggregate target. Start with two generated
compilation jobs; raise cautiously after observing RAM use. `--parallel` does not
override a more restrictive generated-code CMake job pool.

## 4. Run with explicit paths

```bash
MHP1_NATIVE_MAIN=1 MHP1_NATIVE_OVERLAYS=1 \
MHP1_OVERLAY_DIR="$PWD/out/mhp1-linux-clean/bin/overlays" \
MHP1_PRESENT_ON_FLIP=1 \
python3 profiles/mhp1/scripts/run_headless.py \
  --renderer --build-dir out/mhp1-linux-clean \
  --elf profiles/mhp1/game/EBOOT.ELF \
  --iso "/absolute/path/MH Portable DX.ISO" \
  --ms0 "$PWD/mhp1/ms0" --seconds 0
```

Replace ISO/ELF/save paths with your actual locations. Back up saves first. The
current runner derives renderer-data from the memory-stick parent; preserve your
working layout. Exit 4 can be the diagnostic runner's expected termination code;
read `stopped=` rather than treating it alone as a crash.

Check `out/mhp1-linux-clean/boot.log`: native-main+fallback, 105 overlay corpora,
GPU-eligible vertex decoding and no fatal missing imports. Rare interpreter
fallbacks remain possible. Test menu/fonts/controller, saving/loading, textures,
character creation and a monster quest. Baseline: 30 game FPS, interpolation off,
approximately 100% speed; presentation Hz is a different counter.

## 5. Flatpak audit and build

Keep the complete working kit under `packaging/flatpak/`, preserving helper-relative
paths. Until its CLI is reviewed, use its `--help` rather than assuming flags.
Create a source context outside the checkout. It must include source, corpora,
manifest patches, launcher, importer, branding and required vendor notices. It
must not copy an entire working build/cache/game folder as its source module.

```bash
python3 tools/check_mhp1_project.py --root . \
  --manifest /absolute/path/context/io.github.francl35.Kokoto.json \
  --context /absolute/path/context \
  --installed-files /absolute/path/context/flatpak-build/files \
  --json-out /home/deck/Downloads/mhp1-flatpak-audit.json
```

Omit `--installed-files` before the first build. JSON manifests are supported;
external module files and remote archive contents require separate review.
`repo/`, `.flatpak-builder/` and staging output belong outside the source payload;
they are normal in the external context and are not automatically junk.

From the prepared context, the established Builder command is:

```bash
flatpak run org.flatpak.Builder \
  --arch=x86_64 --jobs=2 --ccache --user \
  --install-deps-from=flathub --repo=repo --install \
  flatpak-build io.github.francl35.Kokoto.json
```

Use your kit's verified cache workflow to reuse overlays. A `.so` filename alone
is insufficient: runtime headers, corpus, SDK/compiler and build flags must match.
Do not bypass cache mismatch validation. A source change needs rebuilding; a
permission-only change can use the established repackaging helper. Do not apply
`--finish-only` to an already finalized build directory.

Validate installed metadata and controller permission:

```bash
flatpak info --user io.github.francl35.Kokoto
flatpak info --user --show-permissions io.github.francl35.Kokoto
flatpak info --user --show-metadata io.github.francl35.Kokoto
```

Select `--branch=test` where appropriate for the installed branch. Keep the tested
`--device=all` manifest permission until a narrower alternative is validated.
Use the copied import_game.py `--help` for import options. Data stays under
`~/.var/app/io.github.francl35.Kokoto/data/kokoto/`. Never package personal saves or
ISO/ELF game inputs. Verify fonts, game import and controller in the sandbox.

This audit cannot verify OSTree repository integrity. If available, use
`ostree --repo=repo fsck` separately; application behavior still needs a launch test.
