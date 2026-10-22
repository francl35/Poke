# Poke FreedomDX: MHP1 bring-up

Target: Japanese Monster Hunter Portable, ULJM05066, disc version 1.03,
patched with FreedomDX and English text. The exact supported decrypted EBOOT
SHA-256 is `255215e8573d48e9d74bee0e78c9779789bfc4b600a8271402906c5d0d74e312`.
BOOT.BIN is a different executable and is not accepted by this probe.

## Bounded headless execution milestone

`PokeFreedomDXBoot` now executes guest instructions with the core interpreter
and reused PSP scheduler, memory allocation and ISO/memory-stick I/O services.
It registers native import bridges, not the game-derived AOT corpus. This gives
runtime instruction changes a current-memory baseline before implementing AOT
invalidation. Missing PSP imports stop with their library and NID/name; they are
never filled in with a generic success result. No window/audio or game controls
are provided at this milestone.

```sh
cmake -S . -B out/mhp1-headless \
  -DPSPRECOMP_PROFILE=mhp1 -DMHP1_BUILD_BOOT=ON -DMHP1_BUILD_PROBE=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build out/mhp1-headless --target PokeFreedomDXBoot mhp1_bootstrap_tests mhp1_target_tests --parallel 2
ctest --test-dir out/mhp1-headless -R '^mhp1_(bootstrap|target)_tests$' --output-on-failure
mkdir -p out/mhp1-headless/ms0/PSP/SAVEDATA
python3 profiles/mhp1/scripts/run_headless.py \
  --iso /absolute/path/to/your-patched-freedomdx.iso
```

Replace the ISO path with your actual patched ISO. The raw EBOOT.BIN inside it
must match the pinned hash, as does your local ELF. Only uncompressed ISO9660
images are supported by this milestone, not CSO. The positional memory-stick
directory is explicit and independent of the stock/FUComplete application paths.
To exercise your DX configuration, copy your complete FDXDAT folder to
`out/mhp1-headless/ms0/PSP/SAVEDATA/FDXDAT/` first. CONFIG.BIN belongs directly
inside FDXDAT. Use a diagnostic directory: PSP I/O can write to the supplied ms0.

The guest execution has a 20-second wall-clock limit and a dispatch limit, or
stops earlier at an unimplemented service. The Python runner additionally kills
the native process after 25 seconds, covering an unexpectedly blocked host call. Values 1..60 seconds
are accepted. Exit status 4 is an expected diagnostic stop, not a game-boot PASS;
status 1 reports input/setup/execution errors, and 124 means the runner enforced
its external timeout. Send the complete boot.log.
This does not regenerate or compile any of your 44 generated units. Keep them
for the subsequent AOT integration. Kernel traces, if needed, currently use the
shared host's `MHP2G_TRACE_KERNEL`/`MHP2G_TRACE_SYNC` environment names.

The boot host reuses selected existing sources under profiles/mhp2g, retaining
their internal namespace, without modifying them. Host-only adapters disable
persistent settings, native archive mods, pacing accelerators and GPU/performance
observers. Guest FreedomDX code can still perform its own file accesses. Module
loading, display, audio, networking and savedata dialogs remain unimplemented
unless explicitly listed by an HLE registration; reaching one stops the run.

Synthetic tests verify real allocation/free, memory-stick reads, scheduler delay
and return across a thread switch, execution of a changed instruction, explicit
failure of an unknown import, dispatch bounds and a one-second wall-clock bound.
Independent scenarios run in separate processes because the shared kernel is a
process-scoped service. Run the process-runner tests separately with
`python3 profiles/mhp1/tests/runner_tests.py` (about six seconds).

## Corpus/link preflight milestone

`PokeFreedomDXProbe` is a native executable that links the locally generated
corpus, verifies and loads the target ELF, checks its module and overlay section
addresses, and enumerates imports. It deliberately executes no guest code.
It has no PSP kernel/HLE, window, audio, ISO installer, settings or save handling.
A PASS means that the ELF/registry preflight succeeded, not that the game boots.
No MHP2G host code or address hooks are enabled and existing profiles are untouched.

Identity/layout facts come from the user's analyzer report and ELF section map.
Configuration byte 0x13 is identified from the Japanese FreedomDX MapScale reader
in https://github.com/SilverJolteon/FreedomDX. No FreedomDX implementation code
is incorporated. The configuration label does not establish the patch release.

## Build and run on macOS or Linux

