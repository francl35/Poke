#!/usr/bin/env python3
"""Plan first; migrate owned C++ namespaces while retaining wire names and keys."""
from pathlib import Path
import argparse
from common import namespace_text,put,update_relocation,refresh_export

def plan(root):
 changes={}
 for tree in ('shared/mh/host','shared/mh/tests','profiles/mhp1/host','profiles/mhp1/tests'):
  for p in sorted((root/tree).rglob('*')):
   if not p.is_file() or p.suffix not in ('.cpp','.hpp','.h','.in'):continue
   old=p.read_bytes();new=namespace_text(old.decode()).encode()
   if new!=old:changes[p.relative_to(root).as_posix()]=(old,new)
 return changes

def main():
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('--root',type=Path,required=True);a.add_argument('--write',action='store_true');v=a.parse_args();root=v.root.resolve()
 if not (root/'shared/mh/host').is_dir():a.error('Expected a source root containing shared/mh/host')
 changes=plan(root)
 for rel in changes:print(rel)
 print(f'{len(changes)} files; only C++ identifier mhp2g becomes mh. Strings, comments, macros, filenames and overlay C exports stay unchanged.')
 if not v.write:print('PLAN ONLY; add --write to apply.');return
 for rel,(_,new) in changes.items():put(root/rel,new)
 update_relocation(root,changes)
 if root.name=='source':refresh_export(root.parent,[*changes,'shared/mh/mhp1-relocation-manifest.json'] if (root/'shared/mh/mhp1-relocation-manifest.json').exists() else list(changes))
 print('Applied. Rebuild the host and its tests; no generated corpus was rewritten.')
if __name__=='__main__':main()
