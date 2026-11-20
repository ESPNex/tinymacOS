#!/usr/bin/env python3
"""Explicit regression reproduction against published 21B7, not a final media PASS."""
import hashlib,json,os,sys,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2];sys.path.insert(0,str(root/'tinymacOS/tests'))
from retail_guest import Retail
from input_pixels import cursor
iso=Path('/var/tmp/tmc245-input2-final/tinymacOS-2.4.5-OpenCore-Installer-x86_64.iso')
os.environ.update(TMC_TEST_INPUT='xhci-hub',TMC_TEST_FIRMWARE='uefi',TMC_POINTER='mouse',TMC_MEM='512',TOOLROOT='/var/tmp/tmc/tools/root',OCR_ROOT='/var/tmp/tmc/tools/root')
w=Path(tempfile.mkdtemp(prefix='tmc-old-hub-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
j={'reproduced':False,'not_final_media_qa':True,'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'work':str(w)}
try:
 assert j['iso_sha256']=='2d164de30042f026f107d573d485398e1930a96b692de11a9934976abd62e17a'
 with Retail(iso,w,ev,'old-hub',live=True,launcher=root/'tinymacOS/tools/run-input-qa.sh') as g:
  usb=g.hmp('info usb');assert 'QEMU USB Hub' in usb and 'QEMU USB Mouse' in usb and 'USB Keyboard' not in usb;j['usb']=usb
  j['mice_before_selection']=g.req('query-mice');m=next(m for m in g.req('query-mice') if m['name']=='QEMU HID Mouse');g.hmp('mouse_set '+str(m['index']));j['mice']=g.req('query-mice')
  assert next(m for m in j['mice'] if m['current'])['name']=='QEMU HID Mouse'
  try:cursor(g,762,482)
  except AssertionError:j['mouse_movement_failed']=True
  else:raise AssertionError('21B7 unexpectedly has a working mouse behind this hub')
  g.enroll();g.visible('Preferiti');g.shot('old-hub-ps2-works-mouse-does-not')
  j['native_ps2_keyboard_login_passed']=True;j['reproduced']=True
finally:
 (Path(__file__).parent/'old-hub-reproduction.json').write_text(json.dumps(j,indent=2)+'\n');print(json.dumps(j,indent=2))
