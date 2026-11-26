"""Destructive factory-reset gate on a DISPOSABLE GUI-installed FS2 fixture.
Requires successful scalable backup/UI acceptance for this exact current ISO.
Never uses physical media, never formats the target on the host. Logical reset
is not secure sanitization of plaintext remnants on the device.
"""
from pathlib import Path
import os,json,struct,hashlib,time
from retail_guest import Retail
from boot_recovery import RecoveryKey
import pixels,retail_guest
base=Path(os.environ['TMC_QA_DIR']);iso=Path(os.environ['TMC_STAGE_ISO'])
fixture=json.loads((base/'fs2-install-guest-result.json').read_text());backup=json.loads((base/'fs2-backup-ui-result.json').read_text())
assert fixture['passed'] and backup['passed'] and backup['work']==fixture['work']
assert fixture['iso_sha256']==backup['iso_sha256']==hashlib.file_digest(iso.open('rb'),'sha256').hexdigest()
w=Path(fixture['work']);assert str(w).startswith('/var/tmp/tmc-fs2-install-');disk=w/'target/system.raw';ev=w/'evidence'
font=Path(os.environ['TMC_FONT']).read_bytes();pixels.b=font;pixels.nf,pixels.ng=struct.unpack_from('<II',font,4);pixels.codes={c:j for j,c in enumerate(struct.unpack_from(f'<{pixels.ng}I',font,12))};pixels.start=12+pixels.ng*4;pixels.pix=pixels.start+pixels.nf*(pixels.ng*8+4);pixels.word.cache_clear()
old=retail_guest.locate;retail_guest.locate=lambda im,text,faces=(0,1,2,3,5,6,8),threshold=.88:old(im,text,faces,threshold)
os.environ.update(TMC_STORAGE='nvme',TMC_MEM='512',TMC_SMP='2',TMC_POINTER='tablet',OCR_ROOT=os.environ['TOOLROOT'])
with disk.open('rb') as f:f.seek(1024+128+32);a,b=struct.unpack('<QQ',f.read(16))
def digest(start,end):
 h=hashlib.sha256()
 with disk.open('rb') as f:
  f.seek(start);left=end-start
  while left:
   chunk=f.read(min(left,1<<20));assert chunk;h.update(chunk);left-=len(chunk)
 return h.hexdigest()
def data_hash():return digest(a*512,(b+1)*512)
def public_hash():return [digest(0,a*512),digest((b+1)*512,disk.stat().st_size)]
def utility(g,label):
 x,y=g.visible('Utility',box=(0,0,800,34));g.click(x+12,y+8);x,y=g.visible(label,box=(0,34,900,250));g.click(x+20,y+8)
def terminal(g):g.key('f2');g.visible('Sessione locale')
class Recovery(RecoveryKey,Retail):recovery_text='Ripristina da una copia locale'
class Enrolled(Retail):initial='Fs2Reset'
r={'passed':False,'release_ready':False,'work':str(w),'iso_sha256':fixture['iso_sha256'],'kernel_sha256':fixture['kernel_sha256'],'host_formatted_target':False,'secure_erase':False,'checks':[]}
def ok(s):r['checks'].append(s);print('PASS',s,flush=True)
try:
 before=data_hash();public=public_hash()
 with Recovery('/not-attached',w,ev,'fs2-reset-recovery',disk='target',boot_disk=True) as g:
  utility(g,'Inizializza i dati utente');g.visible('I dati verranno eliminati.');g.click(450,470);time.sleep(.5);assert data_hash()==before;ok('Canceling Recovery initialization leaves the entire FS2 data partition byte-identical, including account, snapshots and large document')
  utility(g,'Inizializza i dati utente');g.visible('I dati verranno eliminati.');g.click(820,470);g.visible('Operazione completata.',180);g.shot('fs2-reset-completed');g.key('ret');utility(g,'Terminale');g.visible('Sessione locale');g.cmd('diskutil verifyVolume disk0','Il volume risulta valido.',timeout=180);g.cmd('off');g.p.wait(timeout=20)
 assert public_hash()==public;r['public_partition_sha256_unchanged']=public
 ok('Native Recovery logically resets the full scalable home/account/snapshot roots, verifies the result and leaves partition tables, sealed system, Recovery and installation source byte-identical')
 with Retail('/not-attached',w,ev,'fs2-reset-enrollment',disk='target',boot_disk=True) as g:
  g.enroll(name='Fs2Reset',password='reset-test-285');terminal(g)
  g.cmd('cat Documents/backup-marker.txt');g.cmd('echo RESET_OLD_MARKER_ABSENT_$?','RESET_OLD_MARKER_ABSENT_1')
  g.cmd('cat Documents/Pagination/large-copy.txt');g.cmd('echo RESET_OLD_LARGE_ABSENT_$?','RESET_OLD_LARGE_ABSENT_1')
  g.cmd('cat /.snapshots/paged-19/Users/User/Documents/persistent.txt');g.cmd('echo RESET_OLD_SNAPSHOT_ABSENT_$?','RESET_OLD_SNAPSHOT_ABSENT_1')
  g.cmd('echo AFTER_FACTORY_RESET > Documents/reset-proof.txt');g.cmd('cat Documents/reset-proof.txt','AFTER_FACTORY_RESET');g.shot('fs2-reset-new-account-document');g.cmd('off');g.p.wait(timeout=20)
 with Enrolled('/not-attached',w,ev,'fs2-reset-reboot',disk='target',boot_disk=True) as g:
  g.unlock(password='reset-test-285');terminal(g);g.cmd('cat Documents/reset-proof.txt','AFTER_FACTORY_RESET');g.cmd('off');g.p.wait(timeout=20)
 ok('After reset, actual installed boot requires fresh enrollment; previous documents/snapshot namespace are absent and the new account and document survive another reboot');r['passed']=True
except Exception as e:r['failure']=str(e);raise
finally:(base/'fs2-reset-guest-result.json').write_text(json.dumps(r,indent=2)+'\n')
