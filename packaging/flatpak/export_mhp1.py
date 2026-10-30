#!/usr/bin/env python3
"""Prepare an MHP1-only private Flatpak source context; never builds or deletes."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import tempfile

SKIP = {'.git', '__pycache__', '.cache', 'build', 'out', '_deps', 'CMakeFiles',
        'game', 'analysis', 'ms0', 'renderer-data', 'captures', 'overlay-dumps',
        '.flatpak-builder', 'flatpak-build', 'repo', '.venv', 'node_modules'}
BINARY = {'.o', '.obj', '.so', '.dylib', '.dll', '.a', '.lib', '.exe', '.elf', '.iso', '.prx', '.pyc', '.bin'}
ROOTS = ('src', 'include', 'configs', 'cmake', 'tests', 'shared/mh', 'profiles/mhp1')


def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda: f.read(1024*1024), b''):
            h.update(b)
    return h.hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--root', type=Path, default=Path.cwd())
    ap.add_argument('--out', type=Path, required=True, help='New source context outside the repository')
    ap.add_argument('--write', action='store_true', help='Copy files; default only reports plan')
    policy = ap.add_mutually_exclusive_group(required=True)
    policy.add_argument('--overlay-cache', type=Path, help='Existing verified cache folder; SDK revalidates before reuse')
    policy.add_argument('--build-overlays', action='store_true', help='Explicitly compile overlays in SDK')
    a = ap.parse_args()
    root, output = a.root.resolve(), a.out.resolve()
    if output == root or root in output.parents:
        ap.error('--out must be outside the source repository')
    if output.exists():
        ap.error('--out already exists; nothing overwritten')
    required = ('CMakeLists.txt', 'configs/nids.csv', 'profiles/mhp1/cmake_main_native.cmake',
                'shared/mh/host/ui/font_menu.cpp', 'profiles/mhp1/host/ui/mhp1_theme.hpp',
                'shared/mh/third_party/imgui/imgui.cpp', 'shared/mh/third_party/tiny_aes/aes.c',
                'profiles/mhp1/generated/generated_registry.cpp')
    for name in required:
        if not (root/name).is_file():
            ap.error('Required input missing: '+name)
    for name in ('CMakeLists.txt', 'cmake_vulkan.cmake', 'cmake_overlays.cmake', 'cmake_main_native.cmake'):
        p = root/'profiles/mhp1'/name
        for line in p.read_text().splitlines():
            if not line.lstrip().startswith('#') and any(old in line for old in ('profiles/mhp2g', 'profiles/mhp3rd', 'profiles/mhfuc')):
                ap.error('MHP1 still refers to original profile files: '+str(p))
    ff = (root/'shared/mh/cmake/FFmpeg.cmake').read_text()
    kt = (root/'shared/mh/cmake/KTX2.cmake').read_text()
    if 'de668509caf9e35e3cd162473441fdb29538c6d96ed080292b3cf9e6fc5d558f' not in ff:
        ap.error('FFmpeg pin differs from packaging manifest; review before building')
    if '4d0a3c4470c67e0f1544d2a92f379dc919c9627a5c3fa5c4fcaf4c22324827f5' not in kt:
        ap.error('KTX pin differs from packaging manifest; review before building')
    overlays = sorted((root/'profiles/mhp1/overlays').glob('*/meta.txt'))
    if not overlays:
        ap.error('No MHP1 overlay corpora found; do not regenerate automatically')
    for meta in overlays:
        if not list(meta.parent.glob('*.cpp')):
            ap.error('Overlay metadata has no generated source: '+str(meta))
    kit = Path(__file__).resolve().parent
    helpers = root/'tools/maintenance'
    for name in ('common.py', 'overlay_cache.py'):
        if not (helpers/name).is_file(): ap.error('Missing canonical helper: '+str(helpers/name))
    for name in ('glslang-cstdint.patch', 'io.github.francl35.Kokoto.json'):
        if not (kit/name).is_file(): ap.error('Missing packaging input: '+name)
    for p in (kit/'app-files').rglob('*'):
        if p.is_symlink(): ap.error('Packaging symlink requires review: '+str(p))
    cache = a.overlay_cache.resolve() if a.overlay_cache else None
    if cache:
        if not (cache/'manifest.json').is_file() or not (cache/'libraries').is_dir():
            ap.error('Expected overlay-cache manifest.json and libraries/; capture with overlay_cache.py first')
        for p in cache.rglob('*'):
            if p.is_symlink(): ap.error('Cache symlink requires review: '+str(p))
    selected = []
    for name in ('analyzer_main.cpp', 'codegen_main.cpp', 'dump_function.cpp'):
        p = root/'tools'/name
        if not p.is_file() or p.is_symlink(): ap.error('Missing or symlinked native tool: '+str(p))
        selected.append(p)
    for directory in ROOTS:
        start = root/directory
        if not start.is_dir() or start.is_symlink():
            ap.error('Required source directory missing or symlinked: '+directory)
        for base, dirs, files in os.walk(start, followlinks=False):
            for d in dirs:
                if d not in SKIP and (Path(base)/d).is_symlink():
                    ap.error('Symlink source directory requires review: '+str(Path(base)/d))
            dirs[:] = sorted(d for d in dirs if d not in SKIP)
            for name in sorted(files):
                p = Path(base)/name
                if name == '.git' or p.suffix.lower() in BINARY or '.so.' in name or '.before-' in name or p.suffix in {'.bak', '.orig', '.rej', '.log'}:
                    continue
                if p.is_symlink():
                    ap.error('Symlink source file requires review: '+str(p))
                selected.append(p)
    for p in root.iterdir():
        if p.is_file() and (p.name in ('CMakeLists.txt', 'CMakePresets.json', 'VERSION') or
                           p.name.upper().startswith(('LICENSE', 'COPYING', 'NOTICE', 'AUTHORS'))):
            if p.is_symlink():
                ap.error('Symlink root metadata requires review: '+str(p))
            selected.append(p)
    size = sum(p.stat().st_size for p in selected)
    print(f'Plan: {len(selected)} files, {size/1048576:.1f} MiB; {len(overlays)} overlay corpora')
    print('Includes private generated C++ sources; excludes game ELF/ISO, saves, textures and compiled binaries.')
    print('Original repository remains unchanged. Support headers and third-party source are conservative supersets.')
    if not a.write:
        print('Plan only. Re-run with --write to prepare the build context.')
        return
    output.parent.mkdir(parents=True, exist_ok=True)
    temp = Path(tempfile.mkdtemp(prefix='.kokoto-export-', dir=output.parent))
    try:
        records = []
        for p in selected:
            relative = p.relative_to(root)
            dest = temp/'source'/relative
            dest.parent.mkdir(parents=True, exist_ok=True)
            before = sha(p)
            shutil.copy2(p, dest)
            if sha(dest) != before:
                raise RuntimeError('Input changed during copy: '+str(relative))
            records.append({'path': relative.as_posix(), 'sha256': before})
        shutil.copytree(kit/'app-files', temp/'source/flatpak-files',
                        ignore=shutil.ignore_patterns('__pycache__', '*.pyc'))
        for name in ('common.py', 'overlay_cache.py'):
            shutil.copy2(helpers/name, temp/'source/flatpak-files'/name)
        for p in sorted((temp/'source/flatpak-files').rglob('*')):
            if p.is_file():
                records.append({'path':p.relative_to(temp/'source').as_posix(),'sha256':sha(p)})
        manifest = json.loads((kit/'io.github.francl35.Kokoto.json').read_text())
        module = next(m for m in manifest['modules'] if m['name'] == 'kokoto')
        module['sources'] = [s for s in module['sources'] if s.get('path') != 'overlay-cache']
        environment = module.setdefault('build-options', {}).setdefault('env', {})
        environment['MH_OVERLAY_POLICY'] = 'require-cache' if cache else 'build'
        if cache:
            shutil.copytree(cache, temp/'overlay-cache')
            module['sources'].append({'type':'dir', 'path':'overlay-cache', 'dest':'flatpak-deps/overlay-cache'})
        (temp/'io.github.francl35.Kokoto.json').write_text(json.dumps(manifest, indent=2)+'\n')
        shutil.copy2(kit/'glslang-cstdint.patch', temp/'glslang-cstdint.patch')
        (temp/'source-export.json').write_text(json.dumps({'schema':1,'source_root':str(root),
             'scope':'Private MHP1 source context, not a public source release or minimal dependency closure',
             'packaging_manifest_sha256':sha(temp/'io.github.francl35.Kokoto.json'),
             'overlay_count':len(overlays),'files':records},indent=2)+'\n')
        if output.exists():
            raise RuntimeError('Destination appeared during export')
        temp.rename(output)
    finally:
        if temp.exists():
            shutil.rmtree(temp)
    print('Wrote context: '+str(output))
    print('No build or source deletion performed. Keep this context private: it includes game-derived AOT code.')


if __name__ == '__main__':
    main()
