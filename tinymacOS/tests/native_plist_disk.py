"""Read the property lists actually persisted by the retail kernel on its QA disk."""
from pathlib import Path
import os,json,struct,plistlib,hashlib
base=Path(os.environ['TMC_QA_DIR']);install=json.loads((base/'install-result.json').read_text());assert install['passed'];disk=Path(install['external_work'])/'target/system.raw';assert str(disk).startswith('/var/tmp/')
records=[]
with disk.open('rb') as f:
 f.seek((51200+64)*512);nodes=f.read(384*64)
 for i in range(384):
  kind,ro,parent,size,gen,name,reserved=struct.unpack_from('<BBHII48sI',nodes,i*64);name=name.split(b'\0')[0].decode()
  if kind!=2 or not name.endswith('.plist'):continue
  assert size<=8192 and not reserved,'Preferences must remain plaintext/readable'
  f.seek((51200+144+i*16)*512);raw=f.read(size);obj=plistlib.loads(raw);assert isinstance(obj,dict)
  records.append({'name':name,'bytes':size,'sha256':hashlib.sha256(raw).hexdigest(),'keys':sorted(obj)})
  if name=='com.tinymacos.appearance.plist':assert set(obj)=={'Theme','Wallpaper','Dock','ReduceMotion','Opaque','Dark'} and all(type(v) is int for v in obj.values())
assert {x['name'] for x in records}>={'com.tinymacos.appearance.plist','com.tinymacos.system.plist'}
r={'passed':True,'iso_sha256':install['iso_sha256'],'actual_persistent_files':records,'physical_hardware_tested':False}
(base/'plist-disk-result.json').write_text(json.dumps(r,indent=2)+'\n');print('PASS actual installed filesystem property lists:',records)
