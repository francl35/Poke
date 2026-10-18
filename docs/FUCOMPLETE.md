# FUComplete development build

This branch adds an isolated FUComplete instance to Poke. It reuses the MHP2G
host profile, renderer, audio, input and settings implementation, while compiling
its main executable from the reviewed FUComplete ELF. It is a development build,
not a general FUComplete release or a separate international MHFU profile.

## Compatibility checkpoint

On 2026-10-18, the developer tested the FUComplete worktree on Apple Silicon macOS:
startup and gameplay, texture replacements, crisp text, FOV, HUD size, the idle
and L-held item selector, and the sharpness replacement alignment. A normal-run
log shows encrypted save output followed by a successful load from the same
normal save directory. Original and Medium HUD sizes were used during the gauge
investigation. This is manual development evidence, not a full-game completion
or a reproducible automated graphics comparison.

The inherited MHP2G work was previously tested on Steam Deck. The cleaned
FUComplete branch itself still needs Linux/Steam Deck build and gameplay testing.
Windows, Android, multiplayer, complete quest coverage and all FUComplete-specific
features are not validated by this checkpoint. No Flatpak or macOS app bundle is
provided here; the macOS command launcher requires a local build and Python.

## Why a separate native corpus is necessary

Loading a different ELF changes guest memory; it does not change instructions
already compiled into native C++. A simpler language-patched ISO worked with the
original MHP2G executable in earlier testing. FUComplete modifies executable code
as well as game data, so the native main corpus must be regenerated from its ELF.

The reviewed ELF SHA-256 is:

```text
02811bb3a529d48d707072a726e87106d0ecdab62aa7e494b23ecf1469eb662f
```

The original MHP2G ELF used during comparison has SHA-256
`3c221249fcc1c3455ea8aff372993cb78a88ad90bde03a07c3da6974c2bcc846`.
The comparison found 1,820 changed words over 331 ranges in a load segment
containing both code and data. Four original calls (`0x0E213A58`) are NOPs in
FUComplete at `0x0884D868`, `0x0884D8B8`, `0x0884E93C` and `0x0884EE0C`.
The helper checks both the reviewed hash and these sites. A new FUComplete
version must be reviewed before changing the pin; bypassing it is not support.

## Directory layout

| Resource | Location in the FUComplete checkout |
| --- | --- |
| Game ELF and ISO links | `mhfuc/EBOOT.ELF`, `mhfuc/disc.iso` |
| Persistent memory stick | `mhfuc/ms0/` |
| Observed FUComplete save directory | `mhfuc/ms0/PSP/SAVEDATA/ULUS10391/` |
| Build and executable | `out/mhfuc/`, `out/mhfuc/bin/Poke` |
| Generated main corpus | `out/mhfuc/generated/` |
| Overlay candidate libraries | `out/mhfuc/bin/overlays/` |
| Analysis and generation state | `out/mhfuc/analysis/`, `out/mhfuc/fuc-build-state.json` |
| Persistent host settings | `out/fuc-user-data/settings.ini` |
| Texture assets for this instance | `out/fuc-user-data/textures/` |

The normal launcher fixes these paths relative to its checkout. Launch from this
checkout using `run_fuc.py`; a bare binary launch without `--data-dir` can select
the ordinary per-user MHP2G configuration instead. Do not rename a configured
build directory or copy its CMake cache: generated Ninja wrapper paths are absolute.

The shared profile's default corpus remains `profiles/mhp2g/generated/`.
`MHP2G_GENERATED_DIR` selects the FUComplete corpus only for this build.
`MHP2G_AOT_ELF_SHA256` prints compiled provenance; it does not broaden the runtime's
supported-executable allowlist or validate an arbitrary replacement ISO.

## Existing working checkout: incremental build

From the FUComplete checkout root:

```sh
cmake -S . -B out/mhfuc -DMHP2G_KTX2=bundled
cmake --build out/mhfuc --target Poke --parallel 2
python3 run_fuc.py
```

