"""Settings entry point and BIOS cold boot from a disposable encrypted HDD.
Cuts power on the key-save screen (before the first payload conversion), then
uses the password to resume. No recovery code is captured or published.
"""
from pathlib import Path
import os,time,json,struct,hashlib,tempfile,shutil,contextlib,io,subprocess
from retail_guest import Retail
import retail_guest,pixels
root=Path(__file__).resolve().parents[1];base=Path(os.environ['TMC_QA_DIR']);iso=Path(os.environ['TMC_STAGE_ISO'])
source=Path(os.environ['TMC_VAULT_SOURCE']) if os.environ.get('TMC_VAULT_SOURCE') else Path(json.loads((base/'install-result.json').read_text())['external_work'])/'target/system.raw'
mixed=bool(os.environ.get('TMC_VAULT_SOURCE'))
if mixed:
 from dualboot_fixtures import hashes,verify_files
 foreign_before=hashes(source)
w=Path(tempfile.mkdtemp(prefix='tmc-vault-settings-bios-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir();(w/'target').mkdir();disk=w/'target/system.raw';subprocess.run(['cp','--sparse=always',source,disk],check=True)
data_base=51200
if mixed:
 with disk.open('rb') as f:
  mbr=f.read(512)
  if mbr[450]==0xee:
   f.seek(1024);p=f.read(16384);start,end=struct.unpack_from('<QQ',p,3*128+32)
  else:
   start,count=struct.unpack_from('<II',mbr,446+3*16+8);end=start+count-1
  assert start and end>=start
 data_base=start+49152
font=Path(os.environ['TMC_FONT']).read_bytes();pixels.b=font;pixels.nf,pixels.ng=struct.unpack_from('<II',font,4);pixels.codes={c:j for j,c in enumerate(struct.unpack_from(f'<{pixels.ng}I',font,12))};pixels.start=12+pixels.ng*4;pixels.pix=pixels.start+pixels.nf*(pixels.ng*8+4);pixels.word.cache_clear()
_original=retail_guest.locate
retail_guest.locate=lambda im,text,faces=(0,1,2,3,5,6,8),threshold=.88:_original(im,text,faces,threshold)
os.environ.update(OCR_ROOT=os.environ['TOOLROOT'],TMC_MEM='512',TMC_POINTER='tablet')
r={'passed':False,'work':str(w),'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'physical_hardware_tested':False,'checks':[]}
def ok(s):r['checks'].append(s);print('PASS',s,flush=True)
class Private(Retail):
 def wait(self,*a,**k):
  if mixed and 'run-bios-installed.sh' in str(self.p.args):
   until=time.monotonic()+30
   while time.monotonic()<until:
    text=self.ocr(psm=6)
    if 'Previous' in text or 'previous' in text:self.key('2');break
    time.sleep(.2)
   else:raise AssertionError(text)
  return super().wait(*a,**k)
 def type(self,text):
  with contextlib.redirect_stdout(io.StringIO()):return super().type(text)
class Installed(Private):initial='DualBoot' if mixed else 'BiosDisk'
class Locked(Private):initial='Sblocca Macintosh HD'
password='local-test-285';name='settings-bios-cipher.txt';secret='SETTINGS-BIOS-CIPHER-30B14'
try:
 with Installed('/not-attached',w,ev,'settings',disk='target',boot_disk=True) as g:
  g.unlock();g.key('f2');g.visible('Sessione locale');g.cmd(f'echo {secret} > Documents/{name}');g.cmd(f'cat Documents/{name}',secret)
  g.key('f4');x,y=g.visible('Sicurezza e privacy');g.click(x+45,y+8);x,y=g.visible('FileVault…');g.click(x+60,y+8);g.visible('FileVault',faces=(3,));g.visible('Impostazioni di Sistema');assert pixels.locate(g.snap(),'Indietro',threshold=.98) is None;g.shot('settings-filevault-embedded');g.visible('Disattivo');x,y=g.visible('Sicurezza e privacy',box=(0,150,500,500),faces=(0,));g.click(x+40,y+7);g.visible('ACCOUNT');x,y=g.visible('FileVault…');g.click(x+50,y+7);g.visible('Disattivo');x,y=g.visible('Attiva FileVault');g.click(x+50,y+7);g.visible('Attivazione FileVault')
  g.type('wrong-password');g.key('tab');g.type('wrong-password');g.key('ret');g.visible('Password errata',180)
  # The panel erases both fields after a rejected operation and leaves focus
  # on the second field. Tab returns to the first without retaining a secret.
  g.key('tab');g.type(password);g.key('tab');g.type(password);g.key('ret');g.visible('Salva la chiave di recupero',180)
  with disk.open('rb') as f:f.seek((data_base+63632)*512);h=f.read(512)
  assert h[:8]==b'TMCFV002' and struct.unpack_from('<I',h,20)[0]==0
  g.p.terminate();g.p.wait(10)
  ok('Settings Privacy embeds FileVault inside the existing Settings window, rejects a wrong account password and creates the volume without formatting; power cut before key confirmation leaves payload unconverted')
 with Locked('/not-attached',w,ev,'bios',disk='target',boot_disk=True,launcher=root/'tools/run-bios-installed.sh') as g:
  g.type(password);g.key('ret');g.visible('Preferiti',1000);g.key('f2');g.visible('Sessione locale');g.cmd(f'cat Documents/{name}',secret);g.cmd('fdesetup status','FileVault is On.');g.shot('bios-encrypted-desktop');g.cmd('off');g.p.wait(15)
 data=disk.read_bytes();assert name.encode() not in data and secret.encode() not in data
 ok('BIOS/OpenDuet HDD-only boot unlocks with the account password, resumes the complete in-place migration before the desktop and preserves the document; offline disk scan finds neither name nor contents')
 if mixed:
  assert hashes(disk)==foreign_before;verify_files(disk,w);ok('FileVault at a relocated native data base preserves every existing FAT32/NTFS/RAW partition and independently read file')
 r['passed']=True
finally:
 if not r['passed']:
  for p in ev.glob('*.png'):p.unlink()
 for p in w.glob('*.ppm'):p.unlink()
 (base/'vault-settings-bios-result.json').write_text(json.dumps(r,indent=2)+'\n')
