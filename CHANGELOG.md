# Changelog

This changelog covers the Poke MHP2G profile work recorded during development.
It does not replace the upstream Poke/Yakumo release history.

Versions use MAJOR.MINOR.PATCH. Changes remain under Unreleased until a
release is tagged; release entries should then include their tag and date.
The proposed first documented checkpoint is 0.1.0. No historical tags,
release dates, or commit identifiers are inferred here.

## Unreleased — planned 0.1.0

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
