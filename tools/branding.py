#!/usr/bin/env python3
"""Generate application branding; stable app/data identities preserve existing saves."""
from pathlib import Path
import argparse,json,re,shutil
from common import put,json_put,refresh_export,update_relocation

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);p.add_argument('--context',type=Path);p.add_argument('--name');p.add_argument('--write',action='store_true');p.add_argument('--audit',action='store_true');p.add_argument('--allow-identity-change',action='store_true');a=p.parse_args();root=a.root.resolve()
 f=root/'profiles/mhp1/branding.json';b=json.loads(f.read_text())
 if a.name:b['display_name']=a.name
 for k in ('display_name','app_id','command','data_subdir'):
  if not isinstance(b.get(k),str) or not b[k] or any(ord(c)<32 for c in b[k]):p.error('Invalid '+k)
 if not re.fullmatch(r'[A-Za-z][A-Za-z0-9]*(?:\.[A-Za-z_][A-Za-z0-9_-]*){2,}',b['app_id']):p.error('Invalid reverse-DNS app_id')
 if not re.fullmatch(r'[a-z][a-z0-9_-]*',b['command']) or not re.fullmatch(r'[a-z][a-z0-9_-]*',b['data_subdir']):p.error('Invalid command or data_subdir')
 if not a.allow_identity_change and (b['app_id']!='io.github.francl35.Kokoto' or b['data_subdir']!='kokoto'):
  p.error('Changing app/data identity separates saves; use --allow-identity-change deliberately, or keep original identity')
 header='#pragma once\n// Generated from profiles/mhp1/branding.json by tools/branding.py.\n'
 for macro,k in [('NAME','display_name'),('ID','app_id'),('COMMAND','command'),('DATA_SUBDIR','data_subdir')]:header+=f'#define MH_APP_{macro} '+json.dumps(b[k],ensure_ascii=False)+'\n'
 changes={'profiles/mhp1/branding.json':(f.read_bytes(),(json.dumps(b,indent=2,ensure_ascii=False)+'\n').encode()),'profiles/mhp1/host/app_branding.hpp':((root/'profiles/mhp1/host/app_branding.hpp').read_bytes(),header.encode())}
 for rel,(_,new) in changes.items():print(f'{rel}: {len(new)} bytes')
 print(f"Name={b['display_name']}; app-id={b['app_id']}; command={b['command']}; data folder={b['data_subdir']}")
 if a.audit:
  for tree in ('shared/mh/host','profiles/mhp1/host'):
   for q in sorted((root/tree).rglob('*')):
    if q.suffix not in ('.cpp','.hpp','.h','.in'):continue
    for n,line in enumerate(q.read_text().splitlines(),1):
     if re.search(r'\b(Kokoto|Poke|Yakumo)\b',line):print(f'REVIEW {q.relative_to(root)}:{n}: {line.strip()}')
 if not a.write:print('PLAN ONLY; add --write.');return
 for rel,(_,new) in changes.items():put(root/rel,new)
 update_relocation(root,changes)
 if a.context:
  context=a.context.resolve();copied=[]
  for rel,(_,new) in changes.items():put(context/'source'/rel,new);copied.append(rel)
  folder=context/'source/flatpak-files'
  oldicons=list(folder.glob('*.svg'));icon=folder/(b['app_id']+'.svg')
  if not icon.exists():
   if len(oldicons)!=1:p.error('Cannot choose an icon to copy for the new app ID')
   put(icon,oldicons[0].read_bytes())
  desktop=folder/(b['app_id']+'.desktop')
  # Desktop-entry escaping differs from JSON/C++ escaping.
  title=b['display_name'].replace('\\','\\\\').replace(';','\\;')
  put(desktop,('[Desktop Entry]\nType=Application\nName='+title+'\nComment=Monster Hunter Portable FreedomDX native project\nExec='+b['command']+'\nIcon='+b['app_id']+'\nTerminal=false\nCategories=Game;\n').encode())
  for q in context.glob('*.json'):
   obj=json.loads(q.read_text())
   if isinstance(obj,dict) and 'app-id' in obj:obj['app-id']=b['app_id'];obj['command']=b['command'];json_put(q,obj)
  copied.extend([desktop.relative_to(context/'source').as_posix(),icon.relative_to(context/'source').as_posix()]);refresh_export(context,copied)
 print('Generated branding. Internal CMake target and overlay export names are intentionally stable.')
if __name__=='__main__':main()
