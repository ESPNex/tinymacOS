#!/usr/bin/env python3
"""Twelve exact rendered window drags with USB keyboard+mouse behind xHCI hub.
A same-ISO first matrix attempt failed its immediate post-drag position check.
This test waits for the requested framebuffer position, not just any old label.
No failed assertion is suppressed and no operating-system source is modified.
"""
import hashlib,json,os,sys,tempfile,time
from pathlib import Path
q=Path(__file__).resolve().parent;home=q.parents[1];sys.path.insert(0,str(home/'tinymacOS/tests'))
from retail_guest import Retail
from pixels import locate
iso=Path('/var/tmp/tmc-10.1.6/tinymacOS-10.1.6-x86_64.iso');w=Path(tempfile.mkdtemp(prefix='tmc1016-hub-drag-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
os.environ.update(TOOLROOT='/var/tmp/tmc/tools/root',OCR_ROOT='/var/tmp/tmc/tools/root',TMC_BUILD=str(iso.parent/'build'),TMC_TEST_ISO=str(iso),TMC_TEST_FIRMWARE='uefi',TMC_TEST_INPUT='xhci-hub-kbd',TMC_POINTER='mouse',TMC_MEM='512')
r={'passed':False,'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'work':str(w),'physical_hardware_tested':False,'drags':[],'prior_attempt':'attempts/first-input-uefi-xhci-hub-kbd.json','prior_failure_preserved':True,'prior_root_cause':'not established; initial check sampled any visible Preferiti rather than waiting for destination'}
try:
 with Retail(iso,w,ev,'drag',live=True,launcher=home/'tinymacOS/tools/run-input-qa.sh') as g:
  assert 'i8042' not in g.hmp('info qtree');usb=g.hmp('info usb');assert all(t in usb for t in ['QEMU USB Hub','QEMU USB Mouse','QEMU USB Keyboard']);r['usb']=usb
  target=next(m for m in g.req('query-mice') if m['name']=='QEMU HID Mouse');g.hmp('mouse_set '+str(target['index']));g.enroll('Trascinamento')
  for j in range(12):
   x,y=g.visible('Preferiti',faces=(0,));dx,dy=(87,41) if j%2==0 else (-87,-41);g.move(x+200,y-40);g.mouse(True);g.move(x+200+dx,y-40+dy);g.mouse(False);g.move(1050,605)
   deadline=time.monotonic()+5;expected=(x+dx,y+dy)
   while True:
    found=locate(g.snap(),'Preferiti',faces=(0,));actual=found[1] if found else None
    if actual==expected:break
    assert time.monotonic()<deadline,(j,actual,expected)
    time.sleep(.08)
   r['drags'].append({'from':[x,y],'delta':[dx,dy],'rendered_position':list(actual)});print('PASS exact hub drag',j+1,actual,flush=True)
  g.shot('twelve-exact-hub-drags');r['passed']=True
finally:(q/'hub-drag-result.json').write_text(json.dumps(r,indent=2)+'\n')
