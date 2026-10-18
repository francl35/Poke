#!/usr/bin/env python3
"""Generate and build the reviewed FUComplete ELF in a separate native build."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import shutil
import struct
import subprocess
import sys
import tempfile
import time

REVIEWED_ELF = '02811bb3a529d48d707072a726e87106d0ecdab62aa7e494b23ecf1469eb662f'
OWNER = 'poke-fuc-native-v1'


def digest(path):
    value = hashlib.sha256()
    with path.open('rb') as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b''):
            value.update(chunk)
    return value.hexdigest()


def read_cache(path):
    result = {}
    for line in path.read_text(encoding='utf-8', errors='replace').splitlines():
        if not line or line.startswith(('#', '//')) or '=' not in line:
            continue
        left, value = line.split('=', 1)
        key, separator, _kind = left.partition(':')
        if separator:
            result[key] = value
    return result


def source_snapshot(directory):
    return {p.relative_to(directory).as_posix(): digest(p)
            for p in sorted(directory.rglob('*'))
            if p.is_file() and p.suffix in ('.cpp', '.hpp', '.h', '.inc')}


def validate_elf(path):
    if digest(path) != REVIEWED_ELF:
        raise ValueError('This build is pinned to the inspected FUComplete ELF SHA-256 '
                         f'{REVIEWED_ELF}. Do not use the original BOOT.BIN or an unreviewed variant.')
    data = path.read_bytes()
    header = struct.unpack_from('<16sHHIIIIIHHHHHH', data)
    if header[0][:6] != b'\x7fELF\x01\x01' or header[1] != 2 or header[2] != 8:
        raise ValueError('Expected the reviewed ELF32 little-endian MIPS executable')
    for address in (0x0884D868, 0x0884D8B8, 0x0884E93C, 0x0884EE0C):
        found = False
        for index in range(header[10]):
            p = struct.unpack_from('<IIIIIIII', data, header[5] + index * header[9])
            delta = address - p[2]
            if p[0] == 1 and 0 <= delta and delta + 4 <= p[4]:
                word = struct.unpack_from('<I', data, p[1] + delta)[0]
                if word != 0:
                    raise ValueError(f'Expected FUComplete NOP at 0x{address:08X}; found 0x{word:08X}')
                found = True
                break
        if not found:
            raise ValueError(f'FUComplete archive patch site is not file-backed: 0x{address:08X}')


def run(command, root):
    print('\n+ ' + shlex.join([str(x) for x in command]), flush=True)
    subprocess.run([str(x) for x in command], cwd=root, check=True)


def main():
    root = Path(__file__).resolve().parents[3]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tools-build-dir', type=Path, default=Path('out/mhp2g-renderer'))
    parser.add_argument('--build-dir', type=Path, default=Path('out/mhfuc'))
    parser.add_argument('--game-dir', type=Path, default=Path('mhfuc'))
    parser.add_argument('--ktx2', choices=('bundled', 'OFF'), default='bundled')
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--generated-jobs', type=int, default=2)
    parser.add_argument('--regenerate', action='store_true')
    args = parser.parse_args()
    if args.jobs < 1 or args.generated_jobs < 1:
        parser.error('job counts must be positive')
    resolve = lambda p: (root / p).resolve()
    tools, build, game = map(resolve, (args.tools_build_dir, args.build_dir, args.game_dir))
    cmake = shutil.which('cmake')
    if cmake is None:
        parser.error('cmake was not found on PATH')
    if build == tools or not build.is_relative_to(root / 'out') or build == root / 'out':
        parser.error('--build-dir must be a separate directory below the repository out/ directory')
    reference_cache = tools / 'CMakeCache.txt'
    if not reference_cache.is_file():
        parser.error(f'configured reference build not found: {tools}; use --tools-build-dir')
    cache = read_cache(reference_cache)
    if Path(cache.get('CMAKE_HOME_DIRECTORY', '')).resolve() != root:
        parser.error('reference cache belongs to a different source directory; do not move/copy caches')
    elf = game / 'EBOOT.ELF'
    if not elf.is_file() or not (game / 'disc.iso').is_file():
        parser.error(f'{game} must contain readable EBOOT.ELF and disc.iso links/files')
    try:
        validate_elf(elf)
        marker = build / 'fuc-build-state.json'
        previous = {}
        if marker.is_file():
            previous = json.loads(marker.read_text())
            if previous.get('owner') != OWNER:
                raise ValueError('build-state marker is not owned by this FUComplete build')
        elif build.exists() and any(build.iterdir()):
            raise ValueError(f'{build} is not empty and has no FUComplete marker; choose a new build directory')
        build.mkdir(parents=True, exist_ok=True)
        marker.write_text(json.dumps({'owner': OWNER, **previous}, indent=2) + '\n')
        run([cmake, '--build', tools, '--target', 'psp_analyze', 'psp_recomp', '--parallel', args.jobs], root)
        analyzer, recompiler = tools / 'psp_analyze', tools / 'psp_recomp'
        if not analyzer.is_file() or not recompiler.is_file():
            raise ValueError('reference build did not produce psp_analyze and psp_recomp')
        fingerprint = {'elf_sha256': digest(elf), 'generator_sha256': digest(recompiler)}
        generated = build / 'generated'
        snapshot = source_snapshot(generated) if generated.is_dir() else {}
        reusable = (not args.regenerate and all(previous.get(k) == v for k, v in fingerprint.items())
                    and snapshot and snapshot == previous.get('sources')
                    and any(name.endswith('.cpp') for name in snapshot))
        if not reusable:
            analysis = build / 'analysis'
            analysis.mkdir(exist_ok=True)
            run([analyzer, elf, analysis / 'report.json'], root)
            pending = Path(tempfile.mkdtemp(prefix='generated-pending-', dir=build))
            run([recompiler, elf, '--auto', pending], root)
            snapshot = source_snapshot(pending)
            if not any(name.endswith('.cpp') for name in snapshot):
                raise ValueError(f'generator produced no C++ files; inspect {pending}')
            if generated.exists():
                backup = build / ('generated-backup-' + time.strftime('%Y%m%d-%H%M%S'))
                if backup.exists():
                    raise ValueError(f'backup already exists: {backup}; retry after a second')
                generated.rename(backup)
                print(f'Previous FUComplete corpus preserved at {backup}', flush=True)
            pending.rename(generated)
            marker.write_text(json.dumps({'owner': OWNER, **fingerprint, 'sources': snapshot}, indent=2) + '\n')
        else:
            print('FUComplete corpus matches the ELF, generator and recorded source hashes; reusing it.', flush=True)
        command = [cmake, '-S', root, '-B', build]
        generator = cache.get('CMAKE_GENERATOR')
        if generator:
            command += ['-G', generator]
        # Copy selected tool/discovery settings, never the cache itself or its
        # source-dependent Ninja wrapper, generated corpus or binary paths.
        for key in ('CMAKE_CXX_COMPILER', 'CMAKE_C_COMPILER', 'CMAKE_PREFIX_PATH',
                    'CMAKE_OSX_ARCHITECTURES', 'CMAKE_OSX_DEPLOYMENT_TARGET', 'CMAKE_OSX_SYSROOT',
                    'SDL3_DIR', 'Vulkan_INCLUDE_DIR', 'Vulkan_LIBRARY', 'MHP2G_GLSLANG'):
            value = cache.get(key)
            if value and not value.endswith('-NOTFOUND'):
                command.append(f'-D{key}={value}')
        command += ['-DCMAKE_BUILD_TYPE=Release', '-DPSPRECOMP_PROFILE=mhp2g',
                    '-DMHP2G_RENDERER=ON', f'-DMHP2G_KTX2={args.ktx2}',
                    '-DPSPRECOMP_BUILD_TESTS=OFF', '-DPSPRECOMP_BUILD_PROFILE_TESTS=OFF',
                    f'-DPSPRECOMP_GENERATED_JOBS={args.generated_jobs}',
                    f'-DMHP2G_GENERATED_DIR={generated}',
                    f'-DMHP2G_AOT_ELF_SHA256={fingerprint["elf_sha256"]}',
                    f'-DMHP2G_FFMPEG={cache.get("MHP2G_FFMPEG", "bundled")}',
                    f'-DMHP2G_FFMPEG_DOWNLOAD_DIR={cache.get("MHP2G_FFMPEG_DOWNLOAD_DIR", str(tools / "_deps/downloads"))}']
        run(command, root)
        run([cmake, '--build', build, '--target', 'Poke', '--parallel', args.jobs], root)
        print(f'\nFUComplete native build: {build / "bin/Poke"}', flush=True)
        print('Next: install compatible overlay candidates under ' + str(build / 'bin/overlays'))
        print('For the default game/build paths, launch with python3 run_fuc.py.')
        return 0
    except (OSError, ValueError, struct.error, subprocess.CalledProcessError) as exc:
        print(f'FUComplete build stopped: {exc}', file=sys.stderr)
        print('The original profile generated directory and normal executable were not replaced.', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
