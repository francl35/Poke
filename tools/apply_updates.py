#!/usr/bin/env python3
"""Apply reviewed source edits to a checkout AND its existing Flatpak context."""
from pathlib import Path
import argparse,json,datetime,shutil
from common import digest,put,json_put,update_relocation,refresh_export

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);p.add_argument('--context',type=Path,required=True);p.add_argument('--write',action='store_true');a=p.parse_args();kit=Path(__file__).resolve().parent.parent;root=a.root.resolve();context=a.context.resolve()
 entries=json.loads((kit/'changes.json').read_text());plans=[]
 for entry in entries:
  rel=entry['path'];scope=entry['scope'];bases=[root,context/'source'] if scope=='source' else [context/'source/flatpak-files']
  new=(kit/'payload'/entry['payload']).read_bytes()
  for base in bases:
   path=base/rel
   if path.is_symlink():p.error('Refusing linked destination '+str(path))
   old=path.read_bytes() if path.exists() else None
   if old==new:continue
   if (digest(old) if old is not None else None)!=entry['before_sha256']:p.error('Source differs from reviewed snapshot; refusing overwrite: '+str(path))
   plans.append((path,old,new,scope,rel))
 print(f'{len(plans)} file updates; original profiles/mhp2g and generated corpora are untouched.')
 for path,*_ in plans:print(path)
 if not a.write:print('PLAN ONLY; repeat with --write.');return
 stamp=datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f');backup=context/'maintenance-backups'/stamp
 for i,(path,old,new,scope,rel) in enumerate(plans):
  if old is not None:put(backup/f'{i:04d}.before',old)
  put(path,new)
 json_put(backup/'index.json',[{'path':str(t[0]),'backup':f'{i:04d}.before' if t[1] is not None else None} for i,t in enumerate(plans)])
 for base in (root,context/'source'):
  changes={rel:(old,new) for path,old,new,scope,rel in plans if scope=='source' and path==base/rel}
  update_relocation(base,changes)
 folder=context/'source/flatpak-files'
 for name in ('common.py','overlay_cache.py'):shutil.copy2(kit/'tools'/name,folder/name)
 manifests=[f for f in context.glob('*.json') if isinstance((v:=json.loads(f.read_text())),dict) and 'app-id' in v]
 if len(manifests)!=1:p.error('Expected exactly one application manifest')
 manifest=json.loads(manifests[0].read_text());platform={k:manifest[k] for k in ('runtime','runtime-version','sdk')};platform['arch']='x86_64';json_put(folder/'build-platform.json',platform)
 exported=[path.relative_to(context/'source').as_posix() for path,*_ in plans if path.is_relative_to(context/'source')]
 exported+=['flatpak-files/common.py','flatpak-files/overlay_cache.py','flatpak-files/build-platform.json']
 relocation=context/'source/shared/mh/mhp1-relocation-manifest.json'
 if relocation.is_file():exported.append(relocation.relative_to(context/'source').as_posix())
 refresh_export(context,exported)
 print('Applied. Backups:',backup)
 print('Run rename_shared.py on checkout and exported source next, then capture the previous SDK overlays before rebuilding.')
if __name__=='__main__':main()
