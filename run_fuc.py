#!/usr/bin/env python3
"""Launch the isolated FUComplete instance. Installed at its checkout root."""
import os
from pathlib import Path
import sys


def main():
    root = Path(__file__).resolve().parent
    binary = root / 'out/mhfuc/bin/Poke'
    game = root / 'mhfuc'
    data = root / 'out/fuc-user-data'
    overlays = root / 'out/mhfuc/bin/overlays'
    if len(sys.argv) != 1:
        raise SystemExit('Usage: python3 run_fuc.py (game and data paths are fixed to this workspace)')
    if not binary.is_file() or not (game / 'EBOOT.ELF').is_file() or not (game / 'disc.iso').is_file():
        raise SystemExit('Build the isolated FUComplete workspace first; executable or game inputs are missing.')
    data.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    # Normal launches use this instance's saved menu settings. The startup
    # probe supplied temporary environment overrides; do not inherit them.
    quality_overrides = {
        'MHP2G_DUMP_OVERLAYS', 'MHP2G_STRICT_HLE', 'MHP2G_GPU_DECODE',
        'MHP2G_CRISP_TEXT', 'MHP2G_SELECTIVE_UI', 'MHP2G_UPSCALE_MAPS',
        'MHP2G_UI_TEXTURES', 'MHP2G_TEXTURE_PACK',
    }
    for key in list(env):
        if key.startswith('MHP2G_TRACE_') or key in quality_overrides:
            env.pop(key, None)
    env['MHP2G_OVERLAY_DIR'] = str(overlays)
    print(f'[fuc-workspace] settings/data: {data}\n[fuc-workspace] game/saves: {game}', flush=True)
    os.execve(str(binary), [str(binary), '--data-dir', str(data), str(game)], env)


if __name__ == '__main__':
    main()
