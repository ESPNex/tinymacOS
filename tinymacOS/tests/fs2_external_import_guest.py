"""Real Terminal/Recovery external checkpoint import on two disposable NVMe.
Requires real GUI installation receipt on this exact ISO. Destination is a host
COPY of that installed medium, not host-formatted/host-generated filesystem.
Source is attached read-only by QEMU; complete SHA must remain unchanged.
"""
from pathlib import Path
import hashlib,json,os,shutil,struct,tempfile
from retail_guest import Retail
from boot_recovery import RecoveryKey
import pixels,retail_guest
root=Path(__file__).resolve().parents[1];base=Path(os.environ['TMC_QA_DIR']);iso=Path(os.environ['TMC_STAGE_ISO']);build=Path(os.environ['TMC_BUILD'])
fixture=json.loads((base/'fs2-install-guest-result.json').read_text());assert fixture['passed'] and fixture['iso_sha256']==hashlib.file_digest(iso.open('rb'),'sha256').hexdigest()
source_work=Path(fixture['work']);assert str(source_work).startswith('/var/tmp/tmc-fs2-install-');source=source_work/'target/system.raw'
w=Path(tempfile.mkdtemp(prefix='tmc-external-import-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir();dest=w/'target/system.raw';dest.parent.mkdir()
font=Path(os.environ['TMC_FONT']).read_bytes();pixels.b=font;pixels.nf,pixels.ng=struct.unpack_from('<II',font,4);pixels.codes={c:j for j,c in enumerate(struct.unpack_from(f'<{pixels.ng}I',font,12))};pixels.start=12+pixels.ng*4;pixels.pix=pixels.start+pixels.nf*(pixels.ng*8+4);pixels.word.cache_clear()
old=retail_guest.locate;retail_guest.locate=lambda im,text,faces=(0,1,2,3,5,6,8),threshold=.88:old(im,text,faces,threshold)
os.environ.update(TMC_STORAGE='nvme',TMC_MEM='512',TMC_SMP='2',TMC_POINTER='tablet',OCR_ROOT=os.environ['TOOLROOT'])
r={'passed':False,'release_acceptance':False,'work':str(w),'source_work':str(source_work),'iso_sha256':fixture['iso_sha256'],'kernel_sha256':fixture['kernel_sha256'],'host_copied_destination_from_gui_installed_source':True,'host_formatted_target':False,'source_attached_readonly':True,'checks':[]}
def ok(s):r['checks'].append(s);print('PASS',s,flush=True)
class Installed(Retail):initial='Fs2Install'
class Recovery(RecoveryKey,Retail):recovery_text='Ripristina da una copia locale'
def utility(g,label):
 x,y=g.visible('Utility',box=(0,0,800,34));g.click(x+12,y+8);x,y=g.visible(label,box=(0,34,900,250));g.click(x+20,y+8)
def digest(path,start=0,end=None):
 h=hashlib.sha256()
 with path.open('rb') as f:
  f.seek(start);left=(path.stat().st_size if end is None else end)-start
  while left:
   p=f.read(min(left,1<<20));assert p;h.update(p);left-=len(p)
 return h.hexdigest()
def data_range():
 with dest.open('rb') as f:f.seek(1024+128+32);a,b=struct.unpack('<QQ',f.read(16))
 return a*512,(b+1)*512
try:
 with Installed('/not-attached',source_work,ev,'external-donor',disk='target',boot_disk=True) as g:
  g.unlock();g.key('f2');g.visible('Sessione locale');g.cmd('echo EXTERNAL_CHECKPOINT_BYTES > Documents/external.txt');g.cmd('diskutil apfs snapshot create external-transfer','Snapshot creato.');g.cmd('echo SOURCE_LIVE_MUST_NOT_IMPORT > Documents/external.txt');g.cmd('off');g.p.wait(timeout=20)
 shutil.copyfile(source,dest);source_sha=digest(source)
 with Installed('/not-attached',w,ev,'external-destination-before',disk='target',boot_disk=True) as g:
  g.unlock();g.key('f2');g.visible('Sessione locale');g.cmd('echo DESTINATION_BEFORE_IMPORT > Documents/external.txt');g.cmd('echo DESTINATION_ONLY > Documents/destination-only.txt');g.cmd('diskutil apfs snapshot create destination-before','Snapshot creato.');g.cmd('off');g.p.wait(timeout=20)
 start,end=data_range();outside=[digest(dest,0,start),digest(dest,end)];before=digest(dest,start,end)
 launcher=w/'two-nvme.sh';launcher.write_text('#!/bin/bash\nexec '+str(root/'tools/run-qemu.sh')+' -drive if=none,id=external,format=raw,readonly=on,file='+str(source)+' -device nvme,drive=external,serial=TMCIMPORTSOURCE "$@"\n');launcher.chmod(0o755)
 with Recovery('/not-attached',w,ev,'external-recovery',disk='target',boot_disk=True,launcher=launcher) as g:
  assert str(iso) not in str(g.req('query-block'));utility(g,'Terminale');g.visible('Sessione locale');g.cmd('diskutil list','disk1');g.cmd('diskutil importBackup disk1 disk0 external-transfer','Anteprima verificata.',timeout=180);assert digest(dest,start,end)==before
  ok('Real Recovery Terminal preflights a named external NVMe checkpoint without modifying any destination data byte')
  g.cmd('diskutil importBackup disk1 disk0 missing-checkpoint','Importazione non completata:',timeout=180);assert digest(dest,start,end)==before
  g.cmd('diskutil importBackup disk0 disk0 external-transfer','Importazione rifiutata:');assert digest(dest,start,end)==before
  g.cmd('diskutil importBackup disk1 disk0 external-transfer --confirm','Checkpoint esterno importato:',timeout=180);g.shot('external-checkpoint-import-completed');g.cmd('diskutil verifyVolume disk0','Il volume risulta valido.',timeout=180);g.cmd('off');g.p.wait(timeout=20)
 assert digest(source)==source_sha and outside==[digest(dest,0,start),digest(dest,end)]
 ok('Explicit confirmed import runs real syscall/offline backend, source disk read-only and byte-identical; destination GPT, sealed system, Recovery and source partition remain byte-identical')
 with Installed('/not-attached',w,ev,'external-import-after-reboot',disk='target',boot_disk=True) as g:
  g.unlock();g.key('f2');g.visible('Sessione locale');g.cmd('cat Documents/external.txt','EXTERNAL_CHECKPOINT_BYTES');g.cmd('cat Documents/imported.txt','IMPORTED_FROM_LIVE_ACCOUNT');g.cmd('diskutil apfs snapshot verify destination-before','Snapshot valido:');g.cmd('cat /.snapshots/destination-before/Users/User/Documents/destination-only.txt','DESTINATION_ONLY');g.shot('external-import-reboot-and-retained-checkpoint');g.cmd('diskutil apfs snapshot restore destination-before','Volume ripristinato.');g.cmd('cat Documents/external.txt','DESTINATION_BEFORE_IMPORT');g.cmd('cat Documents/destination-only.txt','DESTINATION_ONLY');g.cmd('off');g.p.wait(timeout=20)
 ok('Cold installed boot authenticates imported account, reads checkpoint bytes rather than source live bytes, retains destination checkpoint and can restore previous destination state')
 r['source_entire_disk_sha256_unchanged']=source_sha;r['destination_public_extents_sha256_unchanged']=outside;r['passed']=True
except Exception as e:r['failure']=str(e);raise
finally:(base/'fs2-external-import-guest-result.json').write_text(json.dumps(r,indent=2)+'\n')
