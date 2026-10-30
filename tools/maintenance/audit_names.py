#!/usr/bin/env python3
"""List retained legacy references without replacing configuration/protocol identifiers."""
from pathlib import Path
import argparse,re,json
from common import TOKEN
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,required=True);a=p.parse_args();counts={};left=[]
for tree in ('shared/mh/host','profiles/mhp1/host','profiles/mhp1/tests','shared/mh/tests'):
 for f in sorted((a.root/tree).rglob('*')):
  if not f.is_file() or f.suffix not in ('.cpp','.hpp','.h','.in'):continue
  text=f.read_text()
  for m in TOKEN.finditer(text):
   if m.lastgroup=='identifier' and m.group()=='mhp2g':left.append(str(f)+':'+str(text.count('\n',0,m.start())+1))
  for n,line in enumerate(text.splitlines(),1):
   if not re.search('mhp2g|MHP2G|Yakumo|Kokoto|Poke',line):continue
   category='legacy environment/macro' if 'MHP2G_' in line else 'overlay C ABI' if re.search(r'mhp2g_(overlay_info|register_overlay)',line) else 'legacy filename/config/comment/branding'
   counts[category]=counts.get(category,0)+1
print(json.dumps({'remaining_cpp_namespace_identifiers':left,'retained_reference_lines':counts},indent=2))
raise SystemExit(1 if left else 0)
