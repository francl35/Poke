# Persistent MHP2G video settings

Video settings now retain the selectively enhanced UI configuration without a
launch script. Changes apply during play and are saved in `settings.ini` in
the active data directory. System > Open the data folder reveals that location.

## Defaults

| Video control | New/unspecified choice | Settings key |
| --- | --- | --- |
| Crisp text (in the font section) | On | `text.crisp` |
| Selective UI upscaling | On | `video.selective_ui` |
| Quest map upscaling | On | `video.upscale_maps` |
| Global UI enhancement | Off | `video.ui_textures` |
| Texture pack | On | `video.texture_pack` |
| GPU vertex decoding | Off | `video.gpu_decode` |

Internal resolution remains the player's existing choice (2x for a fresh
configuration). Enhanced text/maps/UI operate above 1x, at up to 4x. Map
selection recognizes the observed 256x256 swizzled CLUT8 stride-160 VRAM layout;
it is not a guarantee that every possible map layout has been covered.

CPU vertex decoding remains the compatibility default because GPU decoding
previously caused a radv GPU timeout on Deck. A player can enable GPU vertex
decoding in Video on a tested device. Hardware availability and diagnostic
paths may still require CPU decoding even when the saved choice is On.

Global enhancement stays Off to preserve native sampling for ordinary HUD
panels and bars. The selective controls are independent of that global choice.
Explicit pack replacements take priority over reconstructed/upscaled originals.
Copies are produced asynchronously; originals remain visible while pending.

Existing explicit values, including version-2 `text.crisp=0`, are preserved.
Use the individual controls to enable enhancements in an older installation.
Restore video defaults restores the new choices too, including crisp text,
while retaining environment overrides. It also resets the other Video options.
Settings schema version 3 adds the new persistent choices. The earlier
pre-version-2 crisp-text migration continues to discard that legacy default.

## Automatic texture-pack discovery

With Texture pack On and no explicit folder selection, discovery checks:

1. `<data directory>/textures/ULJM05500/` (preferred).
2. `<data directory>/textures/` if it directly contains `textures.ini` and the
   preferred folder has no `textures.ini`.

Keep `textures.ini` and its referenced directories together. A saved pack folder
chosen through Video > Import texture pack takes precedence over these defaults.
The existing import screen can use a pack in place without copying it.

Pack loading is checked at startup. After adding/replacing files during play,
restart Poke or use the existing texture-pack controls to reload it. Missing
replacement files cannot be supplied by a renderer setting.

On macOS the normal data folder is typically:
`~/Library/Application Support/Poke/MHP2G/`.
On Linux or inside Flatpak, use System > Open the data folder to find the active
location rather than assuming it matches the macOS or host-container path.

## Launching without arguments

A supported explicit-directory launch with a readable disc image remembers
its absolute game directory in `startup.game_directory`, after ELF/profile
setup. For example, run once:

```bash
./out/mhp2g-renderer/bin/Poke "$PWD/game-mhp2ge"
```

Later starts can omit the game directory:

```bash
./out/mhp2g-renderer/bin/Poke
```

The remembered folder keeps its own `ms0` save location; this does not copy or
merge saves. Explicit game arguments and `MHP2G_GAME_DIR` still take priority.
If the saved directory is missing its ELF or ISO, normal installation/developer
fallbacks apply. `--install` bypasses the remembered folder; after successful
explicit installation that preference is cleared. Unsupported executable hashes
are not remembered automatically.

This is startup behavior, not a new installer or executable package. It does
not create a macOS app bundle, Linux desktop entry, Flatpak, or bundled graphics
libraries. Existing no-argument installation behavior is otherwise retained.
No shell working directory should be needed once the remembered path is saved.

## Environment overrides

Existing environment variables remain available for development. They override
the saved choice for that run, disable its menu control, and are not persisted
as the player's choice:

- `MHP2G_CRISP_TEXT`
- `MHP2G_SELECTIVE_UI`
- `MHP2G_UPSCALE_MAPS`
- `MHP2G_GPU_DECODE`
- `MHP2G_UI_TEXTURES`
- `MHP2G_TEXTURE_PACK`

Unset those variables when testing the menu. Diagnostic tracing remains Off
unless explicitly requested. Recompiling is unnecessary for future setting
changes. Installing a different executable on another machine still requires
that platform's build/runtime dependencies and accessible game data.

## Existing Yakumo MHP2G data

The MHP2G organization name is now Poke. MHP3rd's Yakumo data is unchanged.
With Poke closed, copy only Yakumo/MHP2G to Poke/MHP2G before first launch if
settings, fonts, textures, caches, or installed data should be retained.
There is no automatic migration. Preserve the source until the new location
is verified. Do not merge over a newer destination without reviewing conflicts.
Check saved absolute pack/font/game paths after copying; their strings are not
rewritten automatically. Portable and explicit data-directory overrides remain
in effect and may intentionally use a different root.
