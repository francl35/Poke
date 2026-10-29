#!/usr/bin/env python3
"""Read-only heuristic source/Flatpak audit. Python 3.9+, standard library only."""
import argparse
import collections
import hashlib
import json
import os
from pathlib import Path
import re
import sys

SKIP = {'.git', '.flatpak-builder', 'flatpak-build', 'repo', 'out', 'build',
        '__pycache__', '.venv', 'node_modules', 'generated', 'overlays', 'game',
        'ms0', 'renderer-data', '_deps'}
TEXT = {'.py', '.sh', '.cmake', '.json', '.yml', '.yaml', '.cpp', '.hpp', '.h',
        '.c', '.in', '.toml', '.md', '.txt'}
LEGACY = re.compile(r'profiles/(?:mhp2g|mhp3rd|mhfuc)(?:/|\b)')
LOCAL = re.compile(r'/home/deck/Downloads|/run/host/home/deck|/Users/[^/\s]+|/home/deck/Documents')
BINARY = {'.iso', '.cso', '.elf', '.prx', '.o', '.a', '.so', '.dylib', '.exe', '.flatpak', '.log', '.pyc'}

def walk(root, skipped=SKIP):
    for folder, dirs, names in os.walk(root, followlinks=False):
        dirs[:] = sorted(d for d in dirs if d not in skipped and not (Path(folder)/d).is_symlink())
        for name in sorted(names):
            p = Path(folder)/name
            if not p.is_symlink() and p.is_file():
                yield p

def digest(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda: stream.read(1024*1024), b''):
            h.update(block)
    return h.hexdigest()

