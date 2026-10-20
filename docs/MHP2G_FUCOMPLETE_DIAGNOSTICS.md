# FUComplete startup diagnostics, second pass

This is a diagnostic addition to the existing MHP2G profile, not a FUComplete
compatibility fix. It builds on the initial boot trace already present in the
supplied `hle_system.cpp`. Do not reapply that old patch.

## Purpose

Run the actual ELF and ISO in `mhfuc/`, identify the first observable unsupported
execution/loading operation, and compare later with a known-working English patch.
No separate profile or duplicate overlay corpus is introduced at this stage.
Existing executable-hash validation, HLE results, dispatch, overlay installation and
normal draw behavior are retained. Debug output is opt-in.

A separate game variant/code corpus can be added once changed functions and overlay
identities are established. Giving an unknown ELF an accepted hash alone does not
supply native implementations for new or changed instructions.

## Apply and build

From the repository root after downloading the patch:

```sh
git apply --check ~/Downloads/mhp2g-fuc-diagnostics-v2.patch &&
git apply ~/Downloads/mhp2g-fuc-diagnostics-v2.patch &&
cmake --build out/mhp2g-renderer --target Poke --parallel 4 &&
python3 profiles/mhp2g/tools/trace_fuc_startup.py
```

Use the existing configured build directory. If yours differs:

```sh
python3 profiles/mhp2g/tools/trace_fuc_startup.py --build-dir out/mhp2g
```

Rebuild Poke in that directory first. No overlay regeneration is required merely
to add tracing. No CMake modification is included in this patch.

## Inputs and isolation

The default input is the repository's `mhfuc/EBOOT.ELF` and `mhfuc/disc.iso`.
They can be symlinks. The script rejects missing links and an executable without
ELF magic. That check does not establish executable compatibility or prove that
the ISO and ELF are a matching pair.

The script creates a separate diagnostic game directory with links to those two
inputs, an empty/separate `ms0/`, and a separate host `--data-dir`. This is necessary:
in this profile, an explicit game directory can route saves to its own `ms0`,
independently of `--data-dir`. Normal per-user settings and existing game saves are
not selected for this run. Existing staging links are reused only if they still
point to the requested inputs; conflicting staging files/links cause an error.

No original ISO/ELF bytes are changed. Diagnostics remain in `out/fuc-diagnostics/`.
They are local development output and should not be committed. The staged memory
stick initially lacks any existing FUCDAT configuration or external loader files;
the log must be interpreted as a clean-memory-stick startup. If a later experiment
needs such files, copy only the relevant files into the diagnostic memory stick.

Texture packs, GPU decoding and crisp text are temporarily disabled in the child
process. Unbounded old I/O/UI/pack tracing and overlay dumping are disabled. Strict
HLE is disabled so existing logging stubs still expose the subsequent path. These
choices do not overwrite the normal saved settings. A pre-existing
`MHP2G_OVERLAY_DIR` override is retained and recorded in the log.

## Duration and result

The default run lasts at most 15 seconds before the runner requests termination,
then escalates if necessary. It can finish earlier if the game exits. The timeout
is intentional; it is not itself proof of a game crash. A runner trailer records
exit status, elapsed time and whether timeout/interruption occurred.

```sh
python3 profiles/mhp2g/tools/trace_fuc_startup.py --seconds 30
```

Attach **`out/fuc-diagnostics/startup.log`**. Include the FUComplete release/version
and whether the screen stayed black, displayed a logo, or changed before stopping.
No ISO or full executable needs to be attached.

The same script can produce a comparison log from the working game folder:

```sh
python3 profiles/mhp2g/tools/trace_fuc_startup.py \
  --game-dir game-mhp2ge --output out/english-diagnostics/startup.log
```

This also uses a separate clean memory stick and fresh host settings. Use the actual
working game folder name if different.

## What the patch records

All new entries start with `[fuc-trace #N category]`; numbering orders emitted
records across categories. Categories have separate limits, so repeated I/O cannot
consume the budget for the first dispatch failure. Payloads are constructed only
when tracing is enabled and a category has budget left.

| Category | Bound | Contents |
| --- | --- | --- |
| profile | 16 | Loaded entry, GP, load base/end, resolved roots, ISO/DATA.BIN sizes and LBA. |
| stub | 96 | First eight calls per logging stub, registers, caller code and existing result 0. |
| io | 192 | Opens, resolved byte/sector offsets, reads before allocation and after completion, seeks, stat/devctl calls. |
| module | 48 | Module header bytes and operations acknowledged without executable loading. |
| code | 96 | Dispatch misses/unsupported operations, PC, registers and nearby guest instructions. |
| icache | 32 | Invalidation arguments and mapped words near the requested address. |
| overlay | 32 | Unmatched overlay identity and up to four same-slot corpus hash comparisons. |
| validation | 32 | Installed overlay code identity, including a header-same/code-different audit. |

Dispatch observations deduplicate PC/reason pairs and keep at most 64 pairs.
Memory probes read at most 16 words per window, check mapping before each load,
and avoid wrapping at the 32-bit address-space boundary. Registers include v0,
a0–a3, t9, gp, sp and ra. File-read samples contain at most the first 16 bytes.
The existing first-pass boot trace retains its own smaller limits. Other normal
application messages are not globally capped by this helper.

The profile also observes the previously reported address `0x089B4384` in loaded
memory. It is not a patch target and is not assumed to be valid in every version.

## Interpretation

- `read-request` with no completed read can expose a very large allocation or an
  operation that fails before returning; it is a lead, not proof of an OOM.
- Failed `disc0:`/FUC opens point to file/layout or required startup assets.
- A code miss with meaningful instructions outside the existing native corpus
  points toward missing generation/registration or dynamically loaded code.
- A native-code/body change without a new dispatch miss can still be incompatible;
  boundary tracing does not observe every instruction inside already generated code.
- `acknowledged-only=1` matters if the requested module supplies actual game/mod code;
  it can be expected for stock modules whose exports are provided by HLE.
- `header-same=1 code-match=0` identifies a possible blind spot in the current
  overlay validation shortcut. The patch observes this; it does not change policy.
- A stub's result 0 is inherited behavior, not evidence that the requested PSP
  service has been implemented correctly.

We still need the matching FUComplete patch/modloader sources and exact release
before implementing version-specific bootstrap or executable-generation changes.
If the hang never reaches these boundaries, the next step is runtime dispatch or
scheduler-level instrumentation, requiring those current runtime/kernel sources.

## Local verification

The patch was checked against the four supplied source snapshots. The shared trace
helper was compiled with strict warnings and tested with tracing disabled/enabled:
category bounds across different callbacks, disabled callback suppression, mapped
reads, high/low address boundaries and duplicate-PC suppression. Synthetic runner
tests cover input validation, staging, environment overrides, log metadata, timeout
termination and ordinary child failure. No full Poke build or FUComplete run was
possible in the patch-preparation environment.

Optional local helper check (no game data or GPU required):

```sh
c++ -std=c++20 -Wall -Wextra -Werror -pthread \
  -Iprofiles/mhp2g/host profiles/mhp2g/tests/boot_trace_tests.cpp \
  -o /tmp/mhp2g-boot-trace-tests &&
env -u MHP2G_TRACE_BOOT /tmp/mhp2g-boot-trace-tests &&
MHP2G_TRACE_BOOT=1 /tmp/mhp2g-boot-trace-tests
```
