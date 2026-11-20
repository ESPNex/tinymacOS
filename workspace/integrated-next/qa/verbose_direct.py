#!/usr/bin/env python3
"""Retail direct-UEFI V-key verbose, screen pixels and Terminal, no guest debug hooks."""
import os,sys,tempfile,time,json
from pathlib import Path
sys.path.insert(0,'/home/user/tinymacOS/tests')
from retail_guest import Retail
from pixels import locate
w=Path(tempfile.mkdtemp(prefix='tmc-verbose-direct-',dir='/var/tmp'));ev=Path(__file__).resolve().parents[1]/'evidence';out=ev/'verbose-direct';out.mkdir(exist_ok=True)
os.environ.update(TOOLROOT='/var/tmp/tmc/tools/root',TMC_BUILD='/var/tmp/tmc-integrated-system',TMC_SMP='2',TMC_POINTER='mouse')
class Direct(Retail):
 def wait(self,*a,**k):
  end=time.monotonic()+65;pressed=False;seen=False
  while time.monotonic()<end:
   im=self.snap()
   if im.size==(1280,800) and im.getpixel((640,378))==(255,255,255):
    self.key('v',.05,hold=.35);pressed=True
   if pressed and im.size==(1280,800) and im.getpixel((40,40))==(0,0,0) and im.crop((8,8,440,30)).convert('L').getextrema()[1]>180:
    im.save(out/'verbose-screen.png');seen=True
   if locate(im,'Crea un account'):
    record.update(key_v_sent=pressed,verbose_frame_captured=seen);assert pressed,'Never detected direct boot splash';return
   time.sleep(.08)
record={'passed':False,'scope':'direct UEFI V-key and retail command/log; not installed persistence'}
try:
 with Direct('/var/tmp/tmc-integrated-system/tinymacOS-2.4.3-x86_64.iso',w,out,'direct',live=True) as g:
  g.key('backspace');g.key('backspace');g.enroll(name='Direct');g.key('f2');g.visible('Sessione locale');g.cmd('bootverbose status','Verbose avvio corrente: attivo');g.shot('status');g.cmd('bootverbose log');g.visible('[boot] tinymacOS native kernel');g.shot('log-page0');g.cmd('bootverbose on','Preferenza non scritta');g.shot('live-write-refusal');record.update(passed=True)
finally:(out/'result.json').write_text(json.dumps(record,indent=2))
print('PASS direct UEFI V-key verbose + status/log + live immutable medium write refusal')
