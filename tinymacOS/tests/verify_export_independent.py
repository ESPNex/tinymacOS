#!/usr/bin/env python3
"""Independent raw-partition exFAT content reader; never the OS's driver.
Requires dissect.fat==3.13. A real guest receipt is mandatory.
"""
from pathlib import Path
import hashlib,json,sys,struct,importlib.metadata
from dissect.fat.exfat import ExFAT,RunlistStream
receipt=Path(sys.argv[1]);r=json.loads(receipt.read_text());assert r['passed']
w=Path(r['work']);assert str(w).startswith('/var/tmp/tmc-volume-export-')
checks=[]
def file_bytes(fs,path):
 tree=fs.files['/'][1];parts=path.split('/')
 for part in parts[:-1]:tree=tree[part][1]
 entry,children=tree[parts[-1]];assert children is None;s=entry.stream
 if not s.data_length:return b''
 return RunlistStream(fs.filesystem,fs.runlist(s.location,not_fragmented=bool(s.flags.not_fragmented)),s.data_length,fs.sector_size).read()
for output in r['outputs']:
 p=w/output['file'];assert hashlib.file_digest(p.open('rb'),'sha256').hexdigest()==output['sha256'];assert p.stat().st_size==output['sectors']*512
 with p.open('rb') as f:
  f.seek(3);assert f.read(8)==b'EXFAT   ';f.seek(64);assert struct.unpack('<Q',f.read(8))[0]==output['start'];f.seek(0);fs=ExFAT(f)
  paths={'Users/Fs2Install/Documents/persistent.txt':b'FS2_INSTALL_PERSISTENT\n'} if p.name=='snapshot-copy.img' else {'Documenti/esterno.txt':b'EXTERNAL_NATIVE_GUEST','Documenti/caffè.txt':bytes((i*17+23)&255 for i in range(65537)),'Documenti/vuoto.txt':b''}
  for name,expected in paths.items():
   data=file_bytes(fs,name);assert data==expected,(p.name,name,len(data),len(expected));checks.append({'image':p.name,'path':name,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()})
result={'passed':True,'reader':'dissect.fat','version':importlib.metadata.version('dissect.fat'),'real_raw_partitions_not_vhd':True,'checks':checks,'windows_tested':False}
(receipt.parent/'independent-exfat-reader.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
