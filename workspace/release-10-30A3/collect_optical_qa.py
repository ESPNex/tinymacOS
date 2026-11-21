#!/usr/bin/env python3
"""Bind actual optical selector/pointer captures to the production ISO and harness."""
import hashlib,json,shutil
from pathlib import Path
r=Path(__file__).resolve().parent
iso=Path('/var/tmp/tmc30a3-final/tinymacOS-10-x86_64.iso')
def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
digest=sha(iso);cases=[]
ev=r/'evidence';ev.mkdir(exist_ok=True)
for bus in ['xhci','ehci']:
 for key in ['r','f12']:
  name=f'recovery-pointer-bios-{bus}-{key}.json';j=json.loads((r/name).read_text())
  assert j['passed'] and j['iso_sha256']==digest and len(j['checks'])==2 and j['ordinary_key_hold_ms']==300
  assert {v['native_key'] for v in j['boot_selection'].values()}=={key}
  logname=f'optical-bios-{bus}-{key}.log';shutil.copyfile(f'/var/tmp/30a3-production-matrix4-{bus}-{key}.log',r/logname)
  captures=[]
  for pointer in ['tablet','mouse']:
   src=Path(f'/var/tmp/tmc30a3-production-matrix4-{bus}-{key}/evidence/{pointer}-rec-terminale.png')
   dst=ev/f'optical-bios-{bus}-{key}-{pointer}.png';shutil.copyfile(src,dst)
   captures.append({'file':str(dst.relative_to(r)),'sha256':sha(dst)})
  cases.append({'bus':bus,'key':key,'passed':True,'result':name,'result_sha256':sha(r/name),'log':logname,'log_sha256':sha(r/logname),'captures':captures})
negative=json.loads((r/'negative-original-30A2-result.json').read_text())
assert negative['passed'] is False and negative['selector_failure_reproduced'] and negative['same_final_harness']
assert sha(r/negative['log'])==negative['log_sha256']
uefi=json.loads((r/'recovery-pointer-uefi.json').read_text());assert uefi['passed'] and uefi['iso_sha256']==digest
for pointer in ['tablet','mouse']:
 src=Path(f'/var/tmp/tmc30a3-production-optical-uefi4/evidence/{pointer}-rec-terminale.png');shutil.copyfile(src,ev/f'optical-uefi-{pointer}.png')
out={'passed':True,'iso_sha256':digest,'cases':cases,'cold_boot_pointer_cases':8,'keyboard_buses':['ehci','xhci'],'native_selector_keys':['r','f12'],'no_installed_disk_or_external_Recovery_attached':True,'actual_shell_checks':['sw_vers','diskutil info /'],'source_image_verified':'/Recovery/BaseSystem.dmg','BIOS_MEMDISK_origin_note':'The optical BIOS firmware exposes its embedded FAT as a UEFI volume. Its textual origin label is not proof of the host backing; QMP query-block verifies only this ISO is attached.','harness_sha256':sha(r/'recovery_pointer_qa.py'),'launcher_sha256':sha(r/'run-optical-recovery-input.sh'),'ordinary_key_hold_ms':300,'loader_key_hold_ms':120,'keyboard_throughput_tested':False,'negative_original_30A2_control':negative,'physical_hardware_tested':False,'physical_USB_mouse_resolution_confirmed':False}
(r/'supplementary-selector-result.json').write_text(json.dumps(out,indent=2,ensure_ascii=False)+'\n')
print('PASS all 8 actual optical BIOS selector/pointer cases, original negative control, independent UEFI cases, hash-bound captures')
