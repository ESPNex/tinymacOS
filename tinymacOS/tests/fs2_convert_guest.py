"""Real offline Recovery UI conversion/deactivation on a GUI-installed extent.
No host formatting, filesystem writes, secret logging or injected OS actions.
"""
from pathlib import Path
import ast,contextlib,hashlib,io,json,os,struct,time
import cv2,numpy as np
from PIL import Image
from retail_guest import Retail
from boot_recovery import RecoveryKey
import pixels,retail_guest
# Reuse only the pure UI drivers/key pixel decoder; never run the fixture test.
source=Path(__file__).with_name('encrypted_install_reset_guest.py')
names={'Private','Locked','Recovery','utility','terminal','volume_unlock','read_rendered_key'}
module=ast.parse(source.read_text());exec(compile(ast.Module(body=[n for n in module.body if isinstance(n,(ast.ClassDef,ast.FunctionDef)) and n.name in names],type_ignores=[]),str(source),'exec'))
base=Path(os.environ['TMC_QA_DIR']);iso=Path(os.environ['TMC_STAGE_ISO']);native=os.environ.get('TMC_CONVERT_4KN')=='1'
f=json.loads((base/('native4k-install-guest-result.json' if native else 'fs2-install-guest-result.json')).read_text());assert f['passed'] and f['iso_sha256']==hashlib.file_digest(iso.open('rb'),'sha256').hexdigest()
w=Path(f['work']);ev=w/'evidence';disk=w/'target/system.raw'
font=Path(os.environ['TMC_FONT']).read_bytes();pixels.b=font;pixels.nf,pixels.ng=struct.unpack_from('<II',font,4);pixels.codes={c:j for j,c in enumerate(struct.unpack_from(f'<{pixels.ng}I',font,12))};pixels.start=12+pixels.ng*4;pixels.pix=pixels.start+pixels.nf*(pixels.ng*8+4);pixels.word.cache_clear()
old=retail_guest.locate;retail_guest.locate=lambda im,text,faces=(0,1,2,3,5,6,8),threshold=.88:old(im,text,faces,threshold)
os.environ.update(TMC_STORAGE='nvme',TMC_NVME_LBA_BYTES='4096' if native else '512',TMC_MEM='512',TMC_SMP='2',TMC_POINTER='tablet',OCR_ROOT=os.environ['TOOLROOT'])
with disk.open('rb') as stream:stream.seek((8192 if native else 1024)+128+32);a,b=struct.unpack('<QQ',stream.read(16));a,b=a*(8 if native else 1),(b+1)*(8 if native else 1)-1
class Installed(Private):initial='Fs2Install'
r={'resumed_after_unacknowledged_preparation':os.environ.get('TMC_CONVERT_RESUME')=='1','passed':False,'physical_hardware_tested':False,'native_logical_bytes':4096 if native else 512,'work':str(w),'iso_sha256':f['iso_sha256'],'kernel_sha256':f['kernel_sha256'],'host_formatted_target':False,'host_prepared_encrypted_fixture':False,'checks':[]}
def ok(s):r['checks'].append(s);print('PASS',s,flush=True)
def public_hash():
 raw=disk.read_bytes();return [hashlib.sha256(raw[:a*512]).hexdigest(),hashlib.sha256(raw[(b+1)*512:]).hexdigest()]
def panel(g):
 utility(g,'Terminale');g.visible('Sessione locale');g.cmd('fdesetup manage');x,y=g.visible('FileVault',faces=(3,));return x-28,y-22

def state(g,text,timeout=300):
 x,y=g.visible('FileVault',faces=(3,));return g.visible(text,timeout,box=(x+112,y+88,x+400,y+145),faces=(1,))

def shutdown(g):
 g.key('ctrl-w');time.sleep(.5);g.key('ctrl-w');time.sleep(.5);utility(g,'Terminale');g.visible('Sessione locale');g.cmd('off');g.p.wait(timeout=20)
