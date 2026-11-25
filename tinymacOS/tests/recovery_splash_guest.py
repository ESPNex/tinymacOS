"""First Recovery-frame regression: no late BSP progress over the AP compositor.
Six disposable optical boots: BIOS/UEFI, 1/2/4 CPUs. No real disks.
"""
from pathlib import Path
import hashlib,json,os,struct,tempfile,time
import numpy as np
from retail_guest import Retail
import pixels
root=Path(__file__).resolve().parents[1];base=Path(os.environ['TMC_QA_DIR']);iso=Path(os.environ['TMC_STAGE_ISO'])
font=Path(os.environ['TMC_FONT']).read_bytes();pixels.b=font;pixels.nf,pixels.ng=struct.unpack_from('<II',font,4);pixels.codes={c:j for j,c in enumerate(struct.unpack_from(f'<{pixels.ng}I',font,12))};pixels.start=12+pixels.ng*4;pixels.pix=pixels.start+pixels.nf*(pixels.ng*8+4);pixels.word.cache_clear()
w=Path(tempfile.mkdtemp(prefix='tmc-recovery-splash-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir();r={'passed':False,'work':str(w),'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'physical_hardware_tested':False,'checks':[]}
os.environ.update(OCR_ROOT=os.environ['TOOLROOT'],TMC_MEM='512',TMC_POINTER='tablet',TMC_BIOS_ISO=str(iso))
class Probe(Retail):
 def wait(self,*a,**k):
  deadline=time.monotonic()+65;sent=0;first=None;self.samples=0
  while time.monotonic()<deadline:
   im=self.snap();a=np.asarray(im)
   if im.size==(1280,800):
    # The ordinary R window, no firmware variables or extra boot devices.
    if a[378,640,0]==255 and np.max(a[40,40])==0 and time.monotonic()-sent>.15:
     self.key('r',dt=.01);sent=time.monotonic()
    # Full panel is present past the bar's last scanline, avoiding captures
    # which bisect the initial top-to-bottom framebuffer copy.
    panel=all(np.all(a[y,x]==45) for x,y in [(325,250),(325,500),(955,500)]) and np.all(a[10,800]==36)
    if panel:
     if first is None:first=time.monotonic();self.shot('first-panel-'+self.tag)
     assert np.all(a[489:495,544:736]==45),'Boot progress pixels survived or reappeared over Recovery'
     self.samples+=1
     if time.monotonic()-first>=2 and self.samples>=12:return
   if time.monotonic()+1>deadline:break
   time.sleep(.025)
  raise AssertionError('No clean first Recovery frame in boot window')
try:
 for fw in ['uefi','bios']:
  for cpus in [1,2,4]:
   os.environ['TMC_SMP']=str(cpus)
   with Probe(iso,w,ev,f'{fw}-{cpus}cpu',live=True,launcher=root/'tools/run-bios-iso.sh' if fw=='bios' else None) as g:
    g.visible('Ripristina da una copia locale');g.visible('Utility',box=(0,0,900,34));g.shot('recovery-clean-'+g.tag)
    r['checks'].append({'firmware':fw,'cpus':cpus,'clean_early_samples':g.samples,'first_frame_and_two_seconds_no_progress_residue':True})
    print('PASS first Recovery frame and early idle:',fw,cpus,'CPUs,',g.samples,'clean samples',flush=True)
 r['passed']=True
finally:(base/'recovery-splash-result.json').write_text(json.dumps(r,indent=2)+'\n')
