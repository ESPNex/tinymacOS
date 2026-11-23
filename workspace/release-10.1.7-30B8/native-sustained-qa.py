#!/usr/bin/env python3
"""Sustained native mouse at the actual pre-login/setup framebuffer, no PS/2 fallback."""
import hashlib,json,os,sys,tempfile,time
from pathlib import Path
home=Path(__file__).resolve().parents[2];sys.path.insert(0,str(home/'tinymacOS/tests'))
from retail_guest import Retail
from input_pixels import cursor
profile=os.environ['TMC_TEST_INPUT'];firmware=os.environ['TMC_TEST_FIRMWARE'];assert profile in ['ehci','xhci']
iso=Path(os.environ['TMC_TEST_ISO']);w=Path(tempfile.mkdtemp(prefix='tmc1013-sustained-'+firmware+'-'+profile+'-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
os.environ.update(TMC_POINTER='mouse',TOOLROOT='/var/tmp/tmc/tools/root',OCR_ROOT='/var/tmp/tmc/tools/root',TMC_MEM='512')
r={'passed':False,'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'firmware':firmware,'profile':profile,'physical_hardware_tested':False,'work':str(w),'exact_cursor_checks':0,'cycles':0}
try:
 with Retail(iso,w,ev,'sustained',live=True,launcher=home/'tinymacOS/tools/run-input-qa.sh') as g:
  mice=g.req('query-mice');target=next(m for m in mice if m['name']=='QEMU HID Mouse');g.hmp('mouse_set '+str(target['index']))
  assert [m['name'] for m in g.req('query-mice') if m['current']]==['QEMU HID Mouse']
  assert 'USB Keyboard' not in g.hmp('info usb');r['usb']=g.hmp('info usb')
  for lap in range(12):
   for x,y in [(762,482),(142,482),(142,613),(762,613)]:cursor(g,x,y);r['exact_cursor_checks']+=1
   r['cycles']+=1
  time.sleep(2)
  for x,y in [(142,613),(762,482)]:cursor(g,x,y);r['exact_cursor_checks']+=1
  assert [m['name'] for m in g.req('query-mice') if m['current']]==['QEMU HID Mouse']
  g.shot('sustained-native');r['passed']=True
finally:(Path(__file__).parent/f'native-sustained-{firmware}-{profile}.json').write_text(json.dumps(r,indent=2)+'\n')
print('PASS native sustained mouse',firmware,profile,'12 XY reversal cycles + idle/resume, 50 exact sprite checks, PS/2 fallback excluded')
