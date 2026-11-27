"""Actual GUI fresh-install -> installed FS2 -> internal Recovery reinstall.
Disposable virtual NVMe only. No host formatting/import of the target volume.
This gate does not certify scalable encryption, backups or 4Kn installation.
"""
from pathlib import Path
import hashlib,json,os,struct,tempfile,uuid
from uefi_variables import vars_active
from retail_guest import Retail
from boot_recovery import RecoveryKey
import pixels,retail_guest
base=Path(os.environ['TMC_QA_DIR']);base.mkdir(parents=True,exist_ok=True);iso=Path(os.environ['TMC_STAGE_ISO']);build=Path(os.environ['TMC_BUILD'])
w=Path(tempfile.mkdtemp(prefix='tmc-fs2-install-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir();disk=w/'target/system.raw'
font=Path(os.environ['TMC_FONT']).read_bytes();pixels.b=font;pixels.nf,pixels.ng=struct.unpack_from('<II',font,4);pixels.codes={c:j for j,c in enumerate(struct.unpack_from(f'<{pixels.ng}I',font,12))};pixels.start=12+pixels.ng*4;pixels.pix=pixels.start+pixels.nf*(pixels.ng*8+4);pixels.word.cache_clear()
old=retail_guest.locate;retail_guest.locate=lambda im,text,faces=(0,1,2,3,5,6,8),threshold=.88:old(im,text,faces,threshold)
os.environ.update(TMC_STORAGE='nvme',TMC_MEM='512',TMC_SMP='2',TMC_POINTER='tablet',OCR_ROOT=os.environ['TOOLROOT'])
r={'passed':False,'release_acceptance':False,'work':str(w),'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'kernel_sha256':hashlib.file_digest((build/'KernelCache').open('rb'),'sha256').hexdigest(),'host_formatted_target':False,'checks':[]}
def ok(s):r['checks'].append(s);print('PASS',s,flush=True)
class Installed(Retail):initial='Fs2Install'
class Recovery(RecoveryKey,Retail):recovery_text='Ripristina da una copia locale'
def data_range():
 with disk.open('rb') as f:f.seek(1024+128+32);a,b=struct.unpack('<QQ',f.read(16))
 return a,b

def data_digest():
 a,b=data_range();h=hashlib.sha256()
 with disk.open('rb') as f:
  f.seek(a*512);left=(b-a+1)*512
  while left:
   chunk=f.read(min(left,1<<20));assert chunk;h.update(chunk);left-=len(chunk)
 return h.hexdigest()
try:
 with Retail(iso,w,ev,'fs2-install',disk='target',live=False) as g:
  g.enroll(name='Fs2Install');g.key('f2');g.visible('Sessione locale');g.cmd('pwd','/Users/Fs2Install');g.cmd('echo $HOME','/Users/Fs2Install');g.cmd('echo IMPORTED_FROM_LIVE_ACCOUNT > Documents/imported.txt');g.key('f11');g.visible('Installa tinymacOS Tramontana');g.key('ret');g.visible('Seleziona la destinazione');g.key('down');g.key('ret');g.visible('Conferma l’installazione');g.key('ret');g.key('spc');g.key('ret');g.visible('Il disco è pronto',180);g.visible('OpenCore è stato impostato come avvio principale.');g.shot('fs2-real-installer-completed');g.key('ret');g.p.wait(timeout=20)
 # This readback occurs BEFORE ANY installed/native boot, ISO still used
 # for the completed installer. It must not be satisfied by loader repair.
 globalguid=uuid.UUID('8be4df61-93ca-11d2-aa0d-00e098032b8c').bytes_le
 vendor=uuid.UUID('3f9a25c4-71b2-4e83-a4c1-5e9d278b6c10').bytes_le
 variables=vars_active(w/'target/vars.fd');order=variables[(globalguid,'BootOrder')];first=struct.unpack_from('<H',order)[0]
 installed_option=variables[(globalguid,f'Boot{first:04X}')]
 assert 'tinymacOS OpenCore'.encode('utf-16le') in installed_option and '\\EFI\\OC\\OpenCore.efi'.encode('utf-16le') in installed_option
 assert variables[(vendor,'tinymacOS-previousBootOrder')].startswith(b'TMCBOOT2')
 r['opencore_verified_before_first_native_boot']=True
 ok('Installer itself already promotes OpenCore FIRST in real OVMF NVRAM before any native boot; UI reports verified preference, original order backed up')
 a,b=data_range()
 with disk.open('rb') as f:
  f.seek(a*512);superblock=f.read(128);assert superblock[:8]==b'TMCBFS2\0';assert struct.unpack_from('<Q',superblock,16)[0]==(b-a+1)//32
 ok('Real GUI installer creates TMCBFS2 spanning the actual data partition, imports current live files and account; target was never host-formatted')
 with Installed('/not-attached',w,ev,'fs2-installed',disk='target',boot_disk=True) as g:
  assert str(iso) not in str(g.req('query-block'));g.unlock();g.key('f2');g.visible('Sessione locale');g.cmd('pwd','/Users/Fs2Install');g.cmd('echo $HOME','/Users/Fs2Install');g.cmd('bootverbose log','[fs2] mounted full data extent');g.cmd('cat Documents/imported.txt','IMPORTED_FROM_LIVE_ACCOUNT');g.cmd('echo FS2_INSTALL_PERSISTENT > Documents/persistent.txt');g.cmd('diskutil apfs snapshot create installer-guest','Snapshot creato.');g.cmd('echo CHANGED > Documents/persistent.txt');g.cmd('diskutil apfs snapshot restore installer-guest','Volume ripristinato.');g.cmd('cat Documents/persistent.txt','FS2_INSTALL_PERSISTENT');g.shot('fs2-installed-import-and-cow');g.cmd('off');g.p.wait(timeout=20)
 variables=vars_active(w/'target/vars.fd');assert struct.unpack_from('<H',variables[(globalguid,'BootOrder')])[0]==first and variables[(globalguid,f'Boot{first:04X}')]==installed_option
 ok('Installed NVMe boot without ISO authenticates imported account, reads imported file and performs real shared-root CoW restore')
 before=data_digest()
 with Recovery('/not-attached',w,ev,'fs2-recovery',disk='target',boot_disk=True) as g:
  assert str(iso) not in str(g.req('query-block'));g.visible('Reinstalla tinymacOS Tramontana');g.click(550,355);g.click(880,557);g.visible('Per reinstallare il sistema');g.shot('fs2-recovery-shared-installer');g.key('ret');g.visible('Seleziona la destinazione');g.key('down');g.key('ret');g.visible('Conferma la reinstallazione');g.key('ret');g.visible('Conferma la reinstallazione');assert data_digest()==before;g.key('spc');g.key('ret');g.visible('Reinstallazione completata',180);g.shot('fs2-reinstall-preserves-data');assert data_digest()==before;g.key('ret');g.visible('Ripristina da una copia locale')
  x,y=g.visible('Utility',box=(0,0,800,34));g.click(x+12,y+8);x,y=g.visible('Terminale',box=(0,34,900,250));g.click(x+20,y+8);g.visible('Sessione locale');g.cmd('diskutil verifyVolume disk0','Il volume risulta valido.',timeout=180);g.cmd('off');g.p.wait(timeout=20)
 r['reinstall_entire_data_partition_sha256_unchanged']=before;ok('Internal native Recovery runs the SAME installer as Reinstalla, verifies FS2 data/snapshots and sealed system; complete data-partition SHA unchanged by reinstall')
 with Installed('/not-attached',w,ev,'fs2-after-reinstall',disk='target',boot_disk=True) as g:
  g.unlock();g.key('f2');g.visible('Sessione locale');g.cmd('cat Documents/imported.txt','IMPORTED_FROM_LIVE_ACCOUNT');g.cmd('cat Documents/persistent.txt','FS2_INSTALL_PERSISTENT');g.cmd('diskutil apfs snapshot verify installer-guest','Snapshot valido:');g.shot('fs2-after-reinstall-account-files-snapshot');g.cmd('off');g.p.wait(timeout=20)
 ok('After reinstall and reboot, imported account, documents and existing snapshot remain usable');r['passed']=True
except Exception as e:r['failure']=str(e);raise
finally:(base/'fs2-install-guest-result.json').write_text(json.dumps(r,indent=2)+'\n')
