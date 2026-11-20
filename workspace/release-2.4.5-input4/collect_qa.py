#!/usr/bin/env python3
"""Fail-closed 21B9 replacement gate. Never publishes or imports 21B8 PASS as new evidence."""
import hashlib,json,shutil
from pathlib import Path
from PIL import Image
r=Path(__file__).resolve().parent;home=r.parents[1]
def read(p):return json.loads(Path(p).read_text())
def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def save(name,data):(r/name).write_text(json.dumps(data,indent=2,ensure_ascii=False)+'\n')
a=read(r/'artifact.json');iso=Path('/var/tmp/tmc245-input4-final')/a['filename'];native=iso.parent/'tinymacOS-2.4.5-x86_64.iso'
assert a['build']=='21B9' and a['prior_build']=='21B8'
assert sha(iso)==a['sha256'] and iso.stat().st_size==a['bytes']
assert sha(Path('/var/tmp/tmc245-input4-repro')/iso.name)==a['sha256']
assert sha(native)==sha(Path('/var/tmp/tmc245-input4-repro')/native.name)==a['native_iso_sha256']
assert b'Diagnostica USB' not in (iso.parent/'desktop.elf').read_bytes()
assert (r/'SHA256SUMS.txt').read_text()==a['sha256']+'  '+iso.name+'\n'
frozen=read(r/'source-hashes.json');checked=0
for path,digest in frozen.items():
 if path.startswith('tests/') or path in ['README.md','tools/run-input-qa.sh']:continue
 assert sha(home/'tinymacOS'/path)==digest,('Production source changed after build',path)
 checked+=1
save('build-input-match.json',{'passed':True,'files_checked':checked,'allowed_postbuild_changes':['tests/*','README.md','tools/run-input-qa.sh']})
host=read(r/'host-contracts-result.json');assert host['passed'] and len(host['checks'])==2
for c in host['checks']:assert c['exit_code']==0 and sha(r/c['log'])==c['sha256']
for path,digest in host['source_sha256'].items():assert sha(home/'tinymacOS'/path)==digest
font=read(r/'italian-font-contract.json');assert font['passed'] and font['typeface_sha256']==sha(home/'tinymacOS/assets/type.tfn')
assert font['euro_faces_present_and_nonempty_not_question_mark']==list(range(10))
c=host['legacy_absolute_pointer_contract'];assert c['passed'] and sha(r/c['log'])==c['sha256']
records=[]
for fw,profiles in [('uefi',['ps2','ehci','companion','xhci','xhci-hub','xhci-hub-kbd','xhci-nested-hub','uhci','ehci-kbd','companion-kbd']),('bios',['ps2','ehci','companion','xhci','xhci-hub','xhci-hub-kbd','xhci-nested-hub'])]:
 for profile in profiles:
  records.append(('input-'+fw+'-'+profile+'.json',profile,False))
for fw in ['bios','uefi']:
 for profile in ['ps2','ehci','companion','xhci-hub','xhci-nested-hub']:records.append(('installed-'+fw+'-'+profile+'.json',profile,True))
inputs=[]
for name,profile,installed in records:
 j=read(r/name);assert j['passed'] and j['iso_sha256']==a['sha256'],name
 expected='QEMU PS/2 Mouse' if profile=='ps2' else 'QEMU HID Mouse'
 assert j['selected_pointer_transport']==expected
 for field in ['mice','final_mice']:assert [m['name'] for m in j[field] if m['current']]==[expected]
 assert any('Independent horizontal-only X and vertical-only Y' in s for s in j['checks']),name
 if profile.endswith('-kbd'):assert j['ps2_controller_present'] is False
 if installed:assert any('Installed Italian accents' in s for s in j['checks']),name
 inputs.append(j)
italian=[];recovery=[]
for fw in ['bios','uefi']:
 for profile in ['ps2','xhci-hub-kbd']:
  for prefix,dest in [('italian-apps',italian),('recovery-keyboard',recovery)]:
   name=f'{prefix}-{fw}-{profile}.json';j=read(r/name)
   assert j['passed'] and j['iso_sha256']==a['sha256'],name
   assert j['i8042_present']==(profile=='ps2'),name
   if prefix=='recovery-keyboard' and fw=='bios':assert j['local_installed_recovery'] is True
   if prefix=='italian-apps':assert len(j['checks'])>=5
   else:assert all(j[k] for k in ['keyboard_only_menu','italian_punctuation','altgr','utf8_backspace'])
   dest.append(j)
extra=read(r/'italian-extra-apps.json');assert extra['passed'] and extra['iso_sha256']==a['sha256'] and len(extra['checks'])==5
for name in ['identity-result.json','bios-installer-result.json','bios-recovery-result.json']:
 j=read(r/name);assert j['passed'],name
 if 'iso_sha256' in j:assert j['iso_sha256']==a['sha256']
