"""Actual live UI refusal of attached retired formats; whole disk SHA unchanged.
Only a newly created /var/tmp image is used. No host/user disks accepted.
"""
from pathlib import Path
import hashlib,json,os,struct,tempfile,time
from retail_guest import Retail
import pixels,retail_guest
root=Path(__file__).resolve().parents[1];qa=Path(os.environ['TMC_QA_DIR']);qa.mkdir(parents=True,exist_ok=True);iso=Path(os.environ['TMC_STAGE_ISO'])
w=Path(tempfile.mkdtemp(prefix='tmc-retired-native-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir();disk=w/'retired.raw'
font=Path(os.environ['TMC_FONT']).read_bytes();pixels.b=font;pixels.nf,pixels.ng=struct.unpack_from('<II',font,4);pixels.codes={c:j for j,c in enumerate(struct.unpack_from(f'<{pixels.ng}I',font,12))};pixels.start=12+pixels.ng*4;pixels.pix=pixels.start+pixels.nf*(pixels.ng*8+4);pixels.word.cache_clear()
old=retail_guest.locate;retail_guest.locate=lambda im,text,faces=(0,1,2,3,5,6,8),threshold=.96:old(im,text,faces,threshold)
os.environ.update(TMC_STORAGE='nvme',TMC_MEM='512',TMC_SMP='2',TMC_POINTER='tablet',OCR_ROOT=os.environ['TOOLROOT'])
# Detection-only signatures plus opaque sentinel payloads. This is not an old
# filesystem generator: the removed layout is never interpreted or imported.
raw=bytearray(40<<20);raw[510:512]=b'\x55\xaa'
for i,magic in enumerate([b'TMCBFS1\0',b'TINYFS1\0']):
 at=2048+i*32768;off=446+i*16;raw[off+4]=0xda;struct.pack_into('<II',raw,off+8,at,32768);raw[at*512:at*512+8]=magic
 raw[(at+128)*512:(at+128)*512+4096]=bytes((j*17+i+29)&255 for j in range(4096))
disk.write_bytes(raw);before=hashlib.sha256(raw).hexdigest();del raw
launcher=w/'media.sh';launcher.write_text('#!/bin/bash\nexec '+str(root/'tools/run-qemu.sh')+' -drive if=none,id=olddata,format=raw,file='+str(disk)+' -device nvme,drive=olddata,serial=RETIREDNATIVE "$@"\n');launcher.chmod(0o755)
r={'passed':False,'work':str(w),'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'before_disk_sha256':before,'checks':[],'physical_hardware_tested':False,'windows_tested':False}
def ok(s):r['checks'].append(s);print('PASS',s,flush=True)
try:
 with Retail(iso,w,ev,'legacy-refused',live=True,launcher=launcher) as g:
  g.enroll('SoloTMCFS2');g.key('f2');g.visible('Sessione locale');g.cmd('diskutil list','non supportato')
  for part in [1,2]:
   g.cmd('ls /Volumes/disk0s'+str(part));g.cmd('echo RETIRED_NOT_MOUNTED_$?','RETIRED_NOT_MOUNTED_1')
   g.cmd('echo FORBIDDEN > /Volumes/disk0s'+str(part)+'/injected.txt');g.cmd('echo RETIRED_NOT_WRITABLE_$?','RETIRED_NOT_WRITABLE_1')
   g.cmd('diskutil resizeVolume disk0s'+str(part)+' 12 --confirm');g.cmd('echo RETIRED_NO_RESIZE_$?','RETIRED_NO_RESIZE_1')
  ok('Actual Terminal inventory identifies both retired formats as unsupported; namespace lookup, writes and confirmed resize refuse')
  g.cmd('open -a "Utility Disco"');g.visible('Interni',faces=(2,));g.key('down');g.visible('non supportato')
  assert not old(g.snap(),'Mostra nel Finder',(0,),.985)
  g.shot('legacy-unsupported-no-finder');ok('Actual Utility Disco identifies the retired partition and offers no Mostra nel Finder mount action')
  g.key('f2');g.visible('Sessione locale');g.cmd('off');g.p.wait(timeout=20)
 after=hashlib.file_digest(disk.open('rb'),'sha256').hexdigest();assert after==before;r['after_disk_sha256']=after;ok('After actual poweroff, entire attached disk SHA is identical; no conversion, repair, resize or implicit format')
 r['passed']=True
except Exception as e:r['failure']=str(e);raise
finally:(qa/'legacy-rejection-guest-result.json').write_text(json.dumps(r,indent=2)+'\n')
