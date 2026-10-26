#!/usr/bin/env python3
"""Launch the existing MHP1 host with explicit native execution settings."""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]


def absolute(path):
    path = path.expanduser()
    return (path if path.is_absolute() else ROOT / path).resolve()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--iso', type=Path, required=True)
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--data-dir', type=Path, required=True,
                        help='Existing MHP1 data root; passes <data-dir>/ms0 to the host. Does not migrate saves.')
    parser.add_argument('--elf', type=Path, default=Path('profiles/mhp1/game/EBOOT.ELF'))
    parser.add_argument('--overlay-dir', type=Path)
    parser.add_argument('--log', type=Path)
    parser.add_argument('--seconds', type=int, default=0)
    parser.add_argument('--gpu-decode', choices=['on', 'off'], default='on')
    parser.add_argument('--fast-loading', choices=['on', 'off'], default='on')
    parser.add_argument('--perf', choices=['off', 'overlay', 'log', 'overlay+log'], default='overlay+log')
    parser.add_argument('--vulkan-icd', type=Path, help='Optional known-working ICD manifest; never guessed')
    parser.add_argument('--dry-run', action='store_true', help='Validate files and print without launching')
    args = parser.parse_args()
    if args.seconds < 0:
        parser.error('--seconds must be non-negative')
    build, iso, elf, data = map(absolute, [args.build_dir, args.iso, args.elf, args.data_dir])
    overlays = absolute(args.overlay_dir) if args.overlay_dir else build / 'bin/overlays'
    log = absolute(args.log) if args.log else build / 'boot.log'
    executable = build / 'bin/PokeFreedomDXBoot'
    for path in [executable, iso, elf]:
        if not path.is_file():
            parser.error(f'Missing file: {path}')
    if not os.access(executable, os.X_OK):
        parser.error(f'Executable permission missing: {executable}')
    if data.exists() and not data.is_dir():
        parser.error(f'Not a data directory: {data}')
    cache = build / 'CMakeCache.txt'
    if not cache.is_file():
        parser.error(f'Missing build cache: {cache}')
    entries = {}
    for line in cache.read_text().splitlines():
        if '=' in line and ':' in line and not line.startswith(('//', '#')):
            key, value = line.split('=', 1)
            entries[key.split(':', 1)[0]] = value
    if entries.get('PSPRECOMP_PROFILE') != 'mhp1':
        parser.error('Build cache must select PSPRECOMP_PROFILE=mhp1')
    if entries.get('MHP1_BUILD_NATIVE_MAIN', '').upper() not in ('ON', 'TRUE', '1'):
        parser.error('Native main is not enabled in this cache; configure MHP1_BUILD_NATIVE_MAIN=ON')
    libraries = sorted(p for p in overlays.glob('*') if p.is_file() and p.suffix in ('.so', '.dylib', '.dll'))
    if not libraries:
        parser.error(f'No overlay libraries in {overlays}; build the native overlay targets first')
    env = os.environ.copy()
    # Preserve user settings such as font paths and Vulkan driver selection.
    # Remove temporary tracing/checks that previously distorted performance.
    for key in list(env):
        if key.startswith(('MHP2G_TRACE_', 'MHP2G_CHECK_')) or key in {
            'MHP1_INTERPOLATION_DIAGNOSTIC', 'MHP1_INTERPOLATION_PARTICLES',
            'MHP1_CHECK_TRANSFORM_REPLAY', 'MHP1_RENDER_DIR', 'MHP1_DUMP_OVERLAYS',
            'MHP2G_SCREENSHOT_DIR', 'PSPRECOMP_INTERPRETER_VERBOSE',
            'PSPRECOMP_NO_INTERPRETER', 'PSPRECOMP_INTERPRETER_BUDGET',
            'MHP2G_INTERPOLATION_NO_MOTION_GUARD',
            'MHP2G_INTERPOLATION_NO_NEAREST_INSTANCES',
            'MHP2G_INTERPOLATION_NO_FLIPBOOK_GUARD',
            'MHP2G_INTERPOLATION_EXTRA_MS',
        }:
            env.pop(key, None)
    env.update({
        'MHP1_RENDERER': '1', 'MHP1_NATIVE_MAIN': '1', 'MHP1_NATIVE_OVERLAYS': '1',
        'MHP1_OVERLAY_DIR': str(overlays), 'MHP1_PRESENT_ON_FLIP': '1',
        'MHP2G_GPU_DECODE': '1' if args.gpu_decode == 'on' else '0',
        'MHP2G_FAST_LOADING': '1' if args.fast_loading == 'on' else '0',
        'MHP2G_PERF': args.perf,
        # Known interpolation defects: keep the release baseline at native pictures.
        'MHP2G_FRAME_RATE': '30',
    })
    if args.vulkan_icd:
        icd = absolute(args.vulkan_icd)
        if not icd.is_file():
            parser.error(f'Missing Vulkan ICD manifest: {icd}')
        env.update({'VK_DRIVER_FILES': str(icd), 'VK_ICD_FILENAMES': str(icd)})
    command = [sys.executable, str(ROOT / 'profiles/mhp1/scripts/run_headless.py'),
               '--renderer', '--build-dir', str(build), '--elf', str(elf),
               '--iso', str(iso), '--ms0', str(data / 'ms0'), '--log', str(log),
               '--seconds', str(args.seconds)]
    print(f'Executable: {executable}\nISO: {iso}\nMemory stick: {data / "ms0"}\n'
          f'Overlay libraries: {len(libraries)} in {overlays}\nLog: {log}', flush=True)
    print('Native main/overlays ON; interpolation OFF; GPU decode ' + args.gpu_decode, flush=True)
    for key in ('VK_DRIVER_FILES', 'VK_ICD_FILENAMES', 'MVK_CONFIG_API_VERSION'):
        if key in env:
            print(f'{key}={env[key]}', flush=True)
    print('+ ' + shlex.join(command), flush=True)
    if args.dry_run:
        return 0
    return subprocess.run(command, cwd=ROOT, env=env, check=False).returncode


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except OSError as error:
        print(f'MHP1 launch failed: {error}', file=sys.stderr)
        raise SystemExit(1)