try:
 if os.environ.get('TMC_CONVERT_NORMALIZE')=='1':
  with disk.open('rb') as stream:stream.seek((b+1)*512-4096);jh=stream.read(16)
  pending=struct.unpack_from('<I',jh,12)[0] if jh[:8]==b'TMCVX004' else 0
  with Recovery('/not-attached',w,ev,'convert-normalize-existing',disk='target',boot_disk=True) as g:
   X,Y=panel(g)
   if pending:
    g.visible('Riprendi conversione');g.click(X+130,Y+260);g.type('conversion-guest-285');g.key('ret')
    if pending==2:state(g,'Disattivo',600)
    else:g.visible('Sblocca volume',600)
   if pending!=2:
    g.visible('Sblocca volume');g.click(X+130,Y+260);g.type('conversion-guest-285');g.key('ret');state(g,'Attivo');g.click(X+130,Y+260);g.visible('Disattiva la cifratura');g.type('conversion-guest-285');g.key('ret');state(g,'Disattivo',600)
   shutdown(g)
 before=public_hash()
 resume=os.environ.get('TMC_CONVERT_RESUME')=='1'
 if resume:
  class SavedScreen:
   def visible(self,*args,**kwargs):return old(self.snap(),'Conferma gli ultimi 8 caratteri della chiave:',(0,1,2,3,5,6,8),threshold=.88)[1]
   def snap(self):return Image.open(w/'convert-enable-snap.ppm').convert('RGB')
  key=read_rendered_key(SavedScreen())
 with Recovery('/not-attached',w,ev,'convert-enable',disk='target',boot_disk=True) as g:
  X,Y=panel(g)
  if resume:
   g.visible('Riprendi conversione');g.click(X+130,Y+260);g.type(key);g.key('ret');g.visible('Sblocca volume',600);g.click(X+130,Y+260);g.type('conversion-guest-285');g.key('ret');state(g,'Attivo');g.shot('same-extent-conversion-resumed');shutdown(g)
  else:
   g.visible('Attiva FileVault');g.click(X+130,Y+260);g.visible('Attivazione FileVault');g.type('conversion-guest-285');g.key('tab');g.type('conversion-guest-285');g.secret_page=True;g.key('ret');g.visible('Salva la chiave di recupero',300);key=read_rendered_key(g)
   g.key('ret');g.visible('Gli 8 caratteri non coincidono.');g.type(key[-8:]);g.key('ret');g.secret_page=False;g.visible('Sblocca volume',600);g.click(X+130,Y+260);g.type('conversion-guest-285');g.key('ret');state(g,'Attivo');g.shot('same-extent-conversion-active');shutdown(g)
 assert public_hash()==before
 with disk.open('rb') as stream:stream.seek(a*512);assert stream.read(8)==b'TMCFV003'
 raw=disk.read_bytes()[a*512:(b+1)*512];assert b'IMPORTED_FROM_LIVE_ACCOUNT' not in raw and b'FS2_INSTALL_PERSISTENT' not in raw and b'Fs2Install' not in raw
 ok('Actual Recovery enables same-extent FV003 after recovery-key acknowledgement; complete DATA scan has no original private account/document plaintext; all public bytes unchanged')
 with Locked('/not-attached',w,ev,'convert-cold-encrypted',disk='target',boot_disk=True) as g:
  g.type('incorrect-password');g.key('ret');g.visible('Password non corretta',300);volume_unlock(g,key);terminal(g);g.cmd('cat Documents/imported.txt','IMPORTED_FROM_LIVE_ACCOUNT');g.cmd('cat Documents/persistent.txt','FS2_INSTALL_PERSISTENT');g.cmd('diskutil apfs snapshot verify installer-guest','Snapshot valido:',timeout=300);g.cmd('off');g.p.wait(timeout=20)
 ok('Cold UEFI boot rejects wrong password, accepts recovery key plus existing account; imported documents and retained CoW snapshot verify after conversion')
 with Recovery('/not-attached',w,ev,'convert-disable',disk='target',boot_disk=True) as g:
  X,Y=panel(g);g.visible('Sblocca volume');g.click(X+130,Y+260);g.type('conversion-guest-285');g.key('ret');state(g,'Attivo');g.click(X+130,Y+260);g.visible('Disattiva la cifratura');g.type(key);g.key('ret');state(g,'Disattivo',600);g.shot('same-extent-deactivation-completed');shutdown(g)
 assert public_hash()==before
 with disk.open('rb') as stream:stream.seek(a*512);assert stream.read(8)==b'TMCBFS2\0'
 with Installed('/not-attached',w,ev,'convert-cold-plaintext',disk='target',boot_disk=True) as g:
  g.unlock();terminal(g);g.cmd('cat Documents/imported.txt','IMPORTED_FROM_LIVE_ACCOUNT');g.cmd('cat Documents/persistent.txt','FS2_INSTALL_PERSISTENT');g.cmd('diskutil apfs snapshot verify installer-guest','Snapshot valido:',timeout=300);g.cmd('diskutil verifyVolume disk0','Il volume risulta valido.',timeout=300);g.cmd('off');g.p.wait(timeout=20)
 assert public_hash()==before;r['public_extents_sha256_unchanged']=before
 ok('Actual Recovery deactivates with independent recovery credential; plaintext capacity/account/files/CoW snapshot survive cold boot and full verification, all public partition bytes unchanged');r['passed']=True
except Exception as e:r['failure']=str(e);raise
finally:(base/('native4k-convert-guest-result.json' if native else 'fs2-convert-guest-result.json')).write_text(json.dumps(r,indent=2)+'\n')
