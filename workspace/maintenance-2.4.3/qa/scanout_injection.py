#!/usr/bin/env python3
"""Inject an actual stale white band in emulated GOP via QEMU GDB physical memory.
No guest changes/debug service: ISO remains production. Compare unchanged 2.4.2.
"""
import hashlib,json,os,socket,subprocess,sys,tempfile,time,struct
from pathlib import Path
import numpy as np
sys.path.insert(0,'/home/user/tinymacOS/tests')
from retail_guest import Retail
w=Path(__file__).resolve().parents[1];e=w/'evidence';results={}
class RSP:
 def __init__(self,path):
  self.s=socket.socket(socket.AF_UNIX);self.s.settimeout(10);self.s.connect(str(path))
 def command(self,text):
  b=text.encode();self.s.sendall(b'$'+b+b'#'+('%02x'%(sum(b)&255)).encode())
  while self.s.recv(1)!=b'$':pass
  out=b''
  while True:
   b=self.s.recv(1)
   if b==b'#':break
   out+=b
  checksum=self.s.recv(2);assert int(checksum,16)==sum(out)&255;self.s.sendall(b'+');return out.decode()
 def close(self):self.s.close()
for version,tag in [('2.4.2','before'),('2.4.3','after')]:
 build=Path('/var/tmp/tmc-maintenance-'+version.replace('.',''));iso=build/f'tinymacOS-{version}-x86_64.iso'
 os.environ.update(TOOLROOT='/var/tmp/tmc/tools/root',TMC_BUILD=str(build),TMC_USB_LAYOUT='standard',TMC_POINTER='mouse')
 run=Path(tempfile.mkdtemp(prefix='tmc-scanout-'+tag+'-',dir='/var/tmp'))
 with Retail(iso,run,e,'scanout-'+tag,live=True) as g:
  clean=g.snap();g.shot(tag+'-clean-setup');assert clean.size==(1280,800)
  symbols=subprocess.check_output(['nm',str(build/'kernel.debug')],text=True)
  boot=int(next(l.split()[0] for l in symbols.splitlines() if l.endswith(' boot')),16)
  sock=run/'gdb.sock';reply=g.hmp('gdbserver unix:'+str(sock)+',server=on,wait=off');assert 'Waiting' in reply,reply
  g.req('stop');rsp=RSP(sock)
  try:
   assert rsp.command('Qqemu.PhyMemMode:1')=='OK'
   raw=bytes.fromhex(rsp.command(f'm{boot:x},40'));fb=struct.unpack_from('<Q',raw)[0];width,height,stride=struct.unpack_from('<III',raw,40)
   assert (width,height)==clean.size and stride>=width
   # Same location as the password field: horizontal white pixels, independent of source canvas.
   x0,x2,y0,y2=529,753,487,491
   for y in range(y0,y2):assert rsp.command(f'M{fb+4*(y*stride+x0):x},{4*(x2-x0):x}:'+('ffffff00'*(x2-x0)))=='OK'
   g.shot(tag+'-injected-band')
  finally:rsp.close()
  dirty=np.asarray(g.snap())[y0:y2,x0:x2,:3];assert np.all(dirty==255),dirty.min()
  g.req('cont');time.sleep(2.4);actual=g.snap();g.shot(tag+'-after-idle-no-mouse')
  expected=np.asarray(clean)[y0:y2,x0:x2,:3];got=np.asarray(actual)[y0:y2,x0:x2,:3]
  repaired=bool(np.array_equal(got,expected));white=int(np.all(got==255,axis=2).sum())
  assert repaired==(tag=='after'),(tag,repaired,white)
  results[tag]={'version':version,'iso_sha256':hashlib.sha256(iso.read_bytes()).hexdigest(),'injected_actual_emulated_framebuffer':True,'band_box':[x0,y0,x2,y2],'repaired_without_mouse_or_keys':repaired,'white_pixels_remaining':white,'cpu_count':len(g.req('query-cpus-fast')),'note':'Synthetic scanout corruption; does not prove original physical cause.'}
(e/'scanout-injection.json').write_text(json.dumps(results,indent=2))
print('PASS: production 2.4.2 retains injected GOP white band; 2.4.3 repairs it while idle, no mouse/keyboard.')
