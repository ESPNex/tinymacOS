"""Read-only verification of the actual three sealed filesystems, not a packer
source assertion. Compare every installed cursor byte against the source manifest.
"""
from pathlib import Path
import os,hashlib,json,struct
R=Path(__file__).resolve().parents[1];D=Path(os.environ['TMC_BUILD']);m=json.loads((R/'assets/cursors/manifest.json').read_text());result={}
for mode in ('live','disk','recovery'):
 raw=(D/('System-'+mode+'.vol')).read_bytes();assert raw[:8]==b'TMVOL2\0\0' and hashlib.sha256(raw[:-32]).digest()==raw[-32:]
 count,stride=struct.unpack_from('<II',raw,8);assert stride==64;paths=[];files={}
 for i in range(count):
  parent,isdir,off,size,name=struct.unpack_from('<iIII48s',raw,16+64*i);name=name.split(b'\0')[0].decode();path='/' if i==0 else paths[parent].rstrip('/')+'/'+name;paths.append(path)
  if not isdir:assert off+size<=len(raw)-32;files[path]=raw[off:off+size]
 for f in m['files']:
  p='/System/Library/Cursors/'+f['name'];data=files[p];assert len(data)==f['bytes'] and hashlib.sha256(data).hexdigest()==f['sha256'],(mode,p)
 assert json.loads(files['/System/Library/Cursors/manifest.json'])==m
 assert files['/System/Library/Fonts/Interface.tfn']==(R/'assets/type.tfn').read_bytes()
 result[mode]={'volume_sha256':hashlib.sha256(raw).hexdigest(),'cursor_files_verified':17,'cursor_bytes_verified':sum(f['bytes'] for f in m['files']),'font_unchanged':True}
print('PASS actual sealed live/installed/Recovery filesystems: all 17 original-derived CUR/ANI bytes, manifest and unchanged proportional system font identical')
if os.environ.get('TMC_CURSOR_BUNDLE_RESULT'):Path(os.environ['TMC_CURSOR_BUNDLE_RESULT']).write_text(json.dumps(result,indent=2)+'\n')
