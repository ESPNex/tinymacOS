"""Retail image smoke: QMP screenshot capture, no serial-debug assumptions."""
import os,sys,time,json,socket,subprocess
from pathlib import Path
root=Path('/home/user/tinymacOS');w=Path('/var/tmp/tmc-baseline-qa');w.mkdir(exist_ok=True)
ev=Path('/home/user/workspace/audit-pre-webkit');sock=w/'qmp.sock'
env={**os.environ,'TOOLROOT':'/var/tmp/tmc/tools/root','TMC_BUILD':'/var/tmp/tmc-webkit-baseline','STATE':str(w/'state'),'LIVE':'1','TMC_NET':'1'}
p=subprocess.Popen(['bash','tools/run-qemu.sh','-display','none','-serial','file:'+str(w/'serial.log'),'-qmp','unix:'+str(sock)+',server=on,wait=off'],cwd=root,env=env,stdout=(w/'stdout.log').open('wb'),stderr=subprocess.STDOUT)
try:
 for i in range(100):
  if sock.exists():break
  if p.poll() is not None:raise RuntimeError((w/'stdout.log').read_text())
  time.sleep(.1)
 s=socket.socket(socket.AF_UNIX);s.settimeout(15);s.connect(str(sock));f=s.makefile('rwb',buffering=0);f.readline()
 def q(cmd,**args):
  f.write(json.dumps({'execute':cmd,'arguments':args}).encode()+b'\n')
  while True:
   r=json.loads(f.readline())
   if 'error'in r:raise RuntimeError(r)
   if 'return'in r:return r['return']
 q('qmp_capabilities');time.sleep(20)
 q('screendump',filename=str(w/'boot.ppm'))
 from PIL import Image
 Image.open(w/'boot.ppm').save(ev/'baseline-boot.png')
 print(q('query-status'))
 (ev/'boot-qmp.json').write_text(json.dumps(q('query-status'),indent=2))
 q('quit');p.wait(timeout=10)
finally:
 if p.poll() is None:p.terminate();p.wait(timeout=10)
