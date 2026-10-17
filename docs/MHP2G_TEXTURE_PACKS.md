# MHP2G texture packs: PNG, KTX2 and legacy quick keys

This extension loads the supplied pack without changing its `hash = quick`
keys or converting its KTX2 images. It is layered over the working MHP2G
text reconstruction and selective UI patches.

## Build

Keep the current build directory and its existing configuration. With CMake
3.22 or newer, from the repository root:

```bash
cmake -S . -B out/mhp2g-renderer -DMHP2G_KTX2=bundled
cmake --build out/mhp2g-renderer --target Poke mhp2g_texture_pack_codec_tests --parallel 2
./out/mhp2g-renderer/bin/mhp2g_texture_pack_codec_tests
```

Use `out/mhp2g` instead if that is already your build directory. Do not move
or rename a configured CMake build tree. This feature does not require
regenerating the game's main code or overlays.

`bundled` fetches and verifies the unmodified KTX-Software v4.4.2 source archive
once (about 203 MiB compressed), then builds a static dependency in the build
tree. It includes Basis transcoding, Zstandard decompression and codec helpers;
it is a longer first build than a normal renderer edit. Later builds reuse it.
No new KTX dynamic library needs installing beside Poke in this mode.

| `MHP2G_KTX2` | Behavior |
| --- | --- |
| `OFF` (default) | PNG and all supported hash modes work; KTX2 files report how to enable decoding |
| `bundled` | Pinned, checksum-verified KTX dependency, built statically |
| `system` | Use an installed Ktx CMake package; supply its prefix with `CMAKE_PREFIX_PATH` or `Ktx_DIR` if needed |

Both Linux and macOS use the same switch. Retain the normal platform SDL/Vulkan
configuration already present in the build cache. System-library packages need
their own runtime deployment; bundled mode avoids that extra setup.

Offline/source-managed builds can set `FETCHCONTENT_SOURCE_DIR_POKE_KTX` to an
already extracted copy of the pinned source. The user-supplied source override
bypasses FetchContent's archive download/checksum, so its identity must be
verified separately. The normal bundled URL has its SHA-256 in `cmake/KTX2.cmake`.

## Install and run

Extract the pack, retaining its internal paths. Point to the directory that
directly contains `textures.ini`, not an outer archive or just the `ui` folder:

```text
my-pack/
    textures.ini
    areas/...
    ui/hud/4f3455d8.ktx2
    ...
```

```bash
MHP2G_GPU_DECODE=1 \
MHP2G_CRISP_TEXT=1 \
MHP2G_UI_TEXTURES=off \
MHP2G_SELECTIVE_UI=1 \
MHP2G_TEXTURE_PACK="/absolute/path/to/my-pack" \
MHP2G_TRACE_TEXTURE_PACK=1 \
./out/mhp2g-renderer/bin/Poke "$PWD/mhp2g-test-game" \
2>&1 | tee /tmp/poke-ktx2-pack.log
```

Use your actual game directory (`game` in the standardized layout). The older
paths above match the current tested checkout; the implementation does not
hardcode either game/build directory.

The texture-pack variable both selects the folder and enables replacements.
Trace messages distinguish a key not present in the pack, a matched file,
successful decoding, and a GPU upload. Omit the trace during normal play.
Original game textures are drawn while replacements load or if decoding fails.

To check the sample independently of gameplay:

```bash
./out/mhp2g-renderer/bin/mhp2g_texture_pack_codec_tests \
  "/absolute/path/to/my-pack/ui/hud/4f3455d8.ktx2"
```

Keep `hash = quick` unchanged. Changing it to `xxh64` does not convert the
pack's existing mappings. The parser accepts a UTF-8 BOM, CRLF line endings,
the pack's 24-digit keys and its data-only wildcard keys.

## Decoder scope

- Basis ETC1S/UASTC LDR KTX2 is transcoded to RGBA8 on the loader threads.
- Zstandard-supercompressed data is decompressed by the KTX dependency.
- Raw RGBA8 and BGRA8 KTX2 is accepted, including its sRGB format variants;
  encoded channel values are preserved as for existing PNG replacements.
- Orientation is converted to top-left, and premultiplied alpha to straight alpha.
- Arrays, cubemaps, 3D images, HDR and unsupported raw GPU block formats fall
  back with a diagnostic. Individual dimensions are limited to 8192 pixels.
