#!/usr/bin/env python3
"""Retail UI selects 1080p; real USB +/-1 reports must move the visible cursor.
No guest instrumentation. Normalize physical screenshot to exact logical pixels.
"""
import os,sys,json,tempfile,time,hashlib
from pathlib import Path
import numpy as np
import cv2
from PIL import Image
sys.path.insert(0,'/home/user/tinymacOS/tests')
from retail_guest import Retail
w=Path(__file__).resolve().parents[1];e=w/'evidence';results={}
class Scaled(Retail):
 def snap(self):
  im=super().snap();self.raw=im
  pw,ph=im.size;lw=min(1280,pw);lh=(ph*1280+pw//2)//pw if pw>1280 else ph
  a=np.asarray(im);xx=(np.arange(lw)*pw+lw-1)//lw;yy=(np.arange(lh)*ph+lh-1)//lh
  return Image.fromarray(a[yy[:,None],xx])
 def move(self,x,y):
  self.snap();pw,ph=self.raw.size;lw,lh=self.snap().size
  # Re-home with physical relative reports; not an absolute tablet.
  for i in range(18):self.hmp('mouse_move -127 -127');time.sleep(.013)
  dx=round(x*pw/lw);dy=round(y*ph/lh)
  while dx or dy:
   a=min(45,dx);b=min(45,dy);self.hmp(f'mouse_move {a} {b}');dx-=a;dy-=b;time.sleep(.014)
  time.sleep(.12)
 def shot(self,name):self.snap();self.raw.save(e/(name+'.png'))
for tag,ver in [('before','2.4.2'),('after','2.4.3')]:
 build=Path('/var/tmp/tmc-maintenance-'+ver.replace('.',''));iso=build/f'tinymacOS-{ver}-x86_64.iso';run=Path(tempfile.mkdtemp(prefix='tmc-scaled-'+tag+'-',dir='/var/tmp'))
 os.environ.update(TOOLROOT='/var/tmp/tmc/tools/root',TMC_BUILD=str(build),TMC_POINTER='mouse',TMC_USB_LAYOUT='standard',TMC_SMP='2')
 with Scaled(iso,run,e,'scaled-'+tag,live=True) as g:
  g.enroll();g.key('f4');x,y=g.visible('Schermo');g.click(x+25,y+5);g.visible('RISOLUZIONE')
  modes=[]
  for i in range(48):
   g.snap();mode=g.raw.size;modes.append(mode)
   if mode==(1920,1080):break
   x,y=g.visible('RISOLUZIONE');g.click(x+60,y+8);time.sleep(.45)
  else:raise AssertionError(('1080p mode not reached',modes))
  g.key('ctrl-w');g.move(610,350);time.sleep(.3);before=np.asarray(g.snap()).copy();g.shot(tag+'-1080p-before-tiny-reports')
  for i in range(30):g.hmp('mouse_move 1 0');time.sleep(.25)
  time.sleep(.3);after=np.asarray(g.snap()).copy();g.shot(tag+'-1080p-after-tiny-reports')
  sprite=np.frombuffer(Path('/home/user/tinymacOS/assets/symbols.rgba').read_bytes(),dtype=np.uint8).reshape(-1,64,64,4)[15]
  ix=np.arange(26)*64//26;rgba=sprite[ix[:,None],ix];mask=(rgba[:,:,3]==255).astype(np.uint8)
  def locate_cursor(im):
   m=cv2.matchTemplate(im.astype(np.float32),rgba[:,:,:3].astype(np.float32),cv2.TM_SQDIFF,mask=mask)
   score,_,pos,_=cv2.minMaxLoc(m);assert score<100,(score,pos)
   return pos
  p0=locate_cursor(before);p1=locate_cursor(after);displacement=[p1[0]-p0[0],p1[1]-p0[1]]
  moved=displacement!=[0,0];assert displacement==([20,0] if tag=='after' else [0,0]),(tag,displacement,p0,p1)
  changed=np.any(before!=after,axis=2)
  results[tag]={'version':ver,'iso_sha256':hashlib.sha256(iso.read_bytes()).hexdigest(),'usb_mouse_only':True,'cpu_count':len(g.req('query-cpus-fast')),'physical_mode':[1920,1080],'modes_visited':modes,'reports':30,'delta_per_report':[1,0],'visible_cursor_moved':moved,'expected_logical_displacement':[20,0],'changed_pixels':int(changed.sum()),'cursor_sprite_translation_verified':True,'measured_logical_displacement':displacement,'cursor_before':p0,'cursor_after':p1}
(e/'scaled-mouse.json').write_text(json.dumps(results,indent=2))
print('PASS: 1080p, 2 vCPU, 30 real USB reports of (+1,0): 2.4.2 no movement; 2.4.3 cursor translated 20 logical pixels.')
