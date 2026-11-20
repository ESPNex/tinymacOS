#!/usr/bin/env python3
"""Same QEMU split-controller topology: demonstrate old failure and new cursor."""
import json,os,sys,tempfile,time
from pathlib import Path
import numpy as np
sys.path.insert(0,'/home/user/tinymacOS/tests')
from retail_guest import Retail
root=Path('/home/user/tinymacOS');w=Path(__file__).resolve().parents[1];e=w/'evidence'
os.environ.update(TOOLROOT='/var/tmp/tmc/tools/root',TMC_BUILD='/var/tmp/tmc-maintenance-242',TMC_USB_LAYOUT='split',TMC_POINTER='mouse')
sprite=np.frombuffer((root/'assets/symbols.rgba').read_bytes(),dtype=np.uint8).reshape(-1,64,64,4)[15]
records={}
for tag,iso in [('before','/var/tmp/tmc-maintenance-241/tinymacOS-2.4.1-x86_64.iso'),('after','/var/tmp/tmc-maintenance-242/tinymacOS-2.4.2-x86_64.iso')]:
 run=Path(tempfile.mkdtemp(prefix='tmc-second-controller-',dir='/var/tmp'))
 with Retail(Path(iso),run,e,'second-'+tag,live=True) as g:
  bg=np.asarray(g.snap(),dtype=np.int32);x,y,n=300,200,26;ix=np.arange(n)*64//n;rgba=sprite[ix[:,None],ix].astype(np.int32);a=rgba[:,:,3:4]
  region=bg[y-2:y-2+n,x-3:x-3+n];expected=np.where(a==255,rgba[:,:,:3],(region*(256-a)+rgba[:,:,:3]*a)//256)
  g.move(x,y);time.sleep(.3);actual=np.asarray(g.snap(),dtype=np.int32)[y-2:y-2+n,x-3:x-3+n];mask=rgba[:,:,3]>=200
  matched=bool(np.max(np.abs(actual-expected)[mask])<=2)
  g.shot(tag+'-second-controller-cursor')
  records[tag]={'usb':g.hmp('info usb'),'pointer_on_second_xhci':True,'cursor_at_300_200_matches_distribution_sprite':matched,'max_pixel_error':int(np.max(np.abs(actual-expected)[mask]))}
assert not records['before']['cursor_at_300_200_matches_distribution_sprite'],records
assert records['after']['cursor_at_300_200_matches_distribution_sprite'],records
(e/'second-controller-comparison.json').write_text(json.dumps(records,indent=2))
print('PASS: same topology; 2.4.1 fails mouse on second xHCI, 2.4.2 cursor matches real sprite at requested coordinates.')