- The base image goes through the existing Vulkan replacement uploader, which
  generates its own mip chain. Pack-supplied lower mip levels are not used.
- Matched HD UI replacements use continuous sampling of their higher-resolution
  image. Unreplaced panels keep the native PSP-grid path; reconstructed fonts
  and selectively upscaled icons keep their existing paths.

KTX2 is a container, not a guarantee of a particular codec. The supplied sample
is 128x64 UASTC LDR with Zstandard supercompression. Other files in the full
pack still need gameplay testing; decoding a file does not prove that its key
matches the game's corresponding texture.

## Legacy quick arithmetic

The implementation uses explicit little-endian reads and the guest address to
choose the legacy aligned path, so allocator/host-pointer alignment does not
change pack keys across machines.

For a 16-byte-aligned guest range whose size is a multiple of 64, treat each
64-byte block as four rows of four 32-bit words. Each output word is evaluated
as an independent recurrence. Its initial 16-bit weight pairs, packed low-half
first, are `9BD9C00B`, `B6514B73`, `43094D9B`, and `00010083`.

For a word lane in each block:

1. Add the first-row word times the weight to the accumulator, independently
   modulo 2^16 in each halfword.
2. XOR the second-row word, then add the third-row word modulo 2^32.
3. XOR the halfword products of the fourth-row word and the weight.
4. Increment both weight halves by `2455` modulo 2^16.

Add each lane's final weight word to its accumulator and sum the four words
modulo 2^32. The fallback adds then XORs each successive pair of 32-bit words,
starting at zero and ignoring a trailing incomplete eight-byte pair. Separate
rows are combined by the existing `hash = (hash * 11) XOR row_hash` rule.

Fixed vectors checked against the external format reference:

| Input | Size | Guest alignment | Result |
| --- | ---: | --- | --- |
| Empty | 0 | 16-byte aligned | `9535599c` |
| Zero bytes | 64 | 16-byte aligned | `2689eaf0` |
| Bytes 00..3f | 64 | 16-byte aligned | `db8be980` |
| Bytes 00..7f | 128 | 16-byte aligned | `d70047e4` |
| Bytes 00..1f | 32 | 16-byte aligned (fallback by size) | `00000000` |

These semantics were implemented independently; external reference source is
not embedded in the patch or its tests. See [SOURCE_PROVENANCE.md](SOURCE_PROVENANCE.md).

## Validation and troubleshooting

The patch adds public synthetic tests for quick hashes, malformed/oversized
KTX2, raw RGBA8, alpha, orientation and the disabled-decoder build. Existing
pack-import tests are updated to accept `quick`, recognize BOM-prefixed options,
and display the correct hash name. They use no game or pack artwork.

```bash
cmake --build out/mhp2g-renderer --target mhp2g_texture_pack_tests mhp2g_texture_pack_codec_tests --parallel 2
ctest --test-dir out/mhp2g-renderer -R '^mhp2g_texture_pack(_codec)?_tests$' --output-on-failure
```

| Symptom | Check |
| --- | --- |
| Decoder disabled | Reconfigure the same build tree with `-DMHP2G_KTX2=bundled` |
| Fetch/checksum failure | Keep the expected checksum; check network access or use a separately verified source override |
| All keys miss | Confirm the pack targets this game release and retains its original hash setting |
| File missing | Preserve the folders named in the INI; check that the selected directory contains it |
| Decode fails | Read the specific `[texpack] cannot use ...` message; KTX2 can contain unsupported formats |
| Compilation runs out of memory | Retry with `--parallel 1` |
| Increased GPU memory use | Set `MHP2G_TEXTURE_PACK_MEMORY` (MiB); active images can temporarily exceed the budget |

Keep the original pack unchanged for comparisons. Verify several areas, menu
transparency, equipment previews, 30/60 FPS and both host platforms before
treating the full pack as validated. Packaging must retain the copied KTX and
codec notices.

References: [PPSSPP texture replacement format](https://www.ppsspp.org/docs/reference/texture-replacement/),
[KTX-Software v4.4.2](https://github.com/KhronosGroup/KTX-Software/tree/v4.4.2),
[libktx reader API](https://github.khronos.org/KTX-Software/libktx/group__reader.html).
