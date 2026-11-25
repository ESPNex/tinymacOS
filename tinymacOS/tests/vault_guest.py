"""Actual native GUI, cold boot, ciphertext readback, power-cut migration and Recovery.
Only a disposable copy of the new-release installation fixture is attached.
No passwords/recovery codes are written into the result or commit.
"""
from pathlib import Path
import os,sys,time,json,struct,hashlib,tempfile,shutil,contextlib,io
import numpy as np
from retail_guest import Retail
from boot_recovery import RecoveryKey
from nacl.bindings import crypto_aead_xchacha20poly1305_ietf_decrypt as dec
import pixels
base=Path(os.environ['TMC_QA_DIR']);iso=Path(os.environ['TMC_STAGE_ISO']);source=Path(json.loads((base/'install-result.json').read_text())['external_work'])/'target/system.raw'
w=Path(tempfile.mkdtemp(prefix='tmc-vault-native-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir();(w/'target').mkdir();disk=w/'target/system.raw';shutil.copyfile(source,disk)
font=Path(os.environ['TMC_FONT']).read_bytes();pixels.b=font;pixels.nf,pixels.ng=struct.unpack_from('<II',font,4);pixels.codes={c:j for j,c in enumerate(struct.unpack_from(f'<{pixels.ng}I',font,12))};pixels.start=12+pixels.ng*4;pixels.pix=pixels.start+pixels.nf*(pixels.ng*8+4);pixels.word.cache_clear()
os.environ.update(OCR_ROOT=os.environ['TOOLROOT'],TMC_MEM='512',TMC_POINTER='tablet')
password='local-test-285';name='private-vault-qa-10-2-2.txt';secret='TMC-NATIVE-CIPHER-SECRET-30B14'
r={'passed':False,'work':str(w),'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'physical_hardware_tested':False,'checks':[]}
def ok(s):r['checks'].append(s);print('PASS',s,flush=True)
class PrivateRetail(Retail):
 def type(self,text):
  with contextlib.redirect_stdout(io.StringIO()):return super().type(text)
class Installed(PrivateRetail):initial='BiosDisk'
class Locked(PrivateRetail):initial='Sblocca Macintosh HD'
class Recovery(RecoveryKey,PrivateRetail):recovery_text='Ripristina da una copia locale'
def origin(g):
 x,y=g.visible('FileVault',faces=(3,));return x-28,y-22
def click(g,X,Y,x,y):g.click(X+x,Y+y)
def fields(g,values):
 for i,t in enumerate(values):
  if i:g.key('tab')
  g.type(t)
 g.key('ret')
def panel(g):g.cmd('fdesetup manage');g.visible('FileVault');return origin(g)
def header():
 with disk.open('rb') as f:f.seek((51200+63632)*512);return f.read(512)
def verify_key(key):
 h=header();s=h[144:232];k=hashlib.blake2b(s[:16],key=bytes.fromhex(key),digest_size=32).digest();return dec(s[40:72]+s[72:88],h[40:56]+struct.pack('<II',2,1),s[16:40],k)
def read_key(g,X,Y):
 im=np.asarray(g.snap()).mean(axis=2);p=pixels.start+5*(4+pixels.ng*8);height=struct.unpack_from('<I',font,p)[0]
 templates=[];alphabet='0123456789ABCDEF'
 for c in alphabet:
  adv,width,off=struct.unpack_from('<HHI',font,p+4+8*pixels.codes[ord(c)]);a=np.frombuffer(font,np.uint8,count=height*width,offset=pixels.pix+off).reshape(height,width)[:,1:8].astype(float).ravel();a-=a.mean();templates.append(a/np.linalg.norm(a))
 templates=np.asarray(templates);answer=''
 for nominal in [171,198]:
  best=(-1,None)
  for y in range(Y+nominal-9,Y+nominal+10):
   for x in range(X+28-9,X+28+10):
    a=np.asarray([im[y:y+height,x+i*8+1:x+i*8+8].ravel() for i in range(32)]);a-=a.mean(axis=1,keepdims=True);a/=np.maximum(np.linalg.norm(a,axis=1,keepdims=True),1e-6);scores=np.abs(a@templates.T);z=float(scores.max(axis=1).mean())
    if z>best[0]:best=(z,''.join(alphabet[i] for i in scores.argmax(axis=1)))
  assert best[0]>.94,best[0];answer+=best[1]
 verify_key(answer);return answer

def utility(g,label):
 x,y=g.visible('Utility',box=(0,0,850,34));g.click(x+12,y+7);x,y=g.visible(label,box=(0,34,950,300));g.click(x+25,y+7)
def close(g):g.key('ctrl-w');time.sleep(.7)
try:
 with Installed('/not-attached',w,ev,'plaintext',disk='target',boot_disk=True) as g:
  g.unlock();g.key('f2');g.visible('Sessione locale');g.cmd(f'echo {secret} > Documents/{name}');g.cmd(f'cat Documents/{name}',secret)
  assert name.encode() in disk.read_bytes();X,Y=panel(g);click(g,X,Y,120,260);g.visible('Attivazione FileVault');fields(g,[password,password]);g.visible('Salva la chiave di recupero',120)
  recovery=read_key(g,X,Y);g.type(recovery[-8:]);g.key('ret');g.visible('Cifratura in corso',80)
  # Terminate the virtual machine while metadata conversion is still incomplete.
  deadline=time.monotonic()+180
  while time.monotonic()<deadline:
   h=header();progress=struct.unpack_from('<I',h,20)[0]
   if 32<progress<60000:break
   time.sleep(.3)
  else:raise AssertionError('Partial migration not observed')
  g.shot('02-migration-before-power-cut');g.p.terminate();g.p.wait(10);ok('Native Settings/FileVault GUI creates independently verifiable keyslots; interrupted in-place encryption after real disk writes')
 with Locked('/not-attached',w,ev,'resume',disk='target',boot_disk=True) as g:
  g.visible('Password o chiave di recupero');g.type(recovery);g.key('ret');g.visible('BiosDisk',900);g.visible('Inserisci la password',180);g.type(password);g.key('ret');g.visible('Preferiti',150)
  g.key('f2');g.visible('Sessione locale');g.cmd(f'cat Documents/{name}',secret);g.cmd('fdesetup status','FileVault is On.');g.shot('03-cold-boot-resumed');g.cmd('off');g.p.wait(15)
 data=disk.read_bytes();assert name.encode() not in data and secret.encode() not in data and b'BiosDisk' not in data;ok('Cold-boot recovery key resumes the conversion before account login; offline full-disk scan cannot find file name, account name or secret contents')
 # The ordinary account password also cold-unlocks the encrypted volume.
 with Locked('/not-attached',w,ev,'password-boot',disk='target',boot_disk=True) as g:
  g.type(password);g.key('ret');g.visible('Preferiti',180);g.key('f2');g.visible('Sessione locale');g.cmd(f'cat Documents/{name}',secret);g.cmd('off');g.p.wait(15);ok('Password unlock and account authentication complete before the desktop; file bytes survive a second cold boot')
 with Recovery('/not-attached',w,ev,'recovery',disk='target',boot_disk=True) as g:
  utility(g,'Terminale');g.visible('Sessione locale');g.cmd('fdesetup manage');g.visible('FileVault',faces=(3,));X,Y=origin(g);g.visible('Bloccato');click(g,X,Y,140,260);g.visible('Sblocca il volume');fields(g,['incorrect-password']);g.visible('Password errata',150);fields(g,[recovery]);g.visible('Attivo',150);g.shot('04-recovery-unlocked');close(g);close(g)
  utility(g,'Crea una copia locale');g.visible('Confermi l’operazione');g.click(820,470);g.visible('Operazione completata.',240);g.key('ret')
  g.click(550,270);g.click(880,557);g.visible('Confermi l’operazione');g.click(820,470);g.visible('Operazione completata.',240);g.key('ret');g.shot('05-recovery-encrypted-restore');ok('Recovery rejects an incorrect password; recovery key unlocks the native disk and real backup/restore run through encrypted sector I/O')
  utility(g,'Terminale');g.visible('Sessione locale');g.cmd('off');g.p.wait(15)
 with Locked('/not-attached',w,ev,'after-recovery',disk='target',boot_disk=True) as g:
  g.type(password);g.key('ret');g.visible('Preferiti',180);g.key('f2');g.visible('Sessione locale');g.cmd(f'cat Documents/{name}',secret);X,Y=panel(g)
  click(g,X,Y,150,308);fields(g,[password]);g.visible('Salva la chiave di recupero',150);newkey=read_key(g,X,Y)
  try:verify_key(recovery)
  except Exception:pass
  else:raise AssertionError('Old recovery key not revoked')
  g.type(newkey[-8:]);g.key('ret');g.visible('Attivo');ok('Native GUI rotates the recovery key and the old key no longer authenticates the on-disk keyslot')
  click(g,X,Y,135,260);fields(g,[password]);g.visible('Decifratura in corso',150);g.visible('Disattivo',900);g.shot('06-decrypted');close(g);g.key('f2');g.visible('Sessione locale');g.cmd(f'cat Documents/{name}',secret);g.cmd('off');g.p.wait(15)
 assert name.encode() in disk.read_bytes() and secret.encode() in disk.read_bytes();ok('Authenticated GUI deactivation decrypts the volume without formatting or losing data; plaintext readback verified only after deactivation')
 # Recovery code screenshot is sensitive even for disposable test accounts.
 (ev/'01-recovery-key-REDACT-ME.png').unlink(missing_ok=True)
 r['passed']=True
finally:
 # On failure the UI driver's automatic screendump could contain a new key.
 # Remove all failure images and temporary full-frame buffers, never publish secrets.
 if not r['passed']:
  for p in ev.glob('*.png'):p.unlink()
 for p in w.glob('*.ppm'):p.unlink()
 (ev/'01-recovery-key-REDACT-ME.png').unlink(missing_ok=True)
 (base/'vault-guest-result.json').write_text(json.dumps(r,indent=2)+'\n')