Run from the checkout root. Keep your existing 44 generated units under
`profiles/mhp1/generated/` and the matching EBOOT under
`profiles/mhp1/game/EBOOT.ELF`. This does not regenerate or modify them.

```sh
cmake -S . -B out/mhp1-probe \
  -DPSPRECOMP_PROFILE=mhp1 \
  -DMHP1_BUILD_PROBE=ON \
  -DCMAKE_BUILD_TYPE=Debug \
  -DPSPRECOMP_GENERATED_JOBS=2
cmake --build out/mhp1-probe --target PokeFreedomDXProbe mhp1_target_tests --parallel 2
ctest --test-dir out/mhp1-probe -R '^mhp1_target_tests$' --output-on-failure
./out/mhp1-probe/bin/PokeFreedomDXProbe profiles/mhp1/game/EBOOT.ELF \
  > out/mhp1-probe/probe.log 2>&1
```

The probe exits on its own. To inspect your configuration too, pass its local
path as a second argument (the download name does not matter):

```sh
./out/mhp1-probe/bin/PokeFreedomDXProbe profiles/mhp1/game/EBOOT.ELF \
  /path/to/FDXDAT/CONFIG.BIN > out/mhp1-probe/probe.log 2>&1
```

Success includes `[mhp1-probe] PASS`. A mismatch is a failure; there is no
hash-bypass option. An older corpus that happens to register the same entry
cannot be proven compatible by this check: use only the corpus generated from
this exact executable. Generated units compile at -O0 for this diagnostic milestone.

For multi-config generators, pass `--config Debug` to build and `-C Debug` to
CTest; the executable may be under `bin/Debug/`.

## Framework tools without private game data

The probe is OFF by default, so tests and code-generation tools can build before
any private corpus exists:

```sh
cmake -S . -B out/mhp1-tools -DPSPRECOMP_PROFILE=mhp1 -DMHP1_BUILD_PROBE=OFF
cmake --build out/mhp1-tools --target psp_analyze psp_recomp mhp1_target_tests --parallel 2
ctest --test-dir out/mhp1-tools -R '^mhp1_target_tests$' --output-on-failure
```

## Verified section families, not a runtime overlay inventory

| Base | ELF section names | Count |
| --- | --- | ---: |
| 0x098D4700 | task overlays | 11 |
| 0x09A5A180 | demo_sub/game_sub | 2 |
| 0x09AF6900 | stage00 through stage91 | 92 |

These 105 sections have zero file payload. They do not establish the count of
actual archive images, their headers, their code/BSS sizes, the maximum loaded
end address, or an em/monster overlay family. Do not use the next section address
as a proven maximum payload size. The debug.elf section at 0x09AF7E80 is not
classified as a separate overlay slot by this milestone.

## Remaining implementation

1. Extend the bounded bootstrap with verified missing services until title and
   village initialization can run. Audit shared implementations as calls are reached.
2. Inspect the patched DATA.BIN format, implement bounded extraction, and verify
   overlay image headers/load addresses before defining a loader and AOT modules.
3. Use the interpreter baseline to verify FreedomDX's runtime instruction writes. AOT code does not
   automatically follow changes to guest memory; stale generated entries need an
   explicit invalidation/interpreter or equivalent verified implementation.
4. Bring up title, village, quest, saving/loading, then renderer/audio/input.
5. Adapt game-specific camera, HUD, map and texture behavior individually. Retain
   shared renderer fixes, without enabling MHP2G-specific classification/hooks.
6. Add separate application paths and macOS/Linux packaging after gameplay works.

The probe never opens an ISO, changes saves, installs FDXDAT or modifies CONFIG.BIN.
Keep all game data and generated sources out of commits and distributed packages.

### Baseline headless GE command execution milestone (before reference video)

The bootstrap now links the existing MIT-licensed MHP2G `gpu/ge_state.cpp`
command decoder without camera/HUD hooks or a renderer. It registers GE list
submission, FIFO ordering, stall updates, list/draw status polling and completion
checks, callback registration/removal and kernel interrupt delivery. GE block
transfers update guest RAM/VRAM. List state survives stall/resume.

This is command execution without rasterization: draws do not produce pixels,
there is no game window, and framebuffer presentation is not implemented here.
The decoder retains its existing limitations, including conditional bounding-box
branches and advanced SIGNAL behavior. EDRAM translation is configuration only.
Blocking sync on unfinished work, non-null enqueue options/context and head
insertion ahead of an active list stop with explicit diagnostics. Break/continue
and other unregistered services still stop as missing imports. Unknown commands,
unmapped list execution and exhausted command budgets also stop instead of
claiming list completion. This milestone does not establish visual correctness,
full PSP GE conformance or successful FreedomDX boot.

