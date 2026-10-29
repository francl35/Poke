#!/usr/bin/env python3
"""Reuse only matching x86-64 Flatpak overlay binaries; never generate code."""
from pathlib import Path
import argparse,json,re,struct,shutil
from common import digest,namespace_text,json_put,refresh_export

CACHE_VERSION=1

def inputs(root):
 paths={root/'CMakeLists.txt'}
 paths.update((root/'cmake').rglob('*.cmake'))
 for tree,exts in [('include',{'.h','.hpp','.inl'}),('src',{'.h','.hpp','.cpp','.inl'}),('profiles/mhp1/overlays',{'.h','.hpp','.cpp','.inl','.txt'})]:
  directory=root/tree
  if not directory.is_dir():raise ValueError('Missing ABI/corpus inputs: '+str(directory))
  paths.update(p for p in directory.rglob('*') if p.is_file() and p.suffix in exts)
 for rel in ('shared/mh/host/overlay_module.hpp','shared/mh/host/overlay_module.cpp.in','profiles/mhp1/cmake_overlays.cmake'):
  p=root/rel
  if not p.is_file():raise ValueError('Missing overlay interface: '+str(p))
  paths.add(p)
 records={}
 for p in sorted(paths):
  if p.is_symlink():raise ValueError('Refusing linked input: '+str(p))
  if not p.is_file():raise ValueError('Missing build definition: '+str(p))
  data=p.read_bytes()
  # A CTest timeout does not affect compiled overlay compatibility.
  if p == root/'CMakeLists.txt':
   data=data.replace(
    b'set_tests_properties(psprecomp_runtime_contract_tests PROPERTIES TIMEOUT 60)',
    b'set_tests_properties(psprecomp_runtime_contract_tests PROPERTIES TIMEOUT 300)')
  # This migration only renames C++ host namespace identifiers, not the C
  # exports or psprecomp ABI. Normalize exactly that spelling for comparison.
  if p.suffix in ('.h','.hpp','.cpp','.inl','.in'):data=namespace_text(data.decode(),canonical=True).encode()
  records[p.relative_to(root).as_posix()]=digest(data)
 return records

def toolchain(build):
 cache={}
 for line in (build/'CMakeCache.txt').read_text().splitlines():
  m=re.match(r'([^:#]+):[^=]+=(.*)',line)
  if m:cache[m[1]]=m[2]
 files=list((build/'CMakeFiles').glob('*/CMakeCXXCompiler.cmake'))
 if len(files)!=1:raise ValueError('Cannot identify exactly one SDK C++ compiler configuration')
 text=files[0].read_text()
 def cmakevar(k):
  m=re.search(r'set\('+re.escape(k)+r'\s+"([^"\n]*)"\)',text)
  if not m:raise ValueError('Missing compiler property '+k)
  return m[1]
 props={k:cache.get(k,'') for k in ('CMAKE_BUILD_TYPE','CMAKE_CXX_FLAGS','CMAKE_CXX_FLAGS_RELEASE','CMAKE_SHARED_LINKER_FLAGS','CMAKE_SHARED_LINKER_FLAGS_RELEASE','CMAKE_MODULE_LINKER_FLAGS','CMAKE_MODULE_LINKER_FLAGS_RELEASE','PSPRECOMP_GENERATED_OPT_LEVEL')}
 props.update(compiler_id=cmakevar('CMAKE_CXX_COMPILER_ID'),compiler_version=cmakevar('CMAKE_CXX_COMPILER_VERSION'))
 if props['CMAKE_BUILD_TYPE']!='Release' or props['PSPRECOMP_GENERATED_OPT_LEVEL']!='2':raise ValueError('Only the reviewed Release/-O2 overlay configuration can be reused')
 return props

def key(root,build,platform):return {'schema':CACHE_VERSION,'platform':platform,'toolchain':toolchain(build),'inputs':inputs(root)}

