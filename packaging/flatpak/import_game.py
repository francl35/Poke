#!/usr/bin/env python3
"""Import a private game copy into Kokoto's Flatpak data directory. No builds."""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import tempfile

APP_ID='io.github.francl35.Kokoto'
ELF_SHA='255215e8573d48e9d74bee0e78c9779789bfc4b600a8271402906c5d0d74e312'


def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda:f.read(1048576),b''): h.update(b)
    return h.hexdigest()


def checked_tree(path):
    if not path.is_dir() or path.is_symlink():
        raise ValueError('Expected a real directory: '+str(path))
    for base,dirs,files in os.walk(path,followlinks=False):
        for name in dirs+files:
            if (Path(base)/name).is_symlink():
                raise ValueError('Symlink input requires review: '+str(Path(base)/name))


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--elf',type=Path,required=True)
    ap.add_argument('--iso',type=Path,required=True)
    ap.add_argument('--copy-ms0',type=Path)
    ap.add_argument('--copy-renderer-data',type=Path)
    ap.add_argument('--font',type=Path,action='append',default=[])
    ap.add_argument('--write',action='store_true')
    a=ap.parse_args()
    dest=Path.home()/'.var/app'/APP_ID/'data/kokoto'
    if dest.exists() or dest.is_symlink():
        ap.error('Destination already exists; nothing overwritten: '+str(dest))
    if not a.elf.is_file() or a.elf.is_symlink() or sha(a.elf)!=ELF_SHA:
        ap.error('ELF is not the reviewed FreedomDX executable')
    if not a.iso.is_file() or a.iso.is_symlink(): ap.error('ISO is missing or symlinked')
    with a.iso.open('rb') as f:
        f.seek(0x8001)
        if f.read(5)!=b'CD001': ap.error('Input is not an ISO9660 disc image')
    for p in (a.copy_ms0,a.copy_renderer_data):
        if p:
            try: checked_tree(p)
            except ValueError as e: ap.error(str(e))
    for p in a.font:
        if not p.is_file() or p.is_symlink() or p.suffix.lower() not in {'.ttf','.otf','.ttc'}:
            ap.error('Invalid font input: '+str(p))
    print('Destination: '+str(dest))
    print(f'Game copy: {(a.elf.stat().st_size+a.iso.stat().st_size)/1048576:.1f} MiB, plus selected saves/renderer data/fonts')
    print('Original files remain untouched. Existing Flatpak data is never overwritten.')
    if not a.write:
        print('Plan only; re-run with --write after reviewing paths.')
        return
    dest.parent.mkdir(parents=True,exist_ok=True)
    tmp=Path(tempfile.mkdtemp(prefix='.kokoto-import-',dir=dest.parent))
    try:
        (tmp/'game').mkdir()
        shutil.copy2(a.elf,tmp/'game/EBOOT.ELF')
        shutil.copy2(a.iso,tmp/'game/disc.iso')
        if sha(tmp/'game/EBOOT.ELF')!=ELF_SHA:
            raise ValueError('ELF changed during import')
        if a.copy_ms0: shutil.copytree(a.copy_ms0,tmp/'ms0')
        if a.copy_renderer_data:
            renderer=a.copy_renderer_data
            def ignore(base,names):
                return [n for n in names if n in {'pipeline_cache.bin','logs','captures','screenshots'}] if Path(base)==renderer else []
            shutil.copytree(renderer,tmp/'renderer-data',ignore=ignore)
            settings=tmp/'renderer-data/settings.ini'
            if settings.is_file():
                original=renderer.resolve().as_posix()
                final=(dest/'renderer-data').as_posix()
                lines=[]
                for line in settings.read_text().splitlines():
                    if '=' in line:
                        key,value=line.split('=',1)
                        if key.strip() in {'text.font','video.texture_pack_folder'} and value.startswith(original+'/'):
                            line=key+'='+final+value[len(original):]
                    lines.append(line)
                settings.write_text('\n'.join(lines)+'\n')
        if a.font:
            fonts=tmp/'renderer-data/fonts';fonts.mkdir(parents=True,exist_ok=True)
            for p in a.font:
                target=fonts/p.name
                if target.exists() and sha(target)!=sha(p):
                    raise ValueError('Conflicting font filename: '+p.name)
                if not target.exists(): shutil.copy2(p,target)
        if dest.exists(): raise ValueError('Destination appeared during import')
        tmp.rename(dest)
    finally:
        if tmp.exists(): shutil.rmtree(tmp)
    print('Import complete. Choose the copied font in Video -> Text if the previous font path is unavailable.')


if __name__=='__main__': main()
