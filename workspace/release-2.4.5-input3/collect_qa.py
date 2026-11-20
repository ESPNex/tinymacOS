#!/usr/bin/env python3
"""Fail-closed gate for replacement 2.4.5 / 21B8 media. No publication here."""
import hashlib,json,shutil
from pathlib import Path
from PIL import Image
r=Path(__file__).resolve().parent;home=r.parents[1]
def read(p):return json.loads(Path(p).read_text())
def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def save(name,data):(r/name).write_text(json.dumps(data,indent=2,ensure_ascii=False)+'\n')
a=read(r/'artifact.json');iso=Path('/var/tmp/tmc245-input3-final')/a['filename']
assert sha(iso)==a['sha256'] and iso.stat().st_size==a['bytes']
assert sha(Path('/var/tmp/tmc245-input3-repro')/iso.name)==a['sha256']
assert b'Diagnostica USB' not in (iso.parent/'desktop.elf').read_bytes()
# QA-only harness/readme edits after the build must not hide native source drift.
frozen=read(r/'source-hashes.json');checked=0
for path,digest in frozen.items():
 if path.startswith('tests/') or path in ['README.md','tools/run-input-qa.sh']:continue
 assert sha(home/'tinymacOS'/path)==digest,('build input changed after candidate',path)
 checked+=1
save('build-input-match.json',{'passed':True,'files_checked':checked,'allowed_postbuild_changes':['tests/*','README.md','tools/run-input-qa.sh'],'note':'postbuild changes only to QA/readme; independent media rebuild and five probe kernels also match'})
host=read(r/'host-contracts-result.json');assert host['passed'] and len(host['checks'])==2
for check in host['checks']:assert check['exit_code']==0 and sha(r/check['log'])==check['sha256']
for path,digest in host['source_sha256'].items():assert sha(home/'tinymacOS'/path)==digest
old=read(r/'chrome-old-reproduction.json');assert old['reproduced'] and old['iso_sha256']==a['prior_sha256']
oldhub=read(r/'old-hub-reproduction.json')
assert oldhub['reproduced'] and oldhub['iso_sha256']==a['prior_sha256']
assert oldhub['mouse_movement_failed'] and oldhub['native_ps2_keyboard_login_passed']
records=[]
for fw,profiles in [('uefi',['ps2','ehci','companion','xhci','xhci-hub','xhci-hub-kbd','xhci-nested-hub','uhci','ehci-kbd','companion-kbd']),('bios',['ps2','ehci','companion','xhci','xhci-hub','xhci-hub-kbd','xhci-nested-hub'])]:
 for profile in profiles:
  name='input-'+fw+'-'+profile+'.json';j=read(r/name)
  assert j['passed'] and j['iso_sha256']==a['sha256'],name
  expected='QEMU PS/2 Mouse' if profile=='ps2' else 'QEMU HID Mouse'
  assert j['selected_pointer_transport']==expected
  assert [m['name'] for m in j['mice'] if m['current']]==[expected]
  assert [m['name'] for m in j['final_mice'] if m['current']]==[expected]
  if profile.endswith('-kbd'):assert j['ps2_controller_present'] is False
  records.append((name,j))
for fw in ['bios','uefi']:
 for profile in ['ps2','ehci','companion','xhci-hub','xhci-nested-hub']:
  name='installed-'+fw+'-'+profile+'.json';j=read(r/name)
  assert j['passed'] and j['iso_sha256']==a['sha256'],name
  expected='QEMU PS/2 Mouse' if profile=='ps2' else 'QEMU HID Mouse'
  assert j['selected_pointer_transport']==expected
  assert [m['name'] for m in j['mice'] if m['current']]==[expected]
  assert [m['name'] for m in j['final_mice'] if m['current']]==[expected]
  if profile.endswith('-kbd'):assert j['ps2_controller_present'] is False
  records.append((name,j))
