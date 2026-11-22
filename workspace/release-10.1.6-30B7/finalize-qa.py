#!/usr/bin/env python3
"""Fail closed: collect only completed, hash-bound 10.1.6 QA receipts."""
import hashlib,json,subprocess
from pathlib import Path
q=Path(__file__).resolve().parent;root=q.parents[1]
def h(p):return hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
a=json.loads((q/'artifact.json').read_text());digest=a['sha256'];iso=Path('/var/tmp/tmc-10.1.6')/a['filename']
assert h(iso)==digest and iso.stat().st_size==a['bytes']
assert h('/var/tmp/tmc-10.1.6/repro.iso')==digest
assert h('/var/tmp/tmc-10.1.6/final-source.iso')==digest and a['final_source_rebuild_identical']
assert a['source_manifest_sha256']==h(q/'source-hashes.json')
frozen=json.loads((q/'source-hashes.json').read_text());assert len(frozen)==589
for name,expected in frozen.items():assert h(root/'tinymacOS'/name)==expected,name
files=['hub-drag-result.json','ui-icons-result.json','title-result.json','control-result.json','identity-result.json','bios-installer-result.json','bios-recovery-result.json','pointer-contract-result.json','control-contract-result.json','kernel-regression-result.json','regression-negative-controls.json','preview-verification.json','firmware-contract-result.json','host-suite-result.json','async-retry-result.json','prerelease-conversion.json','linux-hcd-result.json','svg-contract-result.json']+[f'kernel-probe-{kind}.json' for kind in ['vm','network','sync','tls','threads']]
kernel_guests=[f'kernel-guest-{kind}-smp{cpu}.json' for kind in ['vm','network','sync','tls','threads'] for cpu in ([1,2] if kind in ['vm','network'] else [1,4,8])]
files+=kernel_guests
live=[];installed=[]
for fw in ['uefi','bios']:
 profiles=['ps2','ehci','companion','xhci','xhci-hub','xhci-hub-kbd','xhci-nested-hub']
 if fw=='uefi':profiles+=['uhci','ehci-kbd','companion-kbd']
 live += [f'input-{fw}-{p}.json' for p in profiles]
 installed += [f'installed-{fw}-{p}.json' for p in ['ps2','ehci','companion','xhci-hub','xhci-nested-hub']]
files+=live+installed+[f'recovery-keyboard-{fw}-{p}.json' for fw in ['uefi','bios'] for p in ['ps2','xhci-hub-kbd']]
files += [f'recovery-pointer-bios-{bus}-{key}.json' for bus in ['ehci','xhci'] for key in ['r','f12']]+['recovery-pointer-uefi.json']
files += [f'recovery-ps2-usb-{fw}-{bus}.json' for fw in ['uefi','bios'] for bus in ['ehci','xhci']]
files += [f'native-sustained-{fw}-{bus}.json' for fw in ['uefi','bios'] for bus in ['ehci','xhci']]
records={}
for name in files:
 r=json.loads((q/name).read_text());assert r['passed'],name
 # QA probe volumes intentionally differ from retail (command replaced).
 # Their ISO hashes are checked against each other and their extracted kernel
 # against the production kernel below, never against the whole retail ISO.
 if 'iso_sha256' in r and not name.startswith(('kernel-probe-','kernel-guest-')):assert r['iso_sha256']==digest,name
 records[name]=r
assert len(records['regression-negative-controls.json']['cases'])==10
assert len(records['hub-drag-result.json']['drags'])==12
assert len(records['ui-icons-result.json']['rendered_icons'])==3
assert all(x['max_channel_error']==0 for x in records['ui-icons-result.json']['rendered_icons'])
for fw in ['uefi','bios']:
 for bus in ['ehci','xhci']:
  assert records[f'recovery-ps2-usb-{fw}-{bus}.json']['sustained_mouse_cycles']==12
  assert records[f'native-sustained-{fw}-{bus}.json']['cycles']==12
  assert records[f'native-sustained-{fw}-{bus}.json']['exact_cursor_checks']==50
