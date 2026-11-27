"""Fixed-work window dragging latency: each real mouse report must produce its
exact expected focused traffic-light pixels before sending the next report.
This avoids treating a CPU-saturated continuous drag as a throughput measure.
"""
import hashlib,json,os,tempfile,time
from pathlib import Path
import numpy as np
from retail_guest import Retail
iso=Path(os.environ['TMC_TEST_ISO']);w=Path(tempfile.mkdtemp(prefix='tmc-drag-step-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
os.environ.update(TMC_POINTER='mouse',TMC_MEM='768',TMC_SMP='2')
r={'passed':False,'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'work':str(w),'latencies_seconds':[],'reports':24,'physical_hardware_tested':False}
try:
 with Retail(iso,w,ev,'drag-step',live=True) as g:
  g.enroll('Trascina');g.key('f2');g.visible('Sessione locale')
  for i in range(6):
   g.key('f2');g.cmd('open -a Calcolatrice');time.sleep(.5)
  x,y=g.visible('Calcolatrice',box=(0,34,1280,690),faces=(2,))
  g.move(x+30,y+5);time.sleep(1)
  def marker():
   a=np.asarray(g.snap().crop((0,y-15,1280,y+22)))
   return (a[:,:,0]>240)&(a[:,:,1]>60)&(a[:,:,1]<150)&(a[:,:,2]>50)&(a[:,:,2]<150)
  initial=marker();assert 50<int(initial.sum())<180,('ambiguous focused red light',int(initial.sum()))
  before=g.snap().crop((1040,240,1190,610)).tobytes();shift=0
  g.mouse(True)
  for dx in [10]*12+[-10]*12:
   start=time.monotonic();g.hmp('mouse_move '+str(dx)+' 0');g.mx+=dx;shift+=dx
   target=np.roll(initial,shift,axis=1)
   while True:
    if np.array_equal(marker(),target):break
    assert time.monotonic()-start<5,'window failed to follow pointer by exact report delta'
    time.sleep(.01)
   r['latencies_seconds'].append(time.monotonic()-start)
  g.mouse(False);time.sleep(1)
  assert np.array_equal(marker(),initial)
  assert g.snap().crop((1040,240,1190,610)).tobytes()==before,'vacated drag path has trails'
  g.snap().save(ev/'drag-restored.png');r['passed']=True
finally:
 Path(os.environ['TMC_UI_RESULT']).write_text(json.dumps(r,indent=2)+'\n')
 for p in w.glob('*.ppm'):p.unlink()
 print(w,flush=True)