for name in ['identity-result.json','bios-installer-result.json','bios-recovery-result.json']:
 j=read(r/name);assert j['passed'],name
 if 'iso_sha256' in j:assert j['iso_sha256']==a['sha256']
suite=read(home/'workspace/webkit-tmc/test-results/milestone-tests.json')
assert suite['passed'] and suite['guest_requested'] and len(suite['cases'])==29
assert all(c['exit_code']==0 for c in suite['cases'])
save('kernel-regression-result.json',suite)
kernel=sha(iso.parent/'kernel.elf')
for kind in ['network','vm','sync','tls','threads']:
 j=read(home/('workspace/webkit-tmc/test-results/guest-'+kind+'-build.json'))
 assert j['sha256']['kernel.elf']==kernel,kind
save('kernel-production-match.json',{'passed':True,'kernel_elf_sha256':kernel,'matching_probe_builds':['network','vm','sync','tls','threads'],'note':'same kernel executable, separate QA command volumes; not browser tests'})
# Retain lossless original-pixel crops, with original hashes and explicit bounds.
ev=r/'evidence';ev.mkdir(exist_ok=True);manifest=[]
for name,frame in [('input-uefi-xhci-hub.json','login-cursor'),('input-bios-xhci-nested-hub.json','login-cursor'),('installed-uefi-xhci-hub.json','desktop-terminal')]:
 j=read(r/name);src=Path(j['work'])/'evidence'/(frame+'.png');dst=ev/(name.removesuffix('.json')+'-'+frame+'.png')
 box=(100,440,810,665) if frame=='login-cursor' else (200,120,1100,680)
 with Image.open(src) as im:im.crop(box).save(dst,optimize=True)
 manifest.append({'source':str(src),'original_sha256':sha(src),'retained':str(dst.relative_to(r)),'sha256':sha(dst),'crop':box})
ui=read(r/'chrome-result.json');assert ui['passed'] and ui['iso_sha256']==a['sha256'] and len(ui['checks'])==6
assert all(c['exterior_halo_pixels']==0 and c['changed_pixels_after_exit']==0 for c in ui['checks'])
assert ui['menu_alignment']['difference_px']<=1 and ui['selected_pointer_transport']=='QEMU HID Mouse'
for f in Path(ui['work']+'/evidence').glob('*.png'):
 if not f.name.startswith('failure'):shutil.copyfile(f,ev/f.name)
save('evidence-manifest.json',manifest)
for src,dst in [('/var/tmp/tmc-input3-final-media.log','media-qa.log'),('/var/tmp/tmc-input3-final-regression.log','kernel-regression.log'),('/var/tmp/tmc-input3-final-recovery.log','recovery.log')]:shutil.copyfile(src,r/dst)
save('final-qa.json',{'passed':True,'version':'2.4.5','build':'21B8','sha256':a['sha256'],'bytes':a['bytes'],'independent_clean_rebuild_identical':True,'f11_diagnostic_absent':True,'live_input_profiles_passed':17,'installed_input_profiles_passed':10,'input_assertions':sum(len(j['checks']) for _,j in records),'kernel_regressions':{'passed':29,'total':29},'native_kernel_matches_production':True,'mission_control_chrome_and_menu_alignment_passed':True,'installer_and_sealed_local_recovery_passed':True,'forced_pointer_transport_verified':True,'usb_keyboard_profiles_without_i8042':True,'old_usb_hub_failure_reproduced':True,'physical_hardware_verified':False,'limits':['HID input scope, not all USB device classes','OHCI companion not implemented','USB3 SuperSpeed hubs not implemented; xHCI hub support is USB2','Cold-boot enumeration; hotplug/reconnect not implemented','EHCI high-speed hub TT code not physically or QEMU-USB2-hub validated','Bounded static controller/device/interface capacity and bounded HID parser','No universal physical compatibility or complete USB certification claim']})
print('PASS replacement media gate: 27 native input profiles, installer/restore, identical rebuild, 29/29 same-kernel regressions')
