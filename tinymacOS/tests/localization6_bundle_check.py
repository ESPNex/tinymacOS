"""Read-only check of actual built System-*.vol plists and executable paths."""
from pathlib import Path
import ast,hashlib,json,os,plistlib,struct
R=Path(__file__).resolve().parents[1];D=Path(os.environ['TMC_BUILD'])
tree=ast.parse((R/'tools/system_volume.py').read_text());labels=None
for node in tree.body:
 if isinstance(node,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='display_names' for t in node.targets):labels=ast.literal_eval(node.value)
assert labels and len(labels)==13
results={}
for mode in ('live','disk','recovery'):
 p=D/('System-'+mode+'.vol');raw=p.read_bytes();assert raw[:8]==b'TMVOL2\0\0' and hashlib.sha256(raw[:-32]).digest()==raw[-32:]
 count,stride=struct.unpack_from('<II',raw,8);assert stride==64;paths=[];files={}
 for i in range(count):
  parent,isdir,off,size,name=struct.unpack_from('<iIII48s',raw,16+64*i);name=name.split(b'\0')[0].decode()
  path='/' if i==0 else (paths[parent].rstrip('/')+'/'+name);paths.append(path)
  if not isdir:files[path]=raw[off:off+size]
 plists={path:plistlib.loads(data) for path,data in files.items() if path.endswith('.app/Contents/Info.plist')};assert len(plists)==(4 if mode=='recovery' else 14 if mode=='live' else 13)
 for path,v in plists.items():
  src=v['CFBundleIdentifier'].removeprefix('org.tinymacos.');display=('Reinstalla tinymacOS Tramontana' if mode=='recovery' else 'Installa tinymacOS Tramontana') if src=='installer' else labels[src]
  assert v['CFBundleDisplayName']==display,(path,v)
  assert v['CFBundleShortVersionString']=='10.2.6' and v['CFBundleVersion']=='30C1'
  exe=path.removesuffix('Info.plist')+'MacOS/'+v['CFBundleExecutable'];assert files[exe][:4]==b'\xcf\xfa\xed\xfe'
  assert path.endswith('/'+v['CFBundleExecutable']+'.app/Contents/Info.plist')
 results[mode]={'volume_sha256':hashlib.sha256(raw).hexdigest(),'localized_bundles_verified':len(plists)}
print('PASS actual sealed System volumes: live/disk/Recovery Italian CFBundleDisplayName, stable English executable/bundle paths, Mach-O files and public version; payload checksums exact')
if os.environ.get('TMC_BUNDLE_RESULT'):Path(os.environ['TMC_BUNDLE_RESULT']).write_text(json.dumps(results,indent=2)+'\n')
