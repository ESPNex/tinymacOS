"""Real firmware keyboard selection: Command+R enters native Recovery;
plain R and F12 do not. No NVRAM writes, synthetic recovery flag or extra disk.
"""
import hashlib,json,os,tempfile,time
from pathlib import Path
from retail_guest import Retail
iso=Path(os.environ['TMC_TEST_ISO']);w=Path(tempfile.mkdtemp(prefix='tmc-desktop4-boot-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
os.environ.update(TMC_MEM='768',TMC_SMP='2',TMC_POINTER='tablet')
r={'passed':False,'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'work':str(w),'checks':[],'physical_hardware_tested':False}
class Probe(Retail):
 boot_key='meta_l-r';recovery_expected=True
 def wait(self,*args,**kwargs):
  start=time.monotonic()
  while time.monotonic()-start<60:
   im=self.snap()
   if im.size==(1280,800) and im.getpixel((640,378))==(255,255,255) and im.getpixel((40,40))==(0,0,0) and im.getpixel((730,492))==(52,52,52):break
   time.sleep(.025)
  else:raise AssertionError('boot logo not observed')
  for i in range(9):self.key(self.boot_key,.12,hold=.20)
  self.visible('Ripristina da una copia locale' if self.recovery_expected else 'account',40)
try:
 for key,recovery in [('meta_l-r',True),('f12',False),('r',False)]:
  class Case(Probe):boot_key=key;recovery_expected=recovery
  with Case(iso,w,ev,key.replace('-','_'),live=True) as g:
   g.snap().save(ev/(key.replace('-','_')+'.png'))
   r['checks'].append({'key':key,'native_recovery':recovery,'passed':True})
 r['passed']=True
finally:
 Path(os.environ['TMC_UI_RESULT']).write_text(json.dumps(r,indent=2)+'\n')
 for p in w.glob('*.ppm'):p.unlink()
 print(w,flush=True)
