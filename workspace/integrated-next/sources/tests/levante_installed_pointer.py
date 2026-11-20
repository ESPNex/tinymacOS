"""2.4.5: real USB mouse -> installed login clicks, both firmwares, no ISO.
Fixture must be the disposable 2.4.5 bios_installer_qa.py disk, cloned per profile.
"""
import hashlib,json,os,shutil,tempfile,time
from pathlib import Path
import numpy as np
from guest import ROOT
from retail_guest import Retail
home=ROOT.parent;release=home/'workspace/release-2.4.5'
receipt=json.loads((release/'bios-installer-result.json').read_text());assert receipt['passed']
src=Path(receipt['external_work'])/'target/system.raw';assert str(src).startswith('/var/tmp/')
w=Path(tempfile.mkdtemp(prefix='tmc245-installed-pointer-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
os.environ.update(TOOLROOT='/var/tmp/tmc/tools/root',TMC_BUILD='/var/tmp/tmc245-final',TMC_POINTER='mouse',TMC_SMP='2')
sprite=np.frombuffer((ROOT/'assets/symbols.rgba').read_bytes(),dtype=np.uint8).reshape(-1,64,64,4)[15]
record={'passed':False,'physical_hardware_tested':False,'iso_sha256':receipt['iso_sha256'],'external_evidence':str(ev),'profiles':{}}
class Locked(Retail):
 initial='BiosDisk'
 def mouse(self,down):
  self.req('input-send-event',events=[{'type':'btn','data':{'button':'left','down':down}}]);time.sleep(.12)
def cursor(g,x,y):
 g.move(70,75);bg=np.asarray(g.snap(),dtype=np.int32);n=26;ix=np.arange(n)*64//n;p=sprite[ix[:,None],ix].astype(np.int32);a=p[:,:,3:4];mask=a[:,:,0]>=200
 region=bg[y-2:y-2+n,x-3:x-3+n];expect=np.where(a==255,p[:,:,:3],(region*(256-a)+p[:,:,:3]*a)//256)
 g.move(x,y);end=time.monotonic()+3
 while True:
  actual=np.asarray(g.snap(),dtype=np.int32)[y-2:y-2+n,x-3:x-3+n]
  if np.max(np.abs(actual-expect)[mask])<=2 and np.max(np.abs(actual-region)[mask])>=3:return
  assert time.monotonic()<end,(g.tag,x,y,'cursor mismatch')
  time.sleep(.05)
try:
 for fw in ['bios','uefi']:
  state=w/fw;state.mkdir();disk=state/'system.raw';shutil.copyfile(src,disk);os.environ['DISK']=str(disk)
  with Locked('/not-attached',w,ev,fw,disk=fw,boot_disk=True,launcher=ROOT/'tools/run-bios-installed.sh' if fw=='bios' else None) as g:
   blocks=g.req('query-block');assert str(disk) in str(blocks) and '.iso' not in str(blocks)
   mice=g.req('query-mice');assert any(m['current'] and not m['absolute'] and 'HID' in m['name'] for m in mice)
   cursor(g,762,482);cursor(g,142,613);g.shot(fw+'-locked-cursor')
   g.key('f11');g.visible('Puntatori configurati: 1',faces=(5,));g.shot(fw+'-login-diagnostics');g.key('f11')
   g.click(600,626);g.type('local-test-285');g.click(746,628);g.visible('Preferiti',40)
   cursor(g,1003,361);g.shot(fw+'-mouse-unlocked');g.key('f2');g.visible('Sessione locale');g.cmd('sw_vers','ProductVersion:    2.4.5');g.cmd('cat Documents/bios-proof.txt','BIOS_DISK_PERSISTENT')
   record['profiles'][fw]={'passed':True,'no_iso_attached':True,'mouse_sprite_at_locked_login':True,'diagnostics_before_auth':True,'password_field_and_arrow_clicked':True,'desktop_sprite':True,'version':'2.4.5','persistent_document':True}
 record['passed']=True
finally:(release/'installed-pointer-result.json').write_text(json.dumps(record,indent=2)+'\n')
print('PASS installed locked login: USB mouse pixel checks, password-field/arrow clicks, BIOS and UEFI without ISO')