def library_names(root):
 names=[m.parent.name+'.so' for m in sorted((root/'profiles/mhp1/overlays').glob('*/meta.txt'))]
 if not names or any(not re.fullmatch(r'[A-Za-z0-9_]+\.so',n) for n in names):raise ValueError('Missing or invalid overlay metadata')
 return names

def audit_elf(path):
 data=path.read_bytes()
 if data[:6]!=b'\x7fELF\x02\x01' or struct.unpack_from('<H',data,18)[0]!=62:raise ValueError('Not an x86-64 little-endian ELF: '+str(path))
 off=struct.unpack_from('<Q',data,40)[0];size,count=struct.unpack_from('<HH',data,58)
 sections=[struct.unpack_from('<IIQQQQIIQQ',data,off+i*size) for i in range(count)]
 exports=set()
 for s in sections:
  if s[1]!=11:continue
  strings=sections[s[6]];names=data[strings[4]:strings[4]+strings[5]]
  for at in range(s[4],s[4]+s[5],s[9]):
   name,info,other,index,value,length=struct.unpack_from('<IBBHQQ',data,at)
   text=names[name:names.find(b'\0',name)].decode(errors='replace')
   if index==0 and ('6mhp2g' in text or '2mh' in text):raise ValueError('Overlay depends on the old/new host namespace: '+text)
   if index!=0:exports.add(text)
 if not {'mhp2g_overlay_info','mhp2g_register_overlay'}<=exports:raise ValueError('Overlay C entry points missing: '+str(path))

