#!/usr/bin/env python3
"""Two real virtual USB mice through original Linux mouse/URB; rendered drag.
No guest test app, replacement report parser or fabricated hardware claims.
"""
from pathlib import Path
import hashlib,json,os,struct,subprocess,sys,tempfile,time
q=Path(__file__).resolve().parent;root=q.parents[1]/'tinymacOS';sys.path.insert(0,str(root/'tests'))
from retail_guest import Retail
from input_pixels import cursor
import numpy as np
fw=sys.argv[1];assert fw in ['uefi','bios']
base=Path(os.environ.get('TMC_LINUX_STAGE','/var/tmp/tmc-10.1.7/linux-mouse-v7'));build=base/'build';iso=base/'tinymacOS-10.1.7-x86_64.iso'
w=Path(tempfile.mkdtemp(prefix=f'tmc-two-mice-{fw}-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
os.environ.update(TOOLROOT='/var/tmp/tmc/tools/root',OCR_ROOT='/var/tmp/tmc/tools/root',TMC_MEM='512',TMC_BUILD=str(build),TMC_TEST_INPUT='xhci-two-mice',TMC_TEST_FIRMWARE=fw,TMC_POINTER='mouse')
r={'passed':False,'firmware':fw,'profile':'xhci-two-mice','iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'work':str(w),'full_linux_hcd_port':False,'physical_hardware_tested':False,'checks':[]}
try:
 symbols=subprocess.check_output(['nm',str(build/'kernel.debug')],text=True).splitlines()
 def address(name):return int(next(s.split()[0] for s in symbols if s.endswith(' '+name)),16)
 for name in ['usb_mouse_irq','usb_submit_urb','tmc_linux_mouse_error']:address(name)
 # Derive ABI offsets from the actual header instead of guessing padding.
 helper=w/'offsets';subprocess.run(['gcc','-I'+str(root/'include'),'-x','c','-','-o',str(helper)],input='#include <stdio.h>\n#include "inputdiag.h"\nint main(void){printf("%zu",offsetof(InputDiag,last_buttons));}\n',text=True,check=True)
 button_offset=int(subprocess.check_output([str(helper)],text=True))
 tool='/var/tmp/tmc/tools/root';env={**os.environ,'PATH':tool+'/usr/bin:'+os.environ['PATH'],'LD_LIBRARY_PATH':tool+'/usr/lib/x86_64-linux-gnu'}
 subprocess.run(['xorriso','-osirrox','on','-indev',str(iso),'-extract','/System/Library/KernelCollections/KernelCache',str(w/'actual.cache')],env=env,check=True,capture_output=True)
 assert (w/'actual.cache').read_bytes()==(build/'KernelCache').read_bytes();r['actual_iso_kernel_equals_build']=True
 with Retail(iso,w,ev,'two-mice',live=True,launcher=root/'tools/run-input-qa.sh') as g:
  def memory(name,size,offset=0):
   p=w/'counters.bin';g.req('pmemsave',val=address(name)+offset,size=size,filename=str(p));return p.read_bytes()
  def buttons():return struct.unpack('<I',memory('input_diag',4,button_offset))[0]
  def counters():return dict(zip(['probes','attached','completions','frames','errors','rejected','disconnected','allocations','frees'],struct.unpack('<9Q',memory('tmc_linux_usb_stats',72))))
  r['before']=before=counters();assert before['attached']==2,before
  mice=[m for m in g.req('query-mice') if m['name']=='QEMU HID Mouse'];assert len(mice)==2,mice;r['mice']=mice
  def select(i):g.hmp('mouse_set '+str(mice[i]['index']));time.sleep(.1)
  select(0);g.enroll('MouseLinux');g.key('f2');g.visible('Sessione locale');cursor(g,650,182)
  # Titlebar is glass, not a flat-pixel font template. Track the exact
  # rendered white Terminal content rectangle, not a click or guest state.
  content_color=np.asarray(g.snap())[400,900].copy()
  def rectangle():
   image=np.asarray(g.snap())
   def longest(values,lo,hi):
    hits=np.all(values[lo:hi,:3]==content_color[:3],axis=1)
    edges=np.diff(np.r_[False,hits,False].astype(np.int8));starts=np.flatnonzero(edges==1);ends=np.flatnonzero(edges==-1)
    index=np.argmax(ends-starts);assert ends[index]-starts[index]>300
    return (int(starts[index]+lo),int(ends[index]+lo))
   left,right=longest(image[400],200,1200);top,bottom=longest(image[:,900],100,700)
   return (left,top,right,bottom)
  title=rectangle();g.mouse(True);assert buttons()==1
  select(1);g.hmp('mouse_move 20 0');g.mx+=20;time.sleep(.3)
  assert buttons()==1;shifted=rectangle();assert shifted==tuple(v+(20 if i%2==0 else 0) for i,v in enumerate(title)),(title,shifted)
  r['checks'].append('First mouse holds drag; second mouse zero-button motion moves actual Terminal window by +20 X without releasing it')
  # HMP tracks one GLOBAL button mask even when mouse_set changes target.
  # Force an edge for the selected second device; its zero report must not
  # release the first device. Then produce its actual left-button press.
  g.mouse(False);assert buttons()==1;g.mouse(True);select(0);g.mouse(False);assert buttons()==1
  g.hmp('mouse_move -10 15');g.mx-=10;g.my+=15;time.sleep(.3)
  shifted2=rectangle();assert shifted2==tuple(v+(-10 if i%2==0 else 15) for i,v in enumerate(shifted)),(shifted,shifted2)
  r['checks'].append('Both mice press left; first releases; second-held drag survives and actual window moves -10 X/+15 Y')
  select(1);g.mouse(True);g.mouse(False);assert buttons()==0
  select(0);g.hmp('mouse_move 20 20');g.mx+=20;g.my+=20;time.sleep(.3)
  assert buttons()==0 and rectangle()==shifted2
  r['checks'].append('Final mouse release ends drag: subsequent pointer motion leaves Terminal window stationary')
  r['after']=after=counters();assert after['completions']>before['completions'] and after['completions']==after['frames'] and not after['errors'] and not after['rejected']
  r['passed']=True;g.shot('two-mice-drag')
 print('PASS two real USB mice + actual rendered drag/button union',fw,r['after'],flush=True)
finally:(q/f'linux-two-mice-{fw}.json').write_text(json.dumps(r,indent=2,ensure_ascii=False)+'\n')
