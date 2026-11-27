"""Same-workload before/after QA: real keys/mouse, framebuffer endpoints and
read-only kernel CPU accounting. No guest memory writes or benchmark syscalls.
Measurements are QEMU/TCG local results, not physical-GPU FPS claims.
"""
import hashlib,json,os,re,struct,subprocess,tempfile,time
import numpy as np
from pathlib import Path
from retail_guest import Retail
from kernel_layout import process_layout
stride,appoff,cpuoff,winoff,winbytes,seqoff,nproc=process_layout()
iso=Path(os.environ['TMC_TEST_ISO']);build=Path(os.environ['TMC_BUILD'])
w=Path(tempfile.mkdtemp(prefix='tmc-animations-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
os.environ.update(TMC_POINTER='mouse',TMC_MEM='768',TMC_SMP='2')
symbols={x.split()[-1]:int(x.split()[0],16) for x in subprocess.check_output(['nm','--defined-only',str(build/'kernel.debug')],text=True).splitlines() if len(x.split())>=3}
r={'process_abi':[stride,appoff,cpuoff,winoff,winbytes,seqoff,nproc],'passed':False,'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'work':str(w),'measurements':{},'physical_hardware_tested':False}
try:
 with Retail(iso,w,ev,'animations',live=True) as g:
  def cpu():
   p=w/'cpu.bin';g.req('pmemsave',val=symbols['proc'],size=stride*nproc,filename=str(p));data=p.read_bytes()
   r['open_windows']=sum(bool(struct.unpack_from('<i',data,off+winoff)[0]) for off in range(0,len(data),stride))
   for off in range(0,len(data),stride):
    pid,app=(struct.unpack_from('<i',data,off)[0],struct.unpack_from('<i',data,off+appoff)[0])
    if pid and app==0:return struct.unpack_from('<Q',data,off+cpuoff)[0]
   raise AssertionError('desktop not found')
  def framehash():
   # Material capture in the old build depends on the animated backdrop.
   # Measure exact opaque geometry, not unstable glass colors. Saturated orange
   # buttons/icons exclude tinted reflections; never a tolerance/fuzzy match.
   a=np.asarray(g.snap().crop((0,34,1280,690)))
   mask=(a[:,:,0]>220)&(a[:,:,1]>90)&(a[:,:,1]<190)&(a[:,:,2]<35)
   return hashlib.sha256(mask.tobytes()).hexdigest()
  def endpoint(key,target):
   start=time.monotonic();g.key(key,dt=0);n=0
   while time.monotonic()-start<12:
    n+=1
    if framehash()==target:return {'seconds_to_exact_geometry':time.monotonic()-start,'framebuffer_observations':n}
    time.sleep(.025)
   raise AssertionError(('endpoint not reached',key))
  g.enroll('Animazioni');g.key('f2');g.visible('Sessione locale')
  for i in range(6):
   g.key('f2');g.cmd('open -a Calcolatrice');time.sleep(.5)
  g.move(1100,660);time.sleep(2)
  r['timing_endpoint']='bit-exact saturated-orange opaque geometry mask; not whole-glass pixel equality'
  # Warm-up establishes independently captured final geometry targets.
  for name,key in [('mission_control','f10'),('library','ctrl-shift-a')]:
   before=framehash();g.key(key);time.sleep(4.5);opened=framehash()
   assert before!=opened,(name,'shortcut did not open overlay')
   g.snap().save(ev/(name+'-open.png'));g.key('esc');time.sleep(4.5);closed=framehash()
   # Moving the pointer changes the glass light on its next material rebuild.
   # Warm-up rebases those textures; subsequent cycles must be pixel exact.
   g.key(key);time.sleep(4.5);opened=framehash();g.key('esc');time.sleep(4.5);closed=framehash()
   samples=[]
   for cycle in range(3):
    a=cpu();enter=endpoint(key,opened);leave=endpoint('esc',closed)
    samples.append({'enter':enter,'exit':leave,'desktop_cpu_ticks':cpu()-a})
    time.sleep(.3)
   r['measurements'][name]=samples
  # Actual moving-window path, then exact reverse path. An exact final
  # opaque mask proves restored geometry; a separate vacated region checks trails.
  x,y=g.visible('Calcolatrice',box=(0,34,1280,690),faces=(2,))
  g.move(x+30,y+5);g.key('f10');time.sleep(4.5);g.key('esc');time.sleep(4.5)
  before=framehash();vacated=g.snap().crop((1040,240,1190,610)).tobytes();a=cpu();start=time.monotonic()
  g.mouse(True)
  for dx in [4]*30+[-4]*30:
   g.hmp('mouse_move '+str(dx)+' 0');g.mx+=dx;time.sleep(.016)
  g.mouse(False);time.sleep(2)
  r['measurements']['drag']={'wall_seconds':time.monotonic()-start,'desktop_cpu_ticks':cpu()-a,'reports':60}
  assert framehash()==before,'drag geometry did not return to original position'
  assert g.snap().crop((1040,240,1190,610)).tobytes()==vacated,'drag left trails in the vacated region'
  g.snap().save(ev/'drag-restored.png')
  g.key('f2');g.visible('Sessione locale');g.cmd('echo ANIMATION_INPUT_OK','ANIMATION_INPUT_OK')
  r['passed']=True
finally:
 Path(os.environ['TMC_UI_RESULT']).write_text(json.dumps(r,indent=2)+'\n')
 for p in w.glob('*.ppm'):p.unlink()
 print(w,flush=True)
