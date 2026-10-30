#!/usr/bin/env python3
"""Read-only check of a prepared source context; no build/runtime claims."""
import argparse
import hashlib
import json
from pathlib import Path


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('context',type=Path)
    a=ap.parse_args();root=a.context.resolve()
    manifest=json.loads((root/'source-export.json').read_text())
    errors=[]
    expected={item['path'] for item in manifest['files']}
    actual=set()
    for p in (root/'source').rglob('*'):
        if p.is_symlink():errors.append('Unexpected symlink '+str(p))
        if p.is_file():actual.add(p.relative_to(root/'source').as_posix())
    for path in sorted(actual-expected):errors.append('Unrecorded exported file '+path)
    for item in manifest['files']:
        p=root/'source'/item['path']
        if (root/'source').resolve() not in p.resolve().parents:
            errors.append('Invalid manifest path '+item['path']);continue
        h=hashlib.sha256()
        if not p.is_file():
            errors.append('Missing '+item['path']);continue
        with p.open('rb') as f:
            for chunk in iter(lambda:f.read(1048576),b''):h.update(chunk)
        if h.hexdigest()!=item['sha256']:errors.append('Changed '+item['path'])
    profiles=root/'source/profiles'
    if sorted(p.name for p in profiles.iterdir())!=['mhp1']:
        errors.append('Export contains a profile other than mhp1')
    if (profiles/'mhp1/game').exists():errors.append('Game-data directory leaked into source export')
    package=json.loads((root/'io.github.francl35.Kokoto.json').read_text())
    if hashlib.sha256((root/'io.github.francl35.Kokoto.json').read_bytes()).hexdigest()!=manifest['packaging_manifest_sha256']:
        errors.append('Packaging manifest changed since source export')
    if package['command']!='kokoto' or package['runtime-version']!='25.08':
        errors.append('Unexpected launcher/runtime selection')
    if not (root/'glslang-cstdint.patch').is_file():
        errors.append('Missing glslang-cstdint.patch')
    for module in package['modules']:
        for source in module['sources']:
            if source.get('path') and not (root/source['path']).exists():
                errors.append('Missing local packaging source '+source['path'])
            if source['type'] in {'file','archive'} and 'url' in source and len(source.get('sha256',''))!=64:
                errors.append('Unpinned remote source in '+module['name'])
    if errors:
        print('\n'.join('FAIL: '+s for s in errors));return 1
    print(f'PASS: {len(manifest["files"])} exported file hashes; one MHP1 profile; dependency archive pins present.')
    print('This is not proof of a successful standalone configure/build or Flatpak execution.')
    return 0


if __name__=='__main__':raise SystemExit(main())