Keep the existing generated code and compatible overlay libraries. Host/UI changes
do not require regeneration. Regenerate only when the ELF or generator changes,
or when deliberately rebuilding the generated corpus.

## New checkout: prerequisites and game input

Use the separate `feature/fucomplete` branch/worktree. Keep the existing stock
checkout intact. Both branches share Git history; they do not require a new remote
repository. Instructions below assume a POSIX shell on macOS or Linux, Python
3.9+, a C++20 compiler, CMake, Ninja or Make, Git, SDL3, Vulkan headers/loader and
glslangValidator. macOS also needs MoltenVK. See the main README for platform
package examples. Bundled FFmpeg and KTX2 dependencies may require network access
on their first build. Steam Deck can use the same prepared Distrobox build
environment as stock Poke; this branch does not install or configure it.

Prepare the decrypted FUComplete executable and matching patched ISO locally.
They must belong to the reviewed variant. In the developer's dump the usable ELF
was `PSP_GAME/SYSDIR/EBOOT.BIN`; `BOOT.BIN` was not an ELF. Do not mix an original
MHP2G ELF with a FUComplete ISO.

```sh
mkdir -p mhfuc
ln -s /absolute/path/to/FUComplete/PSP_GAME/SYSDIR/EBOOT.BIN mhfuc/EBOOT.ELF
ln -s /absolute/path/to/FUComplete.iso mhfuc/disc.iso
file -L mhfuc/EBOOT.ELF
python3 -c 'import hashlib,pathlib; print(hashlib.sha256(pathlib.Path("mhfuc/EBOOT.ELF").read_bytes()).hexdigest())'
```

These link commands are for a new runtime folder; they do not overwrite existing
links. Keep saves in their existing normal location. No game data, texture pack,
save, generated corpus or compiled overlay belongs in a source commit.

## New checkout: tools and FUComplete build

Bootstrap a tools-only build in this checkout, avoiding another checkout's cache:

```sh
cmake -S . -B out/fuc-tools -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DPSPRECOMP_PROFILE= \
  -DPSPRECOMP_BUILD_TESTS=OFF

python3 profiles/mhp2g/tools/build_fuc.py \
  --tools-build-dir out/fuc-tools \
  --jobs 2 --generated-jobs 1
```

The helper builds the analyzer/recompiler, verifies the ELF, generates the main
corpus under `out/mhfuc/generated`, configures the separate renderer build and
builds Poke. KTX2 is bundled by default; `--ktx2 OFF` is an explicit opt-out.
It preserves the selected reference FFmpeg mode, defaulting to bundled.
Increase generated concurrency only when memory permits. Two ordinary build
jobs and one generated compile are conservative defaults for a handheld.

An existing **same-checkout** reference cache can be used instead:

```sh
python3 profiles/mhp2g/tools/build_fuc.py \
  --tools-build-dir out/mhp2g-renderer --jobs 2 --generated-jobs 1
```

The helper never copies that cache or its Ninja wrapper. It skips generation only
when the ELF, recompiler and generated-source hashes match the recorded state.
`--regenerate` forces generation and preserves the previous generated corpus in a
local backup. Do not delete `fuc-build-state.json` to adopt unrelated build output.
Custom `--game-dir` or `--build-dir` values require a corresponding explicit binary
launch; the provided normal launcher intentionally uses the table's fixed paths.

## Install compatible overlay candidates

For this checkpoint, copy a locally built compatible MHP2G overlay candidate
pool into the FUComplete build. Use libraries for the same OS/architecture and
compatible runtime headers:

```sh
OVERLAY_SOURCE="/absolute/path/to/Poke/out/mhp2g-renderer/bin/overlays"
mkdir -p out/mhfuc/bin/overlays
cp -R "$OVERLAY_SOURCE/." out/mhfuc/bin/overlays/
```

