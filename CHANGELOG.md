# Changelog

This changelog covers the Poke MHP2G profile work recorded during development.
It does not replace the upstream Poke/Yakumo release history.

Versions use MAJOR.MINOR.PATCH. Changes remain under Unreleased until a
release is tagged; release entries should then include their tag and date.
The proposed first documented checkpoint is 0.1.0. No historical tags,
release dates, or commit identifiers are inferred here.

## Unreleased — planned 0.1.0

### FUComplete development checkpoint — 2026-10-18

- Add selectable `MHP2G_GENERATED_DIR` and optional compiled ELF SHA-256 provenance.
  Preserve the stock profile corpus default while generating FUComplete main code
  independently under `out/mhfuc/generated`.
- Add a pinned FUComplete ELF build helper with archive NOP-site validation,
  generator/source fingerprint reuse, guarded output ownership and corpus backups.
  Bundle KTX2 by default instead of disabling it for the old startup experiment.
- Add a normal FUComplete launcher with separate game/save, settings and overlay
  paths, and a macOS command launcher without personal absolute paths.
- Recognize the wider idle and L-held FUComplete item-selector layouts, preserving
  the existing HUD anchor and conservative menu exclusions.
- Correct the one-texel V mismatch for the exact sharpness draw on an uploaded
  `ddeab148` replacement. Leave native texture sampling and geometry unchanged.
- Remove temporary startup, HUD-classification and sharpness comparison probes;
  retain functional source changes and game-data-free regression tests.
- Document setup, builds, overlay limitations, settings, saves and verification.
  Normal save/write and subsequent load succeeded in the developer's macOS log.
  FUComplete Steam Deck/Linux verification and complete feature coverage remain open.

### Development checkpoint — 2026-10-18
- Fixed save corruption issues in Fu Complete
- Fix various UI bug on scaling options, including weapons special gauges and palico infos
- Increase the default renderer vertex arena from 64 to 128 MiB per
  frame region, preventing observed missing character and HUD draws
  in Mountain area 4. Preserve diagnostic capacity overrides.
- Added a FOV slider by 5 percent increments
- Add Video > HUD size: Original (100%), Medium (75%), and Small (50%).
  Scale recognized normal quest HUD groups about their corner anchors without
  changing texture enhancement. Preserve sampling coordinates and transform
  clipping alongside placement; broader HUD layouts remain pending testing.
- Add selective MMPX upscaling for quest maps using the observed 256x256,
  swizzled CLUT8, stride-160 VRAM layout. Match the layout rather than one
  area's texture hash, and use enhanced sampling only after its copy is ready.
  This is MMPX enhancement of original artwork, not an HD map replacement.
- Add saved Video controls for selective UI upscaling, quest map upscaling,
  and GPU vertex decoding. Keep the existing crisp-text and texture-pack
  controls; changes are saved immediately to the active settings.ini.
- Enable crisp text, selective UI, map enhancement, and texture packs for
  new or unspecified choices. Keep global UI enhancement and GPU decoding
  off by default. Preserve existing explicit settings, including crisp text Off.
- Advance the settings schema to version 3 and retain environment variables
  as optional, temporary overrides rather than required launch configuration.
- Detect packs in textures/ULJM05500 and also directly in textures when it
  contains textures.ini. Saved pack-folder choices take precedence; a valid
  game-specific textures.ini takes precedence over the flat-layout fallback.
- Remember a supported explicit game-folder launch for subsequent launches
  without a game-directory argument. Preserve that folder's own save location;
  no saves are copied or merged by this feature.
- Change the MHP2G per-user organization from Yakumo to Poke. The normal macOS
  root becomes ~/Library/Application Support/Poke/MHP2G; Linux follows the
  platform's per-user directory under Poke/MHP2G. MHP3rd paths are unchanged.
  Existing MHP2G data was copied manually; automatic migration is not implemented.
- Add documentation for persistent Video settings and Steam Deck building/testing.
- Confirm the simpler English-patched ISO reaches gameplay with the supported
  original executable and existing overlays in the user's test. This does not
  establish compatibility with every translation or with FUComplete.

### Task status for this checkpoint

- [x] Selective quest-map enhancement: working in the user's test.
- [x] Persistent Video options: working in the user's test; the previous saved
  crisp-text Off choice was preserved and then enabled through the menu.
- [x] Settings defaults/round-trip checks and pack-discovery checks passed in
  standalone validation; full platform application testing remains separate.
- [x] MHP2G data-folder rename to Poke and manual copy instructions provided.
- [x] Earlier review fixes: guard the direct-font hook for headless builds,
  remove the duplicate vertex UV assignment, and use game/out/mhp2g defaults.
- [ ] Test the complete current checkpoint on Steam Deck, including saved
  options, copied user data, argument-free startup, and multiple quest maps.
- [x] Add saved quest HUD sizing in Video for macOS and Linux; recognition of
  additional HUD layouts remains a follow-up task.
- [ ] Package a clickable macOS app and a Linux/Steam Deck distribution
  (Flatpak planned), including runtime libraries and required notices.
- [x] Establish an isolated, pinned FUComplete native main build with working
  startup, tested UI fixes and normal save/load on macOS.
- [ ] Validate the FUComplete branch on Steam Deck/Linux, compile changed overlays,
  and audit remaining FUComplete feature/import and archive-mod compatibility.
- [ ] Add version display in menus/logs later; this checkpoint adds no such UI.


### Added

- Reconstruct directly written 256 × 256 CLUT4 font pages from HLE glyph
  positions, rendering known glyphs at up to four times native resolution.
