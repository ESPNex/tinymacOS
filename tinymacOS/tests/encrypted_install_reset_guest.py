"""Actual GUI FV003 fresh install + irreversible Recovery re-enrollment.
No host creation, formatting, account import or ciphertext editing of DATA.
Only disposable QEMU images; key readback uses rendered pixels, never guest RAM.
"""
from pathlib import Path
import contextlib, hashlib, io, json, os, struct, tempfile, time
import cv2, numpy as np
from retail_guest import Retail
from boot_recovery import RecoveryKey
import pixels, retail_guest
base=Path(os.environ['TMC_QA_DIR']);base.mkdir(parents=True,exist_ok=True)
iso=Path(os.environ['TMC_STAGE_ISO']);build=Path(os.environ['TMC_BUILD'])
w=Path(tempfile.mkdtemp(prefix='tmc-encrypted-install-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir();disk=w/'target/system.raw'
font=Path(os.environ['TMC_FONT']).read_bytes();pixels.b=font;pixels.nf,pixels.ng=struct.unpack_from('<II',font,4);pixels.codes={c:j for j,c in enumerate(struct.unpack_from(f'<{pixels.ng}I',font,12))};pixels.start=12+pixels.ng*4;pixels.pix=pixels.start+pixels.nf*(pixels.ng*8+4);pixels.word.cache_clear()
oldlocate=retail_guest.locate;retail_guest.locate=lambda im,text,faces=(0,1,2,3,5,6,8),threshold=.88:oldlocate(im,text,faces,threshold)
os.environ.update(TMC_STORAGE='nvme',TMC_MEM='512',TMC_SMP='2',TMC_POINTER='tablet',OCR_ROOT=os.environ['TOOLROOT'])
r={'passed':False,'release_ready':False,'work':str(w),'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'kernel_sha256':hashlib.file_digest((build/'KernelCache').open('rb'),'sha256').hexdigest(),'host_formatted_target':False,'host_prepared_encrypted_fixture':False,'encrypted_installer_acceptance':False,'encrypted_reset_acceptance':False,'in_place_conversion_acceptance':False,'physical_hardware_tested':False,'secure_erase':False,'reset_is_atomic_or_rollback_safe':False,'recovery_credential_logged':False,'checks':[]}
def ok(text):r['checks'].append(text);print('PASS',text,flush=True)
class Private(Retail):
 secret_page=False
 def type(self,text):
  with contextlib.redirect_stdout(io.StringIO()):return super().type(text)
 def ocr(self,*args,**kwargs):
  if self.secret_page:return '[recovery credential screen redacted]'
  return super().ocr(*args,**kwargs)
 def shot(self,name):
  if not self.secret_page:return super().shot(name)
class Locked(Private):initial='Sblocca Macintosh HD'
class Recovery(RecoveryKey,Private):recovery_text='Ripristina da una copia locale'
def digest(start,end):
 h=hashlib.sha256()
 with disk.open('rb') as f:
  f.seek(start);left=end-start
  while left:
   p=f.read(min(left,1<<20));assert p;h.update(p);left-=len(p)
 return h.hexdigest()
def data_range():
 with disk.open('rb') as f:f.seek(1024+128+32);return struct.unpack('<QQ',f.read(16))
def data_digest():
 a,b=data_range();return digest(a*512,(b+1)*512)
def public_hash():
 a,b=data_range();return [digest(0,a*512),digest((b+1)*512,disk.stat().st_size)]
def terminal(g):g.key('f2');g.visible('Sessione locale')
def utility(g,label):
 x,y=g.visible('Utility',box=(0,0,850,34));g.click(x+12,y+8);x,y=g.visible(label,box=(0,34,950,300));g.click(x+25,y+8)
def volume_unlock(g,credential,account='Fs2Install',password='local-test-285',setup=False):
 g.type(credential);g.key('ret')
 if setup:g.visible('account',300);return
 g.visible(account,300);g.visible('Inserisci la password',180);g.type(password);g.key('ret');g.visible('Preferiti',300)
def recovery_unlock(g,credential):
 utility(g,'Terminale');g.visible('Sessione locale');g.cmd('fdesetup manage');x,y=g.visible('FileVault',faces=(3,));X,Y=x-28,y-22;g.visible('Bloccato');g.click(X+140,Y+260);g.visible('Sblocca il volume');g.type(credential);g.key('ret');g.visible('Attivo',300);g.key('ctrl-w');time.sleep(.5);g.key('ctrl-w');time.sleep(.5)
def read_rendered_key(g,uppercase=False):
 # Decode the 64 visible cells only. No guest memory/clipboard/secret logs.
 # Full 8px cell templates avoid assuming blank side-bearings between glyphs.
 g.secret_page=True
 label='Conferma gli ultimi 8 caratteri della chiave:'
 x,y=g.visible(label);im=g.snap()
 gray=cv2.cvtColor(np.asarray(im),cv2.COLOR_RGB2GRAY).astype(np.float64)
 def glyph(c,face):
  at=pixels.start+face*(4+pixels.ng*8);height=struct.unpack_from('<I',pixels.b,at)[0]
  advance,width,offset=struct.unpack_from('<HHI',pixels.b,at+4+8*pixels.codes[ord(c)])
  return np.frombuffer(pixels.b,np.uint8,count=height*width,offset=pixels.pix+offset).reshape(height,width),advance
 # Recover the uncropped label origin from the SAME shipped TFN2 template.
 parts=[glyph(c,0) for c in label];canvas=np.zeros((parts[0][0].shape[0],sum(a for _,a in parts)+4),np.uint8);at=0
 for img,advance in parts:
  canvas[:,at:at+img.shape[1]]=np.maximum(canvas[:,at:at+img.shape[1]],img);at+=advance
 yy,xx=np.nonzero(canvas);origin_x=x-max(0,int(xx.min())-1);origin_y=y-max(0,int(yy.min())-1)
 alphabet='0123456789ABCDEF' if uppercase else '0123456789abcdef'
 templates=np.array([glyph(c,5)[0].reshape(-1) for c in alphabet],dtype=np.float64)
 assert templates.shape==(16,21*8),'Unexpected secret mono-cell ABI'
 templates-=templates.mean(axis=1,keepdims=True);templates/=np.linalg.norm(templates,axis=1,keepdims=True)
 best=None
 for dy in range(-2,3):
  for dx in range(-2,3):
   cells=[]
   for row in (origin_y-76+dy,origin_y-49+dy):
    for col in range(32):
     left=origin_x+dx+col*8;cell=gray[row:row+21,left:left+8]
     if cell.shape!=(21,8):raise AssertionError('Secret cell bounds invalid')
     cells.append(cell.reshape(-1))
   cells=np.array(cells);cells-=cells.mean(axis=1,keepdims=True);norm=np.linalg.norm(cells,axis=1,keepdims=True)
   if np.any(norm==0):continue
   scores=np.abs((cells/norm)@templates.T);order=np.argsort(scores,axis=1);ids=order[:,-1]
   top=scores[np.arange(64),ids];margin=top-scores[np.arange(64),order[:,-2]]
   candidate=(float(top.min()),float(margin.min()),ids)
   if best is None or candidate[:2]>best[:2]:best=candidate
 assert best is not None and best[0]>=.98 and best[1]>=.005,'Recovery fixed-cell pixel readback ambiguous (secret omitted)'
 return ''.join(alphabet[i] for i in best[2])
def prepare_encryption(g,password,cancel=False):
 g.key('e');g.visible('Nuove credenziali del volume' if isinstance(g,Recovery) else 'Proteggi la nuova installazione');g.type(password);g.key('tab');g.type(password);g.secret_page=True;g.key('ret');g.visible('Salva la chiave di recupero');key=read_rendered_key(g)
 if cancel:
  g.key('esc');g.visible('Conferma la reinstallazione' if isinstance(g,Recovery) else 'Conferma l’installazione');g.secret_page=False;return None
 g.key('ret');g.visible('Gli 8 caratteri non coincidono.');g.type(key[-8:]);g.key('ret');g.visible('Chiave confermata');g.secret_page=False;return key
def recovery_off(g):utility(g,'Terminale');g.visible('Sessione locale');g.cmd('off');g.p.wait(timeout=20)
try:
 with Private(iso,w,ev,'encrypted-real-install',disk='target',live=False) as g:
  g.enroll(name='Fs2Install');terminal(g);g.cmd('echo FV003_GUI_ONLY_PRIVATE_30B16 > Documents/private-proof.txt');g.key('f11');g.visible('Installa tinymacOS Tramontana');g.key('ret');g.visible('Seleziona la destinazione');g.key('down');g.key('ret');g.visible('Conferma l’installazione');before=digest(0,disk.stat().st_size)
  prepare_encryption(g,'fresh-volume-285',cancel=True);assert digest(0,disk.stat().st_size)==before
  recovery=prepare_encryption(g,'fresh-volume-285');assert digest(0,disk.stat().st_size)==before
  g.key('spc');g.key('ret');g.visible('Il disco è pronto',300);g.shot('actual-encrypted-installer-completed');g.key('ret');g.p.wait(timeout=20)
 a,b=data_range()
 with disk.open('rb') as f:f.seek(a*512);assert f.read(8)==b'TMCFV003'
 public=public_hash();r['encrypted_installer_acceptance']=True
 ok('Actual GUI creates FV003 and imports live account/document directly; cancel and recovery-key acknowledgement occur before any target write; no host formatting')
 with Locked('/not-attached',w,ev,'encrypted-install-cold-password',disk='target',boot_disk=True) as g:
  g.type('incorrect-password');g.key('ret');g.visible('Password non corretta',300);volume_unlock(g,'fresh-volume-285');terminal(g);g.cmd('cat Documents/private-proof.txt','FV003_GUI_ONLY_PRIVATE_30B16');g.cmd('fdesetup status','FileVault is On.');g.cmd('echo BEFORE_FV003_GUI_BACKUP > Documents/backup-proof.txt');g.cmd('off');g.p.wait(timeout=20)
 ok('Cold native NVMe boot rejects wrong volume password, accepts configured volume password, separately authenticates imported account and reads its private document')
 with Recovery('/not-attached',w,ev,'encrypted-install-backup',disk='target',boot_disk=True) as g:
  recovery_unlock(g,recovery);utility(g,'Crea una copia locale');g.visible('Confermi l’operazione');g.click(820,470);g.visible('Operazione completata.',300);g.key('ret');recovery_off(g)
 with Locked('/not-attached',w,ev,'encrypted-install-modify',disk='target',boot_disk=True) as g:
  volume_unlock(g,recovery);terminal(g);g.cmd('echo CHANGED_FV003_GUI_BACKUP > Documents/backup-proof.txt');g.cmd('off');g.p.wait(timeout=20)
 with Recovery('/not-attached',w,ev,'encrypted-install-restore',disk='target',boot_disk=True) as g:
  recovery_unlock(g,'fresh-volume-285');g.click(540,265);g.click(880,557);g.visible('Confermi l’operazione');g.click(820,470);g.visible('Operazione completata.',300);g.key('ret');recovery_off(g)
 with Locked('/not-attached',w,ev,'encrypted-install-restored',disk='target',boot_disk=True) as g:
  volume_unlock(g,'fresh-volume-285');terminal(g);g.cmd('cat Documents/backup-proof.txt','BEFORE_FV003_GUI_BACKUP');g.cmd('cat Documents/private-proof.txt','FV003_GUI_ONLY_PRIVATE_30B16');g.cmd('off');g.p.wait(timeout=20)
 ok('Installer-created encrypted volume accepts independent recovery credential; native Recovery creates and restores real encrypted local backup; restored documents survive cold boot')
 with Recovery('/not-attached',w,ev,'encrypted-reset-new-enrollment',disk='target',boot_disk=True) as g:
  recovery_unlock(g,'fresh-volume-285');g.click(550,355);g.click(880,557);g.visible('Per reinstallare il sistema');g.key('ret');g.visible('Seleziona la destinazione');g.key('down');g.key('ret');g.visible('Conferma la reinstallazione');before=data_digest()
  g.key('spc');g.key('ret');g.visible('Reinstallazione completata',300);assert data_digest()==before;g.key('ret');g.visible('Ripristina da una copia locale')
  g.click(550,355);g.click(880,557);g.visible('Per reinstallare il sistema');g.key('ret');g.visible('Seleziona la destinazione');g.key('down');g.key('ret');g.visible('Conferma la reinstallazione');assert data_digest()==before
  ok('Same native Recovery installer reinstalls the sealed system on its GUI-created encrypted volume while the complete DATA hash stays identical')
  prepare_encryption(g,'renewed-volume-285',cancel=True);assert data_digest()==before
  renewed=prepare_encryption(g,'renewed-volume-285');g.visible('Conferma inizializzazione cifrata');assert data_digest()==before;g.key('spc');g.key('ret');g.visible('Dati inizializzati',300);g.shot('encrypted-reset-new-keys-completed');assert public_hash()==public;g.key('ret');g.visible('Ripristina da una copia locale');recovery_off(g)
 with Locked('/not-attached',w,ev,'encrypted-reset-setup',disk='target',boot_disk=True) as g:
  for old in ('fresh-volume-285',recovery):g.type(old);g.key('ret');g.visible('Password non corretta',300)
  volume_unlock(g,'renewed-volume-285',setup=True);g.enroll(name='Fs2Renew',password='renew-account-285');terminal(g);g.cmd('cat Documents/private-proof.txt');g.cmd('echo STATUS_$?','STATUS_1');g.cmd('diskutil apfs snapshot verify Installazione');g.cmd('echo STATUS_$?','STATUS_1')
  g.cmd('echo AFTER_ENCRYPTED_REENROLLMENT > Documents/new-proof.txt');g.cmd('off');g.p.wait(timeout=20)
 with Locked('/not-attached',w,ev,'encrypted-reset-cold-recovery',disk='target',boot_disk=True) as g:
  volume_unlock(g,renewed,account='Fs2Renew',password='renew-account-285');terminal(g);g.cmd('cat Documents/new-proof.txt','AFTER_ENCRYPTED_REENROLLMENT');g.shot('encrypted-reset-new-account-persistent');g.cmd('off');g.p.wait(timeout=20)
 assert public_hash()==public;r['encrypted_reset_acceptance']=True
 ok('Explicit irreversible Recovery reset pre-confirms fresh credentials, cancel leaves DATA unchanged, replaces volume key and both credential slots, rejects old password/recovery, runs new account setup and persists new documents across reboot; public extents unchanged')
 markers=(b'TMCBFS2\x00',b'Fs2Install',b'Fs2Renew',b'private-proof.txt',b'new-proof.txt',b'FV003_GUI_ONLY_PRIVATE_30B16',b'AFTER_ENCRYPTED_REENROLLMENT')
 with disk.open('rb') as f:
  f.seek(a*512);left=(b-a+1)*512;tail=b''
  while left:
   part=f.read(min(left,1<<20));assert part;left-=len(part);window=tail+part;assert all(marker not in window for marker in markers);tail=window[-128:]
 r['public_partition_sha256_unchanged']=public;r['passed']=True
 ok('Offline entire-DATA scan finds none of the tested plaintext account, filename, filesystem or private-document markers; not a physical secure-erasure proof')
except Exception as e:r['failure']=str(e);raise
finally:
 for p in w.glob('*.ppm'):p.unlink()
 (base/'encrypted-install-reset-guest-result.json').write_text(json.dumps(r,indent=2)+'\n')