The observed pool contained 298 libraries; that count is not a compatibility
check. The runtime checks loaded code identity before selecting a corpus.
Changed overlays may fall back to interpretation. This helper does not generate
FUComplete-specific overlays, and the stock overlay build script must not be
assumed to support the patched archive. Reusing a candidate pool is a development
step, not a claim that stock code implements all patched overlays.

## Run, textures and settings

```sh
python3 run_fuc.py
```

On macOS, `chmod +x Launch-FUComplete.command` enables the supplied command launcher.
It resolves its own directory and runs `python3` from PATH. Finder must be able to
find that Python and the build's dynamic libraries. This is not a self-contained app.
On Linux/Steam Deck, run the Python launcher inside the environment that supplies
the binary's libraries; a native clickable/Flatpak package remains future work.

Place a complete user-supplied texture pack in `out/fuc-user-data/textures/`,
including `textures.ini`, or select its folder through the existing texture-pack
menu. Use separate texture folders if different stock/FUComplete atlas revisions
are needed. Video settings persist in this instance's `settings.ini`; text, FOV,
map enhancement and HUD size reuse the existing MHP2G menu implementation.
The launcher clears quality and tracing environment overrides so the saved menu
choices apply. It supplies only this instance's overlay directory explicitly.

The HUD classifier recognizes the FUComplete selector's x=312 idle and L-held
openers and bounds through y=264. Accepted pieces share the existing (472,260)
anchor. Map scaling remains separate. Menu pieces outside these traced layouts
are not broadly admitted by their shared texture-sheet address.

The `ddeab148` HD sheet has gauge artwork at rows 44..52 while the traced
FUComplete draw requests 43..51 at screen (60,32)..(104,40). A one-source-texel
V adjustment applies only after that named replacement is actually uploaded,
on the exact 256x256 CLUT8 draw. It changes sampling, not geometry or clipping.
Native textures, pending/failed replacements, other sheets and stock 44..52 UVs
retain their normal path. This is a targeted pack-compatibility rule, not a
universal correction for every independently authored replacement atlas.

## Saving and troubleshooting

Use `run_fuc.py` for normal play. Old diagnostic captures deliberately used copied
saves under `out/item-ui-comparison/.../runtime/game/ms0`; progress there does not
appear in the normal memory stick. Keep any such folders until their saves have
been reviewed/backed up. This cleanup does not copy or merge saves.

```sh
python3 run_fuc.py > out/fuc-normal.log 2>&1
```

Successful normal logs should show the ELF from `mhfuc/EBOOT.ELF`, the compiled
FUComplete corpus hash, the matching ISO and save paths under `mhfuc/ms0`.
An unsupported-executable warning may remain because the ordinary identity
allowlist has not been broadened. Verify the actual ELF/corpus hashes instead of
suppressing the warning. Existing module/import logging stubs remain limitations.
The host DATA.BIN mod parser may report `mods off` for this archive even when
normal guest gameplay succeeds; this branch does not claim host mod-service support.

## Validation before committing or sharing a build

Run `python3 profiles/mhp2g/tests/fuc_build_tests.py` and
`python3 profiles/mhp2g/tests/fuc_launcher_tests.py`. They use synthetic inputs;
no game data or network is required. The cleanup package also compiles/runs the
item-layout and sharpness-UV tests with assertions enabled, then builds Poke.
CTest registers these as `mhp2g_hud_item_layout_tests` and
`mhp2g_sharpness_replacement_uv_tests` when `PSPRECOMP_BUILD_TESTS=ON`.
Run the full relevant project suites using the contributor guide before proposing
a merge; the standalone checks do not replace them or establish 80% diff coverage.

Manually recheck startup, chest/inventory and start-menu corners, smith preview,
full/collapsed quest maps, HUD Original/Medium/Small with L-held item cycling,
texture pack on/off, crisp text, FOV, a quest/zone transition, and save/load after
restarting. Recheck this on Steam Deck before describing the FUComplete branch
as tested there. Do not distribute game assets or locally generated source corpora.