- Preserve unknown font-page content at native resolution and validate guest
  pixels before reconstructing a page. Invalidate recordings after atlas
  changes, reuse, or font-generation changes.
- Add selective UI enhancement for recognized item, equipment, and menu
  sheets. Limit enhancement of the mixed HUD sheet to selected icon regions.
- Support PPSSPP texture packs using legacy `hash = quick` keys, including
  guest-address alignment behavior and data-only wildcard mappings.
- Decode KTX2 texture replacements through Khronos libktx, including Basis
  ETC1S/UASTC LDR transcoding, Zstandard storage, RGBA8/BGRA8 images,
  orientation handling, and conversion from premultiplied to straight alpha.
- Add optional KTX2 build modes: `OFF`, `bundled`, and `system`. Bundled mode
  uses pinned KTX-Software 4.4.2 sources with archive checksum verification
  and copies dependency notices beside build outputs.
- Add synthetic codec/hash tests and optional decoding of a user-owned sample.
- Document texture-pack building, installation, diagnostics, supported formats,
  dependency provenance, and fallback behavior in `docs/MHP2G_TEXTURE_PACKS.md`.

### Changed

- Accept UTF-8 BOM-prefixed texture-pack INI files and recognize KTX2 filenames.
- Display the selected pack hash algorithm, including `quick`, in pack review.
- Sample successfully reconstructed text, selected enhanced UI, and loaded HD
  UI replacements at their enhanced resolution.
- Sample ordinary through-mode UI on the native PSP pixel grid at higher
  internal resolutions to reduce atlas outlines and internal seams.
- Include direct font-page revisions in enhanced texture cache keys.
- Add bounded UI gate diagnostics through `MHP2G_TRACE_UI`; retain texture-pack
  matching, decoding, and upload diagnostics through `MHP2G_TRACE_TEXTURE_PACK`.

### Fixed

- Honor the guest depth-test enable state for through-mode draws. This corrects
  the observed smith weapon-preview occlusion without globally disabling
  geometry depth behavior.
- Require actual reconstructed glyphs before accepting an enhanced legacy
  glyph page.
- Resolve CMake from `PATH` in the overlay build invocation and supply Ninja's
  numeric keep-going argument (`-k 0`).

### Configuration and compatibility

- Enable crisp text with `MHP2G_CRISP_TEXT=1`; selective UI enhancement uses
  `MHP2G_SELECTIVE_UI=1`.
- `MHP2G_UI_TEXTURES=off` disables global UI enhancement while the explicit
  crisp-text and selective paths remain available.
- Configure `-DMHP2G_KTX2=bundled` or `system` to decode KTX2. The default is
  `OFF`; PNG support remains available.
- Point `MHP2G_TEXTURE_PACK` at the directory containing `textures.ini` and
  its referenced assets. Preserve the pack's internal directory layout.
- Unsupported or failed replacements fall back to original game textures;
  originals also remain visible while replacements load asynchronously.
- Standard documentation/layout target: `game/` for user-supplied game data
  and `out/mhp2g/` for build and runtime outputs. Existing configured build
  directories must be reused or freshly configured, rather than renamed.
  Overlay-script defaults now use this layout.

### Validation and remaining checks

- macOS gameplay testing confirmed crisp text and successful replacement of
  HUD, character, equipment, scenery, menu, and icon assets after missing pack
  files were restored.
- Standalone synthetic codec/hash tests passed with KTX2 enabled and disabled.
  The decoder and bundled integration were previously exercised locally; this
  is not a full-project Linux or macOS build verification.
- Steam Deck build, controller use, memory/performance behavior, and 30/60 FPS
  regression testing remain pending for this checkpoint.
- Guard the renderer-only direct-font hook for headless builds, remove the
  duplicate vertex UV assignment, and align overlay-script defaults with
  the documented layout.
- Review visual behavior at native and higher internal resolutions, with and
  without replacements, including house/chest menus and smith weapon previews.

### External asset adjustment

- A replacement HUD sharpness blade was observed to begin one PSP pixel above
  the original artwork. The user adjusted the pack artwork and confirmed the
  conversion worked. This is a local texture-pack edit, not a renderer change
  or bundled project asset; other sharpness states still need checking.
- Missing menu/background/icon replacements were traced to absent pack files
  and resolved by restoring the assets, rather than changing texture matching.

## Earlier development — unversioned

These entries record earlier work discussed before this checkpoint. Exact
commit membership and dates require the repository's Git history. They do not
assert that every item is already committed or introduce retrospective versions.

### Renderer and gameplay

- Correct expanded vertex-buffer alignment to four-byte boundaries after raw
  GPU vertex data, resolving the observed HUD corruption.
- Exclude skinned draws from interpolation camera estimation to address the
  observed house-scene wobble at 60 FPS.
- Establish opening playback with sound and working movement at 60 FPS, as
  reported during development; broader scene coverage remains unverified.

### Build and documentation

- Prepare Linux/macOS build instructions covering Poke, game inputs,
  dependencies, renderer configuration, overlays, and execution.
- Define the intended `game/` and `out/mhp2g/` layout to replace development
  names such as `mhp2g-test-game` and `out/mhp2g-renderer`.
- Diagnose the Ninja keep-going error and use a numeric argument.

### Existing input behavior verified

- Confirm Xbox right-stick camera control works through the existing D-pad
  right-stick behavior setting; no new input implementation was required.

## Deferred

- Displaying a project version in game menus or startup logs.
- Automatic changelog generation from Git history or release automation.