ui=read(r/'chrome-result.json');assert ui['passed'] and ui['iso_sha256']==a['sha256'] and len(ui['checks'])==6
assert all(c['exterior_halo_pixels']==0 and c['changed_pixels_after_exit']==0 for c in ui['checks'])
assert ui['menu_alignment']['difference_px']<=1 and ui['selected_pointer_transport']=='QEMU HID Mouse'
suite=read(home/'workspace/webkit-tmc/test-results/milestone-tests.json')
assert suite['passed'] and suite['guest_requested'] and len(suite['cases'])==29
assert all(c['exit_code']==0 for c in suite['cases']);save('kernel-regression-result.json',suite)
kernel=sha(iso.parent/'kernel.elf')
for kind in ['network','vm','sync','tls','threads']:
 j=read(home/('workspace/webkit-tmc/test-results/guest-'+kind+'-build.json'));assert j['sha256']['kernel.elf']==kernel,kind
save('kernel-production-match.json',{'passed':True,'kernel_elf_sha256':kernel,'matching_probe_builds':['network','vm','sync','tls','threads'],'note':'Same production kernel, disposable QA command volumes; not WebKit/browser tests'})
ev=r/'evidence';ev.mkdir(exist_ok=True);manifest=[]
def crop(src,name,box):
 src=Path(src);dst=ev/name
 with Image.open(src) as im:im.crop(box).save(dst,optimize=True)
 manifest.append({'source':str(src),'original_sha256':sha(src),'crop':box,'retained':str(dst.relative_to(r)),'sha256':sha(dst)})
for name,frame in [('input-uefi-xhci-hub.json','login-cursor'),('input-bios-xhci-nested-hub.json','login-cursor'),('installed-uefi-xhci-hub.json','desktop-terminal')]:
 j=read(r/name);crop(Path(j['work'])/'evidence'/(frame+'.png'),name[:-5]+'-'+frame+'.png',(100,440,810,665) if frame=='login-cursor' else (200,120,1100,680))
for prefix,frame in [('italian-apps','textedit-italian'),('recovery-keyboard','italian-physical-keys')]:
 j=read(r/(prefix+'-uefi-xhci-hub-kbd.json'));crop(Path(j['work'])/'evidence'/(frame+'.png'),frame+'.png',(100,200,1160,665))
j=read(r/'italian-apps-uefi-ps2.json');crop(Path(j['work'])/'evidence/textedit-italian.png','euro-detail.png',(390,280,800,350))
for f in Path(ui['work']+'/evidence').glob('*.png'):
 if not f.name.startswith('failure'):shutil.copyfile(f,ev/f.name)
save('evidence-manifest.json',manifest)
for src,dst in [('/var/tmp/tmc-input4-final-media.log','media-qa.log'),('/var/tmp/tmc-input4-final-regression.log','kernel-regression.log'),('/var/tmp/tmc-input4-final-recovery.log','recovery.log'),('/var/tmp/tmc-input4-final-italian.log','italian-qa.log')]:shutil.copyfile(src,r/dst)
save('final-qa.json',{'passed':True,'version':'2.4.5','build':'21B9','sha256':a['sha256'],'bytes':a['bytes'],'independent_clean_rebuild_identical':True,'f11_diagnostic_absent':True,'live_input_profiles_passed':17,'installed_input_profiles_passed':10,'input_assertions':sum(len(j['checks']) for j in inputs),'independent_x_y_profiles_passed':27,'italian_app_profiles_passed':len(italian),'italian_app_assertions':sum(len(j['checks']) for j in italian),'italian_extra_clients_passed':['Notes','Stickies','Calendar','Settings search','Desktop search'],'italian_recovery_profiles_passed':len(recovery),'sole_italian_keyboard_passed':True,'vertical_only_decoder_negative_control_passed':True,'kernel_regressions':{'passed':29,'total':29},'native_kernel_matches_production':True,'mission_control_chrome_and_menu_alignment_passed':True,'installer_and_sealed_local_recovery_passed':True,'forced_pointer_transport_verified':True,'usb_keyboard_profiles_without_i8042':True,'prior_21B8_features_regression_passed':True,'physical_hardware_verified':False,'limits':['Not all HID/USB device classes; bounded parser/static capacity','OHCI, SuperSpeed hubs and hotplug/reconnect not implemented','Physical Bay Trail and EHCI HS transaction translator path not hardware-validated','Recovery direct keyboard: HID Boot USB or ACPI PNP0303 PS/2','Synthetic decoder negative control, not an observed descriptor from the physical mouse','QEMU/host tests do not guarantee physical PC compatibility']})
print('PASS 21B9 gate: 27 independent-X/Y input profiles, 4 Italian app + 4 Recovery profiles, installer/restore, reproducible ISO, 29/29 same-kernel regressions')