def audit(root, manifest=None, context=None, installed=None, maximum=4):
    findings = []
    def add(level, code, path, detail):
        findings.append(dict(level=level, code=code, path=str(path), detail=detail))
    for name in ('CMakeLists.txt', 'profiles/mhp1/CMakeLists.txt',
                 'profiles/mhp1/cmake_main_native.cmake', 'profiles/mhp1/cmake_overlays.cmake',
                 'profiles/mhp1/cmake_vulkan.cmake', 'configs/nids.csv', 'src', 'include', 'shared/mh'):
        if not (root/name).exists(): add('ERROR', 'missing-input', name, 'Required source/build input absent')
    for name in ('mhp2g', 'mhp3rd', 'mhfuc'):
        if (root/'profiles'/name).exists():
            add('REVIEW', 'legacy-profile', 'profiles/'+name, 'Still present; may contain ignored local data. Do not delete automatically.')
    files = list(walk(root))
    by_size = collections.defaultdict(list)
    for p in files:
        rel = p.relative_to(root)
        size = p.stat().st_size
        if size: by_size[size].append(p)
        if '.before-' in p.name or p.suffix in {'.bak', '.orig', '.rej'}:
            add('REVIEW', 'backup', rel, 'Backup or patch remainder')
        if p.suffix in BINARY or '.so.' in p.name:
            add('REVIEW', 'source-artifact', rel, 'Build/game artifact in scanned source tree')
        if p.suffix in TEXT or p.name in {'CMakeLists.txt', 'Makefile'}:
            if size > 8*1024*1024: continue
            try: content = p.read_text(encoding='utf-8')
            except (UnicodeError, OSError): continue
            for n, line in enumerate(content.splitlines(), 1):
                if LEGACY.search(line):
                    add('REVIEW', 'legacy-reference', f'{rel}:{n}', line.strip()[:240])
                if LOCAL.search(line):
                    add('REVIEW', 'machine-path', f'{rel}:{n}', line.strip()[:240])
    duplicates = []
    for size, paths in by_size.items():
        if len(paths) < 2: continue
        hashes = collections.defaultdict(list)
        for p in paths: hashes[digest(p)].append(str(p.relative_to(root)))
        for sha, members in hashes.items():
            if len(members) > 1:
                duplicates.append(dict(bytes_each=size, sha256=sha, paths=members))
    # Private corpus sources are excluded from expensive duplicate/text scans but inventoried.
    corpora = {}
    for name in ('generated', 'overlays'):
        base = root/'profiles/mhp1'/name
        corpora[name] = dict(exists=base.is_dir(), cpp=len(list(base.rglob('*.cpp'))) if base.is_dir() else 0,
                             metadata=len(list(base.rglob('meta.txt'))) if base.is_dir() else 0)
        if not corpora[name]['cpp']:
            add('REVIEW', 'missing-corpus', str(base.relative_to(root)), 'Native corpus absent; supply preserved generated sources before native build')
    app_id = None
    if manifest:
        try:
            data = json.loads(manifest.read_text())
            app_id = data.get('app-id') or data.get('id')
            if not app_id or not data.get('command'):
                add('ERROR', 'manifest-identity', manifest, 'Missing app-id/id or command')
            for key in ('runtime', 'runtime-version', 'sdk', 'modules'):
                if not data.get(key): add('ERROR', 'manifest-field', manifest, 'Missing '+key)
            if '--device=all' not in data.get('finish-args', []):
                add('REVIEW', 'controller-permission', manifest, 'Known-working Deck --device=all is absent; test controller access')
            def inspect_modules(modules):
                for module in modules:
                    if not isinstance(module, dict):
                        add('REVIEW', 'external-module', manifest, 'External module file must be audited separately: '+str(module)); continue
                    for source in module.get('sources', []):
                        if not isinstance(source, dict): continue
                        kind = source.get('type')
                        if kind in {'archive', 'file'} and source.get('url') and not source.get('sha256'):
                            add('ERROR', 'unpinned-download', manifest, str(source.get('url')))
                        if kind == 'git' and not source.get('commit'):
                            add('REVIEW', 'unpinned-git', manifest, 'Pin source to a reviewed commit')
                        if kind in {'dir', 'file', 'archive', 'patch'} and source.get('path'):
                            target = manifest.parent/source['path']
                            if not target.exists(): add('ERROR', 'missing-local-source', target, 'Manifest source does not exist')
                            if kind == 'dir' and target.exists():
                                # Walk source payload without excluding build dirs: report their roots only.
                                for folder, dirs, names in os.walk(target, followlinks=False):
                                    for d in list(dirs):
                                        q = Path(folder)/d
                                        if d in {'.git', '.flatpak-builder', 'flatpak-build', 'out', 'build', 'repo', '_deps', 'node_modules', '.venv', '__pycache__'}:
                                            add('REVIEW', 'context-junk-directory', q, 'Local dir source contains build/cache/history data; verify skip/exclusion rules')
                                            dirs.remove(d)
                                        elif q.is_symlink(): dirs.remove(d)
                                    for name in names:
                                        q = Path(folder)/name
                                        if q.suffix.lower() in {'.iso', '.cso', '.elf', '.prx', '.flatpak', '.log', '.pyc'}:
                                            add('REVIEW', 'context-payload', q, 'Game/build/debug file in local source payload')
                    inspect_modules(module.get('modules', []))
            inspect_modules(data.get('modules', []))
        except (OSError, ValueError, TypeError, AttributeError) as e:
            add('ERROR', 'manifest-parse', manifest, str(e))
    if context and not manifest:
        add('REVIEW', 'manifest-not-selected', context, 'Pass --manifest explicitly; no manifest guessed')
    if installed:
        if not installed.is_dir(): add('ERROR', 'missing-staging', installed, 'Installed files directory absent')
        else:
            for p in walk(installed, set()):
                rel = p.relative_to(installed)
                if p.suffix in {'.cpp', '.o', '.pyc', '.iso', '.cso', '.elf', '.prx'} or any(x in rel.parts for x in ('CMakeFiles', '_deps', '.git')):
                    add('REVIEW', 'installed-junk', rel, 'Source/intermediate/game data in application payload; inspect before shipping')
    return dict(schema=1, root=str(root), manifest=str(manifest) if manifest else None, app_id=app_id,
                files_scanned=len(files), corpus=corpora, duplicate_groups=duplicates,
                findings=findings, limits=['Heuristic audit, not compilation or ABI verification',
                'Comments, synthetic fixtures and vendor copies may be intentional',
                'Ignored/private corpora excluded from text/duplicate scan',
                'Does not inspect remote archives, execute CMake or interpret all Flatpak skip rules',
                'Never scans an OSTree repo as application source; use Flatpak/OSTree tools for repository integrity'])

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root', type=Path, default=Path.cwd())
    p.add_argument('--manifest', type=Path, help='Exact JSON Flatpak manifest; YAML not parsed')
    p.add_argument('--context', type=Path, help='Context location for reporting; dir sources come from manifest')
    p.add_argument('--installed-files', type=Path, help='Optional flatpak-build/files staging directory')
    p.add_argument('--json-out', type=Path, help='Write report outside project, preferably Downloads')
    a = p.parse_args()
    if not a.root.is_dir(): p.error('--root must be an existing directory')
    result = audit(a.root.resolve(), a.manifest.resolve() if a.manifest else None,
                   a.context, a.installed_files)
    counts = collections.Counter(f['level'] for f in result['findings'])
    print(f"Scanned {result['files_scanned']} files; {counts['ERROR']} errors, {counts['REVIEW']} review items; {len(result['duplicate_groups'])} identical-content groups")
    for item in result['findings'][:100]: print(f"{item['level']} [{item['code']}] {item['path']}: {item['detail']}")
    if len(result['findings']) > 100: print('More findings in JSON report.')
    for group in result['duplicate_groups'][:10]: print('DUPLICATE:', ', '.join(group['paths']))
    print('No files deleted, no build run. Review items are not automatic failures.')
    if a.json_out:
        if a.json_out.exists(): p.error('--json-out already exists; choose a new report filename')
        a.json_out.write_text(json.dumps(result, indent=2)+'\n')
    return 1 if counts['ERROR'] else 0
if __name__ == '__main__': sys.exit(main())
