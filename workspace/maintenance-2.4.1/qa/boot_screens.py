#!/usr/bin/env python3
"""Capture the actual loader, NOT OVMF/TianoCore, and compare boot-hint region."""
import json,sys,time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from screenshot_vm import VM
root=Path(__file__).resolve().parents[1]
results={}
for tag,iso in [('before','/var/tmp/tmc-before-maintenance/tinymacOS-2.4.0-x86_64.iso'),('after','/var/tmp/tmc-maintenance-241/tinymacOS-2.4.1-x86_64.iso')]:
 v=VM(iso,tag+'-loader')
 try:
  deadline=time.monotonic()+45
  while time.monotonic()<deadline:
   im=v.shot('scan')
   if im.size==(1280,800) and im.getpixel((640,378))==(255,255,255) and im.getpixel((40,40))==(0,0,0) and im.getpixel((730,492))==(52,52,52):break
   time.sleep(.04)
  else:raise AssertionError('Actual tinymacOS loader not observed')
  # The hint is painted immediately after the initial logo/bar. Allow it to finish.
  time.sleep(.035);im=v.shot('boot')
  region=im.crop((400,520,900,540));count=sum(p!=(0,0,0) for p in region.get_flattened_data())
  results[tag]={'loader_logo_and_progress_bar_observed':True,'hint_region_nonblack_pixels':count}
 finally:v.close()
assert results['before']['hint_region_nonblack_pixels']>100,results
assert results['after']['hint_region_nonblack_pixels']==0,results
(root/'evidence/boot-hint.json').write_text(json.dumps(results,indent=2))
print('PASS:',results)