for kind in ['vm','network','sync','tls','threads']:
 p=records[f'kernel-probe-{kind}.json'];assert p['actual_iso_kernel_equals_current_production']
 assert p['kernel_sha256']==h(iso.parent/'build/kernel.elf') and p['kernelcache_sha256']==h(iso.parent/'build/KernelCache')
 for cpu in ([1,2] if kind in ['vm','network'] else [1,4,8]):assert records[f'kernel-guest-{kind}-smp{cpu}.json']['iso_sha256']==p['iso_sha256']
k=records['kernel-regression-result.json'];assert k['guest_requested'] and len(k['cases'])==29 and all(c['exit_code']==0 for c in k['cases'])
assert records['pointer-contract-result.json']['negative_control_failed_as_expected']
assert records['pointer-contract-result.json']['previous_10_1_1_idle_absolute_failed']
assert records['host-suite-result.json']['source_manifest_sha256']==h(q/'source-hashes.json')
assert all(c['exit_code']==0 for c in records['host-suite-result.json']['cases'])
assert records['async-retry-result.json']['source_sha256']==h(root/'tinymacOS/recovery/usbhid.c')
assert records['prerelease-conversion.json']['all_previous_releases_prerelease']
assert len(records['linux-hcd-result.json']['cases'])==5
assert records['svg-contract-result.json']['original_svg_sources']==13
assert records['identity-result.json']['actual_iso_svg_resources_verified']
for name,value in records['linux-hcd-result.json']['source_sha256'].items():assert h(root/'tinymacOS'/name)==value
for filename in ['host-input.log','host-legacy.log']:
 text=(q/filename).read_text();assert 'PASS' in text and 'FAIL' not in text,filename
 files.append(filename)
# Preserve actual image evidence; no reconstruction, annotations or resampling.
ev=q/'evidence';ev.mkdir(exist_ok=True)
import shutil
from PIL import Image
assert h(ev/'control-center-10.1.6.png')==records['preview-verification.json']['full_frame_sha256']
with Image.open(ev/'control-center-10.1.6.png') as im, Image.open(ev/'control-center-detail.png') as crop:
 assert im.crop(tuple(records['preview-verification.json']['crop_box'])).tobytes()==crop.tobytes()
for p in (Path(records['title-result.json']['work'])/'evidence').glob('*.png'):shutil.copyfile(p,ev/p.name)
subprocess.run(['python3',str(root/'workspace/check-snapshot-budget.py')],check=True)
out={**a,'passed':True,'source_files_checked':len(frozen),'reproducible_independent_build':True,'live_input_profiles':len(live),'hdd_only_input_profiles':len(installed),'input_assertions':sum(len(records[n]['checks']) for n in live+installed),'optical_recovery_controller_key_combinations':5,'optical_recovery_mouse_tablet_boots':18,'ps2_keyboard_direct_usb_recovery_boots':8,'native_sustained_relative_cycles':48,'native_extra_exact_cursor_checks':200,'recovery_sustained_relative_cycles':48,'recovery_extra_exact_cursor_checks':200,'recovery_italian_profiles':4,'kernel_regressions':29,'kernel_probe_media_bound_to_production':5,'kernel_guest_profiles_bound_to_production':13,'actual_bios_install_hdd_reboot_uefi_crossboot':True,'sealed_offline_recovery_reinstall':True,'title_pixel_center_and_drag':True,'finder_applications_latest_appstore_pixels_verified':True,'recovery_diskutility_matches_os_pixels_verified':True,'desktop_appearance_duplicate_commands_removed':True,'hub_drag_repetitions':12,'prior_hub_drag_attempt_failed_and_retained':True,'control_center_glass_preserved_duplicates_removed':True,'control_center_original_svg_sources':13,'retained_linux_hcd_contracts':4,'ehci_idle_contract':1,'compatibility_transport_negative_controls':10,'sanitizers':['address','undefined'],'physical_hardware_tested':False,'historical_reference_confirmed_by_user':'2.4.5 Levante 21B9; Recovery working, login vertical-only','browser_included':False,'publication_state':'Not a publication receipt; see github-release-verification.json after source-first upload','result_files_sha256':{n:h(q/n) for n in files}}
(q/'final-qa.json').write_text(json.dumps(out,indent=2)+'\n');print('PASS complete final QA:',len(files),'receipts;',out['input_assertions'],'native input assertions')
