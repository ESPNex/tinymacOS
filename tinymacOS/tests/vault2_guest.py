"""Real NVMe/kernel/Recovery acceptance of the new authenticated block envelope.
The encrypted DATA fixture is host-created AFTER real GUI installation. This
is NOT encrypted-installer acceptance or crash-safe in-place conversion.
No passwords/recovery keys are printed or copied into the result receipt.
"""
from pathlib import Path
import os,json,struct,hashlib,subprocess,time,contextlib,io
from retail_guest import Retail
from boot_recovery import RecoveryKey
import pixels,retail_guest
root=Path(__file__).resolve().parents[1];base=Path(os.environ['TMC_QA_DIR']);iso=Path(os.environ['TMC_STAGE_ISO'])
fixture=json.loads((base/'fs2-install-guest-result.json').read_text());assert fixture['passed'] and fixture['iso_sha256']==hashlib.file_digest(iso.open('rb'),'sha256').hexdigest()
w=Path(fixture['work']);assert str(w).startswith('/var/tmp/tmc-fs2-install-');disk=w/'target/system.raw';ev=w/'evidence';keyfile=w/'envelope-recovery.secret'
with disk.open('rb') as f:f.seek(1024+128+32);a,b=struct.unpack('<QQ',f.read(16))
def digest(start,end):
 h=hashlib.sha256()
 with disk.open('rb') as f:
  f.seek(start);left=end-start
  while left:
   p=f.read(min(left,1<<20));assert p;h.update(p);left-=len(p)
 return h.hexdigest()
def public_hash():return [digest(0,a*512),digest((b+1)*512,disk.stat().st_size)]
public=public_hash()
if os.environ.get('TMC_REUSE_VAULT2_FIXTURE')=='1':
 old=json.loads((base/'vault2-guest-result.json').read_text());assert old['work']==str(w) and old['iso_sha256']==fixture['iso_sha256'] and old['host_prepared_encrypted_fixture']
else:
 assert not keyfile.exists();exe=w/'encrypt-fixture'
 subprocess.run(['gcc','-O2','-fno-builtin','-I'+str(root/'include'),str(root/'tests/fs2_encrypt_fixture.c'),str(root/'storage/tmcbfs2.c'),str(root/'crypto/vault2.c'),str(root/'crypto/sha256.c'),str(root/'crypto/pbkdf.c'),str(root/'crypto/monocypher/monocypher.c'),str(root/'kernel/string.c'),'-o',str(exe)],check=True)
 subprocess.run([str(exe),str(disk),str(a),str(b-a+1),str(keyfile)],check=True)
assert public_hash()==public;recovery=keyfile.read_text();assert len(recovery)==64
font=Path(os.environ['TMC_FONT']).read_bytes();pixels.b=font;pixels.nf,pixels.ng=struct.unpack_from('<II',font,4);pixels.codes={c:j for j,c in enumerate(struct.unpack_from(f'<{pixels.ng}I',font,12))};pixels.start=12+pixels.ng*4;pixels.pix=pixels.start+pixels.nf*(pixels.ng*8+4);pixels.word.cache_clear()
oldlocate=retail_guest.locate;retail_guest.locate=lambda im,text,faces=(0,1,2,3,5,6,8),threshold=.88:oldlocate(im,text,faces,threshold)
os.environ.update(TMC_STORAGE='nvme',TMC_MEM='512',TMC_SMP='2',TMC_POINTER='tablet',OCR_ROOT=os.environ['TOOLROOT'])
chunk=bytearray(b'X'*8192);chunk[:21]=b'VAULT2_SCALABLE_128K\n';expected=hashlib.sha256(chunk*16).hexdigest()
r={'passed':False,'release_ready':False,'work':str(w),'iso_sha256':fixture['iso_sha256'],'kernel_sha256':fixture['kernel_sha256'],'host_prepared_encrypted_fixture':True,'encrypted_installer_acceptance':False,'in_place_conversion_acceptance':False,'physical_hardware_tested':False,'recovery_credential_logged':False,'large_file_sha256':expected,'checks':[]}
def ok(s):r['checks'].append(s);print('PASS',s,flush=True)
class Private(Retail):
 def type(self,text):
  with contextlib.redirect_stdout(io.StringIO()):return super().type(text)
class Locked(Private):initial='Sblocca Macintosh HD'
class Recovery(RecoveryKey,Private):recovery_text='Ripristina da una copia locale'
def terminal(g):g.key('f2');g.visible('Sessione locale')
def unlock(g,credential='local-test-285',second=False):
 g.type(credential);g.key('ret')
 if second:g.visible('Fs2Install',300);g.visible('Inserisci la password',180);g.type('local-test-285');g.key('ret')
 g.visible('Preferiti',300)
def utility(g,label):
 x,y=g.visible('Utility',box=(0,0,850,34));g.click(x+12,y+8);x,y=g.visible(label,box=(0,34,950,300));g.click(x+25,y+8)
