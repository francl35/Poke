#!/usr/bin/env python3
"""Sync the reviewed export inventory and rebuild; overlay reuse is required by default."""
from pathlib import Path
import argparse,json,subprocess,shlex
from common import digest,put,json_put,refresh_export

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);p.add_argument('--context',type=Path,required=True);p.add_argument('--jobs',type=int,default=4);p.add_argument('--write',action='store_true');p.add_argument('--run',action='store_true');p.add_argument('--build-overlays',action='store_true');p.add_argument('--build-missing',action='store_true');a=p.parse_args()
 if a.build_overlays and a.build_missing:p.error('Choose --build-overlays OR --build-missing')
 if not 1<=a.jobs<=64:p.error('--jobs must be between 1 and 64')
 root=a.root.resolve();context=a.context.resolve()
 if root==context/'source':p.error('--root must be the original checkout, not exported source')
 state=json.loads((context/'source-export.json').read_text());changes=[]
 for item in state['files']:
  rel=Path(item['path'])
  if rel.is_absolute() or '..' in rel.parts:p.error('Invalid export inventory path')
  if rel.parts[0]=='flatpak-files':continue
  source=root/rel;target=context/'source'/rel
  if source.is_symlink() or target.is_symlink():p.error('Refusing symlink '+str(source))
  if not source.is_file():p.error('Exported source no longer exists in checkout; review export before removing: '+str(rel))
  data=source.read_bytes()
  if not target.is_file() or target.read_bytes()!=data:changes.append((rel,data))
 manifests=[q for q in context.glob('*.json') if isinstance((v:=json.loads(q.read_text())),dict) and 'app-id' in v]
 if len(manifests)!=1:p.error('Expected exactly one app manifest')
 manifest=json.loads(manifests[0].read_text());module=next(m for m in manifest['modules'] if m['name']=='kokoto')
 if not a.build_overlays and not (context/'overlay-cache/manifest.json').is_file():p.error('Capture matching SDK overlays first, or explicitly use --build-overlays')
 environment=module.setdefault('build-options',{}).setdefault('env',{});environment['MH_BUILD_JOBS']=str(a.jobs);environment['MH_OVERLAY_POLICY']='build' if a.build_overlays else 'repair-cache' if a.build_missing else 'require-cache'
 command=['flatpak','run','org.flatpak.Builder','--arch=x86_64',f'--jobs={a.jobs}','--ccache','--user','--force-clean','--repo=repo','--install','flatpak-build',manifests[0].name]
 for rel,_ in changes:print('SYNC',rel)
 print('Overlay policy:',environment['MH_OVERLAY_POLICY']);print(shlex.join(command))
 if not (a.write or a.run):print('PLAN ONLY. --write syncs; --run syncs and starts the user-side SDK build.');return
 for rel,data in changes:put(context/'source'/rel,data)
 json_put(manifests[0],manifest);refresh_export(context,[rel.as_posix() for rel,_ in changes])
 if a.run:subprocess.run(command,cwd=context,check=True)
if __name__=='__main__':main()
