"""Small local-only helpers. Never configure, compile or execute game code."""
from pathlib import Path
import hashlib,json,re,os
TOKEN=re.compile(r'(?P<raw>(?:u8|u|U|L)?R"(?P<delim>[^ ()\\\t\r\n]{0,16})\(.*?\)(?P=delim)")|(?P<opaque>//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|(?<![A-Za-z0-9_])\'(?:\\.|[^\'\\\n])*\')|(?P<identifier>\b(?:mhp2g|mh)\b)',re.S)
def namespace_text(text,canonical=False):
 def change(m):
  if m.lastgroup=='identifier':return 'mhp2g' if canonical else 'mh'
  return m.group()
 return TOKEN.sub(change,text)
def digest(data):return hashlib.sha256(data).hexdigest()
def put(path,data):
 path=Path(path);path.parent.mkdir(parents=True,exist_ok=True)
 tmp=path.with_name(path.name+'.maintenance-tmp');tmp.write_bytes(data);os.replace(tmp,path)
def json_put(path,value):put(path,(json.dumps(value,indent=2,ensure_ascii=False)+'\n').encode())
def update_relocation(root,changes):
 p=root/'shared/mh/mhp1-relocation-manifest.json'
 if not p.is_file():return
 state=json.loads(p.read_text());mapping={digest(old):digest(new) for old,new in changes.values() if old is not None}
 def walk(x):
  if isinstance(x,dict):
   if x.get('sha256') in mapping:
    before=x['sha256'];x.setdefault('copied_sha256',before);x['sha256']=mapping[before]
   for v in list(x.values()):walk(v)
  elif isinstance(x,list):
   for v in x:walk(v)
 walk(state)
 state.setdefault('maintenance_files',{}).update({k:digest(v[1]) for k,v in changes.items() if k.startswith('shared/mh/')})
 json_put(p,state)
def refresh_export(context,relpaths):
 p=context/'source-export.json'
 if not p.is_file():return
 state=json.loads(p.read_text());records={v['path']:v for v in state['files']}
 for rel in relpaths:
  f=context/'source'/rel
  if not f.is_file():raise SystemExit('Missing exported file: '+str(f))
  records.setdefault(rel,{'path':rel})['sha256']=digest(f.read_bytes())
 state['files']=sorted(records.values(),key=lambda v:v['path'])
 manifests=list(context.glob('*.json'))
 manifests=[m for m in manifests if m.name!='source-export.json' and isinstance((v:=json.loads(m.read_text())),dict) and 'app-id' in v]
 if len(manifests)==1:state['packaging_manifest_sha256']=digest(manifests[0].read_bytes())
 json_put(p,state)
