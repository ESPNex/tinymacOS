#!/usr/bin/env python3
"""Firmware pointer protocols only: actual rendered XY and mouse entry into Terminal.
Keyboard transport also uses firmware drivers; detailed isolated Italian
keyboard regressions are separate. This is not a Linux HCD acceptance test.
"""
from pathlib import Path
import hashlib,json,os,sys,tempfile,time
import numpy as np,cv2
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'tests'))
from retail_guest import Retail
from boot_recovery import RecoveryKey
import pixels,struct
font=Path(os.environ['TMC_FONT']).read_bytes();pixels.b=font
pixels.nf,pixels.ng=struct.unpack_from('<II',font,4)
pixels.codes={c:j for j,c in enumerate(struct.unpack_from(f'<{pixels.ng}I',font,12))}
pixels.start=12+pixels.ng*4;pixels.pix=pixels.start+pixels.nf*(pixels.ng*8+4);pixels.word.cache_clear()
fw=sys.argv[1];iso=Path(os.environ.get('TMC_STAGE_ISO','/var/tmp/tmc-10.1.8/tinymacOS-10.1.8-x86_64.iso'));w=Path(tempfile.mkdtemp(prefix='tmc1017-firmware-'+fw+'-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir();cv2.setNumThreads(1)
os.environ.update(TOOLROOT='/var/tmp/tmc/tools/root',OCR_ROOT='/var/tmp/tmc/tools/root',TMC_BUILD=os.environ.get('TMC_STAGE_BUILD',str(iso.parent/'linux-hid-v12/build')),TMC_BIOS_ISO=str(iso),TMC_POINTER='mouse',TMC_MEM='512')
r={'passed':False,'firmware':fw,'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'work':str(w),'linux_hcd_port':False,'keyboard_adapter_native':False,'physical_hardware_tested':False,'checks':[]}
class Rec(RecoveryKey,Retail):
 recovery_text='Ripristina da una copia locale'
 def key(self,k,dt=.25,hold=None):
  if hold is None:hold=.30 if fw=='bios' else .04
  if fw=='bios' and '-' in k:
   parts=k.split('-');mods=parts[:-1];key=parts[-1]
   def edge(keys,down):self.req('input-send-event',events=[{'type':'key','data':{'down':down,'key':{'type':'qcode','data':p}}} for p in keys])
   edge(mods,True);time.sleep(.20);edge([key],True);time.sleep(hold);edge([key],False);time.sleep(.12);edge(list(reversed(mods)),False);time.sleep(dt);return
  return super().key(k,dt,hold)
def ok(s):r['checks'].append(s);print('PASS',fw,s,flush=True)
raw=(Path(os.environ.get('TMC_STAGE_BUILD',str(iso.parent/'linux-hid-v12/build')))/'rec.icons').read_bytes()[6*16384:7*16384];a=np.frombuffer(raw,np.uint8).reshape(64,64,4);idx=np.arange(23)*64//23;sprite=a[idx[:,None],idx[None,:]];mask=np.uint8(sprite[:,:,3]==255)
def position(g):
 image=np.asarray(g.snap());score=cv2.matchTemplate(image,sprite[:,:,:3].copy(),cv2.TM_SQDIFF,mask=mask);_,_,(x,y),_=cv2.minMaxLoc(score);region=image[y:y+23,x:x+23];assert np.max(np.abs(region[mask>0].astype(int)-sprite[:,:,:3][mask>0].astype(int)))==0;return x+3,y+2
def waitpos(g,want):
 deadline=time.monotonic()+4
 while True:
  actual=position(g)
  if actual==want:return actual
  assert time.monotonic()<deadline,(actual,want)
  time.sleep(.08)
def move(g,x,y):
 for _ in range(120):
  a,b=position(g)
  if (a,b)==(x,y):return
  dx=max(-30,min(30,x-a));dy=max(-30,min(30,y-b));g.hmp(f'mouse_move {dx} {dy}');waitpos(g,(a+dx,b+dy))
 raise AssertionError('Pointer did not reach destination')
try:
 assert (root/'recovery/usbhid.c').is_file()
 with Rec(iso,w,ev,'firmware',live=True,launcher=root/'tools/run-bios-iso.sh' if fw=='bios' else None) as g:
  g.shot('recovery-regular-home')
  background=np.asarray(g.snap());height,width=background.shape[:2]
  edge=background[80,16];glow=background[80,width//2];bottom=background[height-20,16]
  assert int(edge.max())<=8 and int(bottom.max())<=8,(edge,bottom)
  assert int(glow[0])>=int(edge[0])+15 and int(glow.max())-int(glow.min())<=1,glow
  r['background_samples_rgb']={'edge':edge.tolist(),'soft_glow':glow.tolist(),'bottom':bottom.tolist()}
  ok('Recovery uses a neutral soft-light background with near-black edges, not the former flat gray fill')
  import pixels
  title='Ripristina da una copia locale'
  image=g.snap();regular=pixels.locate(image,title,faces=(1,),threshold=.97);bold=pixels.locate(image,title,faces=(2,),threshold=.97)
  assert regular is not None and bold is None,(regular,bold)
  ok('Recovery card title matches actual 14px Regular raster, not Bold')
  x,y=position(g);g.hmp('mouse_move -60 0');waitpos(g,(x-60,y));g.hmp('mouse_move 0 -37');waitpos(g,(x-60,y-37));ok('Rendered cursor exact independent horizontal and vertical motion through firmware pointer protocol')
  move(g,600,350);start=time.monotonic();cycles=0
  while time.monotonic()-start<75:
   for dx,dy in [(13,0),(0,9),(-13,0),(0,-9)]:
    x,y=position(g);g.hmp(f'mouse_move {dx} {dy}');waitpos(g,(x+dx,y+dy))
   cycles+=1
  r['sustained_motion']={'seconds':time.monotonic()-start,'cycles':cycles,'events':cycles*4}
  g.shot('recovery-sustained-motion');ok('Continuous XY motion for at least 75 seconds, exact cursor after every report')
  move(g,400,430);g.mouse(True);g.mouse(False);move(g,1064,648);g.mouse(True);g.mouse(False);g.visible('recovery %');g.shot('firmware-mouse-terminal');ok('Two mouse clicks select Terminal and Continue in actual Recovery')
  g.cmd('sw_vers','tinymacOS Recovery 10.2 (30B11)');g.shot('recovery-version');ok('Recovery version 10.2 / 30B11; BIOS uses pre-existing 300ms firmware-QA key pacing')
 r['passed']=True
finally:(Path(os.environ.get('TMC_QA_DIR','/var/tmp/tmc-10.2'))/f'recovery-{fw}-result.json').write_text(json.dumps(r,indent=2)+'\n')