Synthetic tests cover completion, stalls, FIFO ordering, callback arguments and
return context, transfers into VRAM, invalid inputs and unsupported paths. Linux
native and UBSan runs are available; actual FreedomDX execution and macOS remain
manual validation using the user's private game data. Changed-line coverage has
not been measured for this milestone.


### Experimental reference video milestone

This patch adds a deliberately limited software rasterizer and BMP framebuffer
captures. It is a first-pixel diagnostic, not the existing Vulkan frontend or a
playable MHP1 port. Other profiles retain their existing renderer and features.

`MHP1_RENDER_DIR=/absolute/output/path` enables rendering and saves at most eight
captures per process, on GE draws, immediate framebuffer submissions and vblanks.
Use a new output directory per run to avoid mixing captures. No variable means
command execution without rasterization. `MHP1_STRICT_GE=1` disables the new state
extension and restores the earlier strict handling of unmodeled registers.

The extension retains additional graphics registers in the GE's raw register
bank, instead of treating these state writes as failed list execution. The
reference renderer reads that state for drawing-region clipping and write masks,
and rejects unsupported active pipelines. Zero writes to the observed legacy
0xF0..0xF9 initialization range are retained; nonzero writes still fail. Retaining
a register does not claim to implement its rendering feature. The default
behavior of the shared MHP2G decoder is unchanged when no extension is installed.

Supported: nonindexed, untextured through-mode sprites and constant-color
triangles; rectangular sprite clears of color/alpha/depth; screen/scissor/region
clipping for ordinary draws; color write masks; framebuffer formats RGB565,
RGBA5551, RGBA4444 and RGBA8888. The reference implementation is not pixel-accurate
PSP rasterization and is bounded by vertex/pixel budgets. Ordinary draws requiring
blending, alpha/depth tests, depth writes, lighting, fog, dithering, stencil,
antialiasing, color testing or logical operations stop explicitly. Textures and
transformed geometry also stop. Initialization alone does not produce a picture.

`sceDisplaySetFrameBuf` now records immediate/deferred buffer changes, including
blanking; deferred changes apply at vblank. Captures use guest framebuffer bytes,
so software GE writes, CPU writes and block transfers are visible together.
There is no window, controller input or hardware presentation in this milestone.
Audio, missing imports, overlays, EDRAM translation and the baseline decoder's
advanced control-flow limitations remain separate work.

Build `mhp1_video_tests` in addition to the bootstrap targets. Run its public
synthetic smoke scene without game data:

```sh
out/mhp1-headless/profiles/mhp1/mhp1_video_tests --capture out/mhp1-headless/smoke/reference.bmp
```

The resulting image contains a red rectangle and a green triangle on a dark
background. Format conversion fixtures are also written beside it. These are
synthetic tests, not evidence of FreedomDX rendering. Source facts for register
IDs and display formats: PSPSDK `src/gu/guInternal.h` and
`src/display/pspdisplay.h`; no external implementation was copied.


### Experimental Vulkan and boot-service integration

Configure `-DMHP1_VULKAN=ON -DMHP1_CODECS=ON` to build the existing SDL3/Vulkan
backend and FFmpeg music/movie services. These options require dependencies and
fail configuration when they are unavailable. `--renderer` on the diagnostic
runner enables the backend at runtime; without it the reference/headless path
remains available. The runtime flag is also available as `MHP1_RENDERER=1`.
The native backend submits decoded textured and transformed draws, presents guest
framebuffers, reads GPU data back before GE block transfers and provides real pad
input through the renderer. Main-memory movie presentation currently requires
RGBA8888. Display blanking uses a host upload and does not write guest VRAM.

This connects the existing renderer; it does not establish complete PSP GE
conformance or a completed FreedomDX boot. The shared decoder/backend retains
its existing limitations, including incompletely modeled retained registers,
advanced SIGNAL/control flow and curved surfaces. EDRAM address translation is
still configuration only. Head insertion into an active queue and GE enqueue
context options remain diagnostic stops. Blocking list/draw sync now waits on
real queue completion through the kernel scheduler instead of stopping.

