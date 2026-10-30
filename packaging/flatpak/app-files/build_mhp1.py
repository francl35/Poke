#!/usr/bin/env python3
"""Run by flatpak-builder inside the SDK, only when the user starts a build."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import json
from overlay_cache import cache_load

root = Path.cwd()
build = root/'_flatpak_build'
prefix = Path('/app')
branding=json.loads((root/'profiles/mhp1/branding.json').read_text())
jobs=os.environ.get('MH_BUILD_JOBS','4')
if not jobs.isdecimal() or not 1 <= int(jobs) <= 64:
    raise SystemExit('MH_BUILD_JOBS must be between 1 and 64')
policy=os.environ.get('MH_OVERLAY_POLICY','auto')
if policy not in ('auto','require-cache','repair-cache','build'):
    raise SystemExit('Unknown MH_OVERLAY_POLICY')
platform=json.loads((root/'flatpak-files/build-platform.json').read_text())


def run(args, **kwargs):
    print('+ '+' '.join(map(str,args)), flush=True)
    subprocess.run(list(map(str,args)), check=True, **kwargs)


run(['cmake','-S',root,'-B',build,'-G','Ninja',
     '-DPSPRECOMP_PROFILE=mhp1','-DCMAKE_BUILD_TYPE=Release',
     '-DMHP1_BUILD_BOOT=ON','-DMHP1_BUILD_PROBE=OFF','-DMHP1_BUILD_NATIVE_MAIN=ON',
     '-DMHP1_VULKAN=ON','-DMHP1_CODECS=ON',
     '-DMHP1_SHARED_PROFILE_DIR='+str(root/'shared/mh'),
     '-DMHP2G_FFMPEG=bundled','-DMHP2G_KTX2=bundled',
     '-DMHP2G_FFMPEG_DOWNLOAD_DIR='+str(root/'flatpak-deps/downloads'),
     '-DFETCHCONTENT_SOURCE_DIR_POKE_KTX='+str(root/'flatpak-deps/ktx'),
     '-DPSPRECOMP_GENERATED_JOBS='+jobs,'-DPSPRECOMP_GENERATED_OPT_LEVEL=2',
     '-DPSPRECOMP_BUILD_TESTS=ON','-DPSPRECOMP_BUILD_PROFILE_TESTS=ON',
     '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON'])
cached_names=None
cache=root/'flatpak-deps/overlay-cache'
if policy != 'build':
    try:
        cached_names=cache_load(root,build,cache,platform,allow_partial=(policy=='repair-cache'))
        print(f'[overlays] verified {len(cached_names)} cached SDK libraries; their compilation will be skipped',flush=True)
    except (ValueError,OSError,KeyError) as error:
        if policy in ('require-cache','repair-cache'):
            raise SystemExit('[overlays] '+str(error)+'; use an explicit full overlay build if inputs intentionally changed')
        print('[overlays] no matching cache; normal incremental overlay targets will be built: '+str(error),flush=True)
run(['cmake','--build',build,'--target','PokeFreedomDXBoot','psprecomp_test_binaries',
     'mhp1_bootstrap_tests','--parallel',jobs])
environment = os.environ.copy()
environment.update({'SDL_AUDIODRIVER':'dummy','SDL_VIDEODRIVER':'dummy'})
environment.pop('MHP1_RENDERER',None)
run(['ctest','--test-dir',build,'--output-on-failure'],env=environment)
targets=[]
for meta in sorted((root/'profiles/mhp1/overlays').glob('*/meta.txt')):
    name=meta.parent.name
    if not re.fullmatch(r'[A-Za-z0-9_]+',name):
        raise SystemExit('Unexpected overlay target name: '+name)
    targets.append('overlay_'+name)
if not targets:
    raise SystemExit('No overlay targets; refusing to package an interpreter-only application')
destination=build/'bin/overlays'
destination.mkdir(parents=True,exist_ok=True)
for name in cached_names or []:shutil.copy2(cache/'libraries'/name,destination/name)
reused=set(cached_names or [])
missing_targets=[target for target in targets if target[len('overlay_'):]+'.so' not in reused]
print(f'[overlays] reuse {len(reused)}; build {len(missing_targets)} missing targets',flush=True)
if missing_targets:
    run(['cmake','--build',build,'--target',*missing_targets,'--parallel',jobs])
libdir=build/'bin/lib'
if not list(libdir.glob('libavcodec.so*')) or not list(libdir.glob('libavutil.so*')):
    raise SystemExit('Bundled FFmpeg output is missing')
outputs=sorted((build/'bin/overlays').glob('*.so'))
if {p.name for p in outputs}!={t[len('overlay_'):]+'.so' for t in targets}:
    raise SystemExit(f'Overlay output mismatch: {len(outputs)} libraries for {len(targets)} targets')
(prefix/'bin').mkdir(parents=True,exist_ok=True)
shutil.copy2(build/'bin/PokeFreedomDXBoot',prefix/'bin/PokeFreedomDXBoot')
shutil.copytree(libdir,prefix/'bin/lib',symlinks=True,dirs_exist_ok=True)
shutil.copytree(build/'bin/overlays',prefix/'bin/overlays',dirs_exist_ok=True)
flags=subprocess.check_output(['pkg-config','--cflags','--libs','sdl3'],text=True)
import shlex
run(['c++','-std=c++20','-O2',root/'flatpak-files/kokoto-launcher.cpp',
     '-I'+str(root/'profiles/mhp1/host'),
     '-o',prefix/'bin'/branding['command'],*shlex.split(flags)])
for path in (prefix/'bin'/branding['command'],prefix/'bin/PokeFreedomDXBoot'):
    path.chmod(0o755)
licenses=prefix/'share/licenses'/branding['command']
if (build/'bin/licenses').is_dir():
    shutil.copytree(build/'bin/licenses',licenses,dirs_exist_ok=True)
for start in (root,root/'shared/mh/third_party'):
    candidates=start.iterdir() if start==root else start.rglob('*')
    for p in candidates:
        if p.is_file() and p.name.upper().startswith(('LICENSE','COPYING','NOTICE','AUTHORS')):
            rel=p.relative_to(root)
            dst=licenses/rel;dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,dst)
for name,dest in [(branding['app_id']+'.desktop','share/applications'),
                  (branding['app_id']+'.svg','share/icons/hicolor/scalable/apps')]:
    d=prefix/dest;d.mkdir(parents=True,exist_ok=True);shutil.copy2(root/'flatpak-files'/name,d/name)
print('Installed native MHP1 binary, matching overlays, codecs, launcher and licenses; game data remains external.')
