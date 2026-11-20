#!/usr/bin/env python3
"""Fail-closed gate for replacement 2.4.5 / 21B7 media. No publication here."""
import hashlib,json,shutil
from pathlib import Path
from PIL import Image
r=Path(__file__).resolve().parent;home=r.parents[1]
def read(p):return json.loads(Path(p).read_text())
def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def save(name,data):(r/name).write_text(json.dumps(data,indent=2,ensure_ascii=False)+'\n')
a=read(r/'artifact.json');iso=Path('/var/tmp/tmc245-input2-final')/a['filename']
assert sha(iso)==a['sha256'] and iso.stat().st_size==a['bytes']
assert sha(Path('/var/tmp/tmc245-input2-repro2')/iso.name)==a['sha256']
assert b'Diagnostica USB' not in (iso.parent/'desktop.elf').read_bytes()
records=[]
for fw,profiles in [('uefi',['ps2','ehci','companion','uhci','ehci-kbd','companion-kbd','xhci']),('bios',['ps2','ehci','companion'])]:
 for profile in profiles:
  name='input-'+fw+'-'+profile+'.json';j=read(r/name)
  assert j['passed'] and j['iso_sha256']==a['sha256'],name;records.append((name,j))
for fw in ['bios','uefi']:
 for profile in ['ps2','ehci','companion']:
  name='installed-'+fw+'-'+profile+'.json';j=read(r/name)
  assert j['passed'] and j['iso_sha256']==a['sha256'],name;records.append((name,j))
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
# Retain a few real full-screen frames, without repainting or resizing.
ev=r/'evidence';ev.mkdir(exist_ok=True);manifest=[]
for name,frame in [('input-uefi-ps2.json','login-cursor'),('input-uefi-ehci.json','login-cursor'),('input-bios-companion.json','login-cursor'),('installed-bios-ps2.json','desktop-terminal'),('installed-uefi-ehci.json','desktop-terminal')]:
 j=read(r/name);src=Path(j['work'])/'evidence'/(frame+'.png');dst=ev/(name.removesuffix('.json')+'-'+frame+'.png')
 with Image.open(src) as im:im.save(dst,optimize=True)
 manifest.append({'source':str(src),'original_sha256':sha(src),'retained':str(dst.relative_to(r)),'sha256':sha(dst),'crop':None})
save('evidence-manifest.json',manifest)
for src,dst in [('/var/tmp/tmc-input2-final-media.log','media-qa.log'),('/var/tmp/tmc-input2-final-regression.log','kernel-regression.log'),('/var/tmp/tmc-input2-final-recovery.log','recovery.log')]:shutil.copyfile(src,r/dst)
save('final-qa.json',{'passed':True,'version':'2.4.5','build':'21B7','sha256':a['sha256'],'bytes':a['bytes'],'independent_clean_rebuild_identical':True,'f11_diagnostic_absent':True,'live_input_profiles_passed':10,'installed_input_profiles_passed':6,'input_assertions':sum(len(j['checks']) for _,j in records),'kernel_regressions':{'passed':29,'total':29},'native_kernel_matches_production':True,'installer_and_sealed_local_recovery_passed':True,'physical_hardware_verified':False,'limits':['HID input scope, not all USB device classes','OHCI companion not implemented','Cold-boot enumeration; hotplug/reconnect not implemented','EHCI high-speed hub TT code not physically or QEMU-USB2-hub validated','Bounded static controller/device/interface capacity and bounded HID parser','No universal physical compatibility or complete USB certification claim']})
print('PASS replacement media gate: 16 native input profiles, installer/restore, identical rebuild, 29/29 same-kernel regressions')