Boot-service groups now include controller sampling/input, PSP audio/SAS mixing,
host fonts, savedata operations, platform state, virtual stock font/codec module
lifecycle, MPEG and (with FFmpeg) ATRAC. Stock-module virtualization accepts only
named HLE-supported libraries; unknown executable modules stop. Reverb inherits
the existing SAS limitation. Networking is not connected; the wireless switch is
off. The existing message/OSK dialogs, HTTP/SSL, some legacy SAS/audio/I/O exports
and other unresolved imports remain work. No unregistered call gets a generic
success fallback. Interpreted guest code can execute the game's own copied code;
AOT overlays and title-specific enhancements still need separate validation.

MHP2G's camera/HUD hooks, glyph-atlas reconstruction, texture-pack selection,
aspect patches and native mods are disabled in this MHP1 integration. The existing
profiles and their feature configurations are retained. Renderer caches use a
MHP1-local `renderer-data` directory beside the diagnostic memory-stick folder;
no existing application's settings are loaded. Controller reserved bytes are
zeroed for the original PSP ABI rather than used for an HD second stick.

The game runner writes `import-audit.csv` beside the executable's `bin` directory,
listing every actual executable import and whether it is registered. The same
registration audit can run without guest execution:

```sh
out/mhp1-native/bin/mhp1_import_audit profiles/mhp1/analysis/report_imports.csv out/mhp1-native/import-audit.csv
```

The supplied analysis metadata shows 166 of 274 imports registered with codecs
on. The remaining 108 include optional networking as well as unresolved legacy
services; they cannot all be assumed irrelevant to boot.

The game-free GPU smoke test is bounded to about three seconds, renders a
checkerboard sprite and colored transformed triangle, saves a BMP and checks
that it contains a varied image. It opens a real window and fails clearly if the
GPU/window cannot initialize. It is separate from automatic CPU tests:

```sh
out/mhp1-native/bin/mhp1_vulkan_smoke out/mhp1-native/gpu-smoke
```

For game runs, reuse the already prepared diagnostic memory stick so FreedomDX's
`PSP/SAVEDATA/FDXDAT` and its configuration remain available:

```sh
python3 profiles/mhp1/scripts/run_headless.py --renderer --build-dir out/mhp1-native --ms0 out/mhp1-headless/ms0 --capture-dir out/mhp1-native/captures-run1 --seconds 60 --iso /absolute/path/to/patched.iso
```

GPU capture mode samples at most once per 60 vblanks, up to eight captures, so an
initial black frame does not consume the entire capture budget immediately.
Use a new folder per run. A diagnostic stop remains exit 4; it is never labelled
a successful boot. Closing the window also ends the diagnostic. The external
runner timeout remains enabled. Save operations act only on the selected ms0.

Validation: Linux GCC builds with Vulkan on/off and FFmpeg on/off; all eight
native test suites passed in the renderer/codecs build; four runner process tests
passed; bootstrap/video/services/savedata suites passed under UBSan. New controller
regression fails against the previous bootstrap; a signature-only compatibility
shim was used to link its original GE implementation for that comparison. Focused
GCC line coverage in selected CPU production files was 366/424 (86.3%).
This is not branch/whole-project coverage and does not measure the compiled-out
Vulkan bridge. Native bridge execution, GPU presentation/readback, valid encoded
movie playback, macOS and actual FreedomDX boot remain manual validation gaps.
The local GPU smoke attempt stopped at SDL initialization because there was no
available video device. Source provenance: existing MIT project services/renderer
are reused; legacy I/O export names/IDs follow PSPSDK `IoFileMgrForUser.S`.


### Shared bundled FFmpeg build correction

MHP1_CODECS now uses the same `profiles/mhp2g/cmake/FFmpeg.cmake` helper as the
original MHP2G profile. Its default `MHP2G_FFMPEG=bundled` downloads the pinned,
SHA-256-checked source and builds the required LGPL decoders at configuration
time. It copies runtime libraries into the build's `bin/lib` directory and adds
runtime search paths to the final MHP1 executables. The previous MHP1-specific
system FFmpeg/pkg-config discovery is removed. `MHP2G_FFMPEG=system` remains an
explicit alternative through the shared helper; headers and libraries must all
match the executable architecture in that mode.

Use a fresh `out/mhp1-native-bundled` directory to avoid old Intel library paths
cached from the previous MHP1 configuration. On Apple Silicon run configuration
from an arm64 terminal (`uname -m` should print arm64). The first configure step
builds FFmpeg and can take time, exactly as in the original profile. Existing
MHP1 game data and diagnostic ms0 can be reused. No system codec installation or
removal, game-code generation or overlay rebuild is required by this correction.
The helper's `mhp2g` log prefix and `MHP2G_FFMPEG` option are inherited names; they
do not select MHP2G guest code or hooks.