def cache_load(root,build,folder,platform,allow_partial=False):
 state=json.loads((folder/'manifest.json').read_text())
 expected=key(root,build,platform)
 if state['key']!=expected:raise ValueError('Overlay ABI/source/toolchain fingerprint changed. Build overlays explicitly; cache reuse refused.')
 names=library_names(root)
 available=set(state['libraries'])
 if not available<=set(names):raise ValueError('Cache contains overlays outside the current corpus')
 if not allow_partial and set(names)!=available:raise ValueError('Partial cache requires --build-missing / repair-cache policy')
 for name in sorted(available):
  p=folder/'libraries'/name
  if p.is_symlink() or digest(p.read_bytes())!=state['libraries'][name]:raise ValueError('Overlay cache hash mismatch: '+name)
  audit_elf(p)
 return sorted(available)

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--capture',type=Path,help='Completed SDK source directory; default: latest complete corpus in this context');p.add_argument('--context',type=Path,required=True);p.add_argument('--write',action='store_true');p.add_argument('--partial',action='store_true',help='Capture available libraries; compile missing targets later');p.add_argument('--replace',action='store_true',help='Back up an existing cache and replace it');p.add_argument('--libraries',type=Path,help='Existing SDK-packaged overlay directory');a=p.parse_args();context=a.context.resolve();new=context/'source'
 if a.capture:old=a.capture.resolve()
 else:
  candidates=[];names=library_names(new)
  for q in (context/'.flatpak-builder/build').glob('kokoto*'):
   folder=q/'_flatpak_build/bin/overlays'
   present=sum((folder/n).is_file() for n in names)
   if present==len(names) or (a.partial and present>0):candidates.append(q.resolve())
  if not candidates:p.error('No suitable SDK libraries found. Use --capture with a configured SDK source and --partial for an empty cache, or build overlays explicitly.')
  old=max(set(candidates),key=lambda q:(sum((q/'_flatpak_build/bin/overlays'/n).is_file() for n in names),max((q/'_flatpak_build/bin/overlays'/n).stat().st_mtime for n in names if (q/'_flatpak_build/bin/overlays'/n).is_file())))
 print('SDK source:',old)
 build=old/'_flatpak_build'
 manifests=[q for q in context.glob('*.json') if isinstance((v:=json.loads(q.read_text())),dict) and 'app-id' in v]
 if len(manifests)!=1:p.error('Expected exactly one application manifest')
 manifest=json.loads(manifests[0].read_text());platform={k:manifest[k] for k in ('runtime','runtime-version','sdk')};platform['arch']='x86_64'
 oldkey=key(old,build,platform);newinputs=inputs(new)
 if oldkey['inputs']!=newinputs:
  differences=sorted(k for k in set(oldkey['inputs'])|set(newinputs) if oldkey['inputs'].get(k)!=newinputs.get(k))
  p.error('Old SDK outputs do not match current ABI/corpus inputs: '+', '.join(differences[:12]))
 expected_names=library_names(old);folder=a.libraries.resolve() if a.libraries else build/'bin/overlays';hashes={}
 names=[n for n in expected_names if (folder/n).is_file()]
 missing=sorted(set(expected_names)-set(names))
 if missing and not a.partial:p.error(f'{len(missing)} overlay libraries missing; use --partial to reuse the available subset')
 for name in names:
  f=folder/name
  if f.is_symlink():p.error('Refusing linked overlay '+str(f))
  audit_elf(f);hashes[name]=digest(f.read_bytes())
 # A source modified after compilation cannot certify that output.
 oldest=min(((folder/n).stat().st_mtime for n in names),default=float('inf'))
 newer=[rel for rel in oldkey['inputs'] if (old/rel).stat().st_mtime>oldest+2]
 if newer:
  if not a.libraries:
   p.error('SDK source changed after some overlays were built: '+', '.join(newer[:8]))
  expected=(context/'flatpak-build/files/bin/overlays').resolve()
  if folder != expected:
   p.error('Timestamp exception applies only to this context staging folder')
  import configparser
  metadata=configparser.ConfigParser()
  metadata.read(context/'flatpak-build/metadata')
  app=metadata['Application'] if metadata.has_section('Application') else {}
  expected_runtime=platform['runtime']+'/x86_64/'+platform['runtime-version']
  if app.get('name') != manifest['app-id'] or app.get('runtime') != expected_runtime:
   p.error('Staging package identity/runtime does not match this manifest')
  expected_sdk=platform['sdk']+'/x86_64/'+platform['runtime-version']
  if app.get('sdk') and app.get('sdk') != expected_sdk:
   p.error('Staging SDK does not match this manifest')
  print('[cache] staging identity/runtime validated; packaged timestamps are not used')

 print(f'Checked {len(names)} SDK overlay libraries; namespace-independent C boundary and matching core/corpus inputs.')
 print(f'Reuse {len(names)} / {len(expected_names)}; build {len(missing)} missing targets.')
 if not names:print('No libraries to reuse: all overlay targets will need compilation.')
 print('Cache payload: %.1f MiB' % (sum((folder/n).stat().st_size for n in names)/1048576))
 cache=context/'overlay-cache'
 if cache.exists() and not a.replace:p.error('Cache already exists; use --replace to preserve it as a backup and replace it')
 if not a.write:print('PLAN ONLY; add --write to create cache and enable require-cache packaging.');return
 temporary=context/'overlay-cache.maintenance-tmp'
 if temporary.exists():p.error('A partial cache exists: '+str(temporary))
 (temporary/'libraries').mkdir(parents=True)
 for name in names:shutil.copy2(folder/name,temporary/'libraries'/name)
 json_put(temporary/'manifest.json',{'key':oldkey,'libraries':hashes})
 if cache.exists():
  import datetime
  backup=context/('overlay-cache.backup-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
  cache.rename(backup);print('Previous cache saved:',backup)
 temporary.rename(cache)
 module=next(m for m in manifest['modules'] if m['name']=='kokoto')
 source={'type':'dir','path':'overlay-cache','dest':'flatpak-deps/overlay-cache'}
 if source not in module['sources']:module['sources'].append(source)
 module.setdefault('build-options',{}).setdefault('env',{})['MH_OVERLAY_POLICY']='repair-cache' if missing else 'require-cache'
 json_put(manifests[0],manifest);refresh_export(context,[])
 print('Cache saved. Fingerprint mismatches still stop packaging; missing targets require the explicit --build-missing policy.')
if __name__=='__main__':
 try:main()
 except (ValueError,OSError,KeyError) as e:raise SystemExit(str(e))
