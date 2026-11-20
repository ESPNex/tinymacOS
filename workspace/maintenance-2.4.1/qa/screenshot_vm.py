#!/usr/bin/env python3
"""Retail QMP screenshot harness, ordinary USB input, no debug log dependency."""
import argparse,json,os,shutil,socket,subprocess,time
from pathlib import Path
from PIL import Image
class VM:
 def __init__(self,iso,tag,pointer='tablet'):
  self.tag=tag;self.w=Path('/var/tmp/tmc-maintenance-qa')/tag;self.w.mkdir(parents=True,exist_ok=True)
  sock=self.w/'qmp.sock'
  if sock.exists():sock.unlink()
  env={**os.environ,'TOOLROOT':'/var/tmp/tmc/tools/root','TMC_BUILD':'/var/tmp/tmc-maintenance-241','ISO':str(iso),'STATE':str(self.w/'state'),'LIVE':'1','TMC_POINTER':pointer,'TMC_NET':'1'}
  self.p=subprocess.Popen(['bash','tools/run-qemu.sh','-display','none','-serial','file:'+str(self.w/'serial.log'),'-qmp','unix:'+str(sock)+',server=on,wait=off'],cwd='/home/user/tinymacOS',env=env,stdout=(self.w/'stdout.log').open('wb'),stderr=subprocess.STDOUT)
  for _ in range(150):
   if sock.exists():break
   if self.p.poll() is not None:raise RuntimeError((self.w/'stdout.log').read_text())
   time.sleep(.1)
  self.s=socket.socket(socket.AF_UNIX);self.s.settimeout(20);self.s.connect(str(sock));self.f=self.s.makefile('rwb',buffering=0);self.f.readline();self.q('qmp_capabilities')
 def q(self,cmd,**args):
  self.f.write(json.dumps({'execute':cmd,'arguments':args}).encode()+b'\n')
  while True:
   r=json.loads(self.f.readline())
   if 'error'in r:raise RuntimeError(r)
   if 'return'in r:return r['return']
 def key(self,k):
  self.q('input-send-event',events=[{'type':'key','data':{'down':True,'key':{'type':'qcode','data':k}}}]);time.sleep(.05)
  self.q('input-send-event',events=[{'type':'key','data':{'down':False,'key':{'type':'qcode','data':k}}}]);time.sleep(.065)
 def type(self,s):
  for c in s:self.key({'-':'minus'}.get(c,c))
 def shot(self,name):
  f=self.w/'frame.ppm';self.q('screendump',filename=str(f));im=Image.open(f).convert('RGB');p=Path(__file__).resolve().parents[1]/'evidence'/(self.tag+'-'+name+'.png');im.save(p);return im
 def close(self):
  if self.p.poll() is None:
   try:self.q('quit');self.p.wait(timeout=10)
   except Exception:self.p.terminate();self.p.wait(timeout=10)
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('iso');p.add_argument('tag');p.add_argument('--recovery',action='store_true');p.add_argument('--pointer',default='tablet');a=p.parse_args()
 v=VM(a.iso,a.tag,a.pointer)
 try:
  if a.recovery:
   # R is delivered via the USB keyboard during the loader key-check interval.
   for _ in range(75):
    im=v.shot('boot-probe')
    if im.size==(1280,800) and im.getpixel((40,40))==(0,0,0) and sum(im.getpixel((640,y))[0]>200 for y in range(330,440))>12:
     for _ in range(8):v.key('r')
     break
    time.sleep(.1)
   time.sleep(10);v.shot('recovery')
  else:
   time.sleep(15);v.shot('setup');v.type('tester');v.key('tab');v.type('test-password-285');time.sleep(.3);v.shot('password')
 finally:v.close()