def recovery_unlock(g,credential,wrong=False):
 utility(g,'Terminale');g.visible('Sessione locale');g.cmd('fdesetup manage');x,y=g.visible('FileVault',faces=(3,));X,Y=x-28,y-22;g.visible('Bloccato');g.click(X+140,Y+260);g.visible('Sblocca il volume')
 if wrong:g.type('incorrect-password');g.key('ret');g.visible('Password errata',300)
 g.type(credential);g.key('ret');g.visible('Attivo',300);g.shot('vault2-recovery-unlocked');g.key('ctrl-w');time.sleep(.5);g.key('ctrl-w');time.sleep(.5)
try:
 with Locked('/not-attached',w,ev,'vault2-password-boot',disk='target',boot_disk=True) as g:
  g.type('incorrect-password');g.key('ret');g.visible('Password non corretta',300);unlock(g);terminal(g);g.cmd('fdesetup status','FileVault is On.');g.cmd('cat Documents/encrypted-proof.txt','FV3_GUEST_ONLY_SECRET_30B16');g.cmd('shasum -a 256 Documents/encrypted-large.txt',expected,timeout=180)
  g.cmd('open Documents/encrypted-large.txt');g.visible('VAULT2_SCALABLE_128K');g.key('ctrl-shift-s');tx,ty=g.visible('Salva documento');X,Y=tx-32,ty-23;g.click(X+300,Y+383);g.type('encrypted-copy.txt');g.click(X+690,Y+446);g.visible('Salvato su disco');terminal(g);g.cmd('shasum -a 256 Documents/encrypted-copy.txt',expected,timeout=180)
  g.cmd('echo BEFORE_ENCRYPTED_BACKUP > Documents/encrypted-marker.txt');g.shot('vault2-native-account-document');g.cmd('off');g.p.wait(timeout=20)
 ok('Actual native NVMe boot rejects wrong password, unlocks the authenticated scalable volume before imported account login, reads private content and TextEdit streams/saves 128KiB with byte-exact hash')
 with Recovery('/not-attached',w,ev,'vault2-backup-create',disk='target',boot_disk=True) as g:
  recovery_unlock(g,recovery,wrong=True);utility(g,'Crea una copia locale');g.visible('Confermi l’operazione');g.click(820,470);g.visible('Operazione completata.',240);g.shot('vault2-encrypted-local-backup');g.key('ret');utility(g,'Terminale');g.visible('Sessione locale');g.cmd('off');g.p.wait(timeout=20)
 ok('Native internal Recovery rejects wrong password, accepts independent recovery credential and creates a real scalable encrypted local CoW backup')
 with Locked('/not-attached',w,ev,'vault2-backup-modify',disk='target',boot_disk=True) as g:
  unlock(g);terminal(g);g.cmd('echo AFTER_ENCRYPTED_BACKUP > Documents/encrypted-marker.txt');g.cmd('cat Documents/encrypted-marker.txt','AFTER_ENCRYPTED_BACKUP');g.cmd('off');g.p.wait(timeout=20)
 with Recovery('/not-attached',w,ev,'vault2-backup-restore',disk='target',boot_disk=True) as g:
  recovery_unlock(g,'local-test-285');g.click(550,270);g.click(880,557);g.visible('Confermi l’operazione');g.click(820,470);g.visible('Operazione completata.',240);g.shot('vault2-encrypted-local-restore');g.key('ret');utility(g,'Terminale');g.visible('Sessione locale');g.cmd('off');g.p.wait(timeout=20)
 with Locked('/not-attached',w,ev,'vault2-after-restore',disk='target',boot_disk=True) as g:
  unlock(g,recovery,second=True);terminal(g);g.cmd('cat Documents/encrypted-marker.txt','BEFORE_ENCRYPTED_BACKUP');g.cmd('shasum -a 256 Documents/encrypted-copy.txt',expected,timeout=180);g.cmd('cat /.snapshots/encrypted-baseline/Users/User/Documents/encrypted-proof.txt','FV3_GUEST_ONLY_SECRET_30B16');g.cmd('diskutil apfs snapshot verify encrypted-baseline','Snapshot valido:',timeout=180);g.shot('vault2-recovery-key-account-and-restored-data');g.cmd('off');g.p.wait(timeout=20)
 ok('Encrypted Recovery restore recovers the old document and 128KiB file; cold boot accepts recovery credential then account password, and encrypted snapshot contents remain valid')
 assert public_hash()==public;r['public_partition_sha256_unchanged']=public
 needles=[b'TMCBFS2\0',b'Fs2Install',b'FV3_GUEST_ONLY_SECRET_30B16',b'BEFORE_ENCRYPTED_BACKUP',b'encrypted-proof.txt']
 with disk.open('rb') as f:
  f.seek(a*512);left=(b-a+1)*512;tail=b''
  while left:
   p=f.read(min(left,1<<20));assert p;left-=len(p);scan=tail+p;assert not any(n in scan for n in needles);tail=scan[-128:]
 ok('Offline scan of the entire DATA partition finds no plaintext filesystem signature, account name, filename or private document; public partition extents remain byte-identical');r['passed']=True
except Exception as e:r['failure']=str(e);raise
finally:
 if r['passed']:keyfile.unlink(missing_ok=True)
 (base/'vault2-guest-result.json').write_text(json.dumps(r,indent=2)+'\n')
