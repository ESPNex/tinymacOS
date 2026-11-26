#!/usr/bin/env python3
"""Independent readback of FINAL optical live/install/Recovery fonts and kernels.
Does not rebuild/modify media. All extracted temporary files are external.
"""
from pathlib import Path
import os,sys,tempfile,struct,json,subprocess,hashlib,plistlib,zlib
root=Path(__file__).resolve().parents[1];iso=Path(sys.argv[1]).resolve();out=Path(sys.argv[2]).resolve();assert str(iso).startswith('/var/tmp/') and str(out).startswith('/var/tmp/')
tr=Path(os.environ['TOOLROOT']);env={**os.environ,'PATH':str(tr/'usr/bin')+':'+str(tr/'usr/sbin')+':'+os.environ['PATH'],'LD_LIBRARY_PATH':str(tr/'usr/lib/x86_64-linux-gnu')}
def digest(b):return hashlib.sha256(b).hexdigest()
lock=json.loads((root/'assets/interface-font-lock.json').read_text());record={'passed':False,'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'fonts':{},'kernels':{},'catalog_sha256':{}}
def catalog(b):
 assert b[:8]==b'TMVOL2\0\0' and digest(b[:-32])==b[-32:].hex();n,s=struct.unpack_from('<II',b,8);assert s==64;names=[];files={}
 for i in range(n):
  parent,directory,offset,size,name=struct.unpack_from('<iIII48s',b,16+i*s);name=name.split(b'\0')[0].decode();path='' if parent<0 else names[parent]+'/'+name;names.append(path)
  if not directory:assert offset+size<=len(b)-32;files[path]=b[offset:offset+size]
 return files
with tempfile.TemporaryDirectory(prefix='tmc-hotfix-audit-',dir='/var/tmp') as work:
 w=Path(work)
 def run(*args):subprocess.run(list(map(str,args)),env=env,check=True,capture_output=True)
 def iso_file(path,name):
  p=w/name;run('xorriso','-osirrox','on','-indev',iso,'-extract',path,p);return p
 def fat_file(image,path,name):
  p=w/name;run('mcopy','-o','-i',image,'::'+path,p);return p
 live=catalog(iso_file('/System/Library/CoreServices/System.vol','live.vol').read_bytes())
 installed=catalog(fat_file(iso_file('/Install.esp','install.fat'),'/System/Library/CoreServices/System.vol','disk.vol').read_bytes())
 dmg=iso_file('/Recovery/BaseSystem.dmg','base.dmg').read_bytes();footer=dmg[-512:];assert footer[:4]==b'koly';start,size=struct.unpack_from('>QQ',footer,216);xml=plistlib.loads(dmg[start:start+size]);table=xml['resource-fork']['blkx'][0]['Data'];count=struct.unpack_from('>I',table,200)[0];raw=bytearray()
 for i in range(count):
  kind,_,sector,n,offset,length=struct.unpack_from('>IIQQQQ',table,204+40*i)
  if kind==0xffffffff:break
  assert kind==1 and sector*512==len(raw) and n*512==length;raw+=dmg[offset:offset+length]
 assert zlib.crc32(raw)==struct.unpack_from('>I',footer,88)[0];(w/'base.fat').write_bytes(raw)
 recovery=catalog(fat_file(w/'base.fat','/RECOVERY.VOL','recovery.vol').read_bytes())
 for mode,name in [('live','live.vol'),('installed','disk.vol'),('recovery','recovery.vol')]:record['catalog_sha256'][mode]=digest((w/name).read_bytes())
 for mode,files in [('live',live),('installed',installed),('recovery',recovery)]:
  font=files['/System/Library/Fonts/Interface.tfn'];assert len(font)==lock['bytes'] and digest(font)==lock['sha256'];record['fonts'][mode]={'bytes':len(font),'sha256':digest(font),'exact_locked_production_font':True}
  version=plistlib.loads(files['/System/Library/CoreServices/SystemVersion.plist']);assert version['ProductVersion']=='10.2.5' and version['ProductBuildVersion']=='30B17'
 for image,path,name in [(None,'/System/Library/KernelCollections/KernelCache','live.cache'),(w/'install.fat','/System/Library/KernelCollections/KernelCache','disk.cache')]:
  p=fat_file(image,path,name) if image else iso_file(path,name);record['kernels'][name]=digest(p.read_bytes())
 assert len(set(record['kernels'].values()))==1
record.update(passed=True,build='30B17',no_Safari_or_full_WebKit_claim=True,locked_production_font_in_all_three_namespaces=True)
out.write_text(json.dumps(record,indent=2)+'\n');print('PASS FINAL ISO: live/installed/Recovery all contain exact currently locked production font; 30B17 version and common live/install KernelCache verified')
