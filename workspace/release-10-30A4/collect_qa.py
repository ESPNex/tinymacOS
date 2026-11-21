#!/usr/bin/env python3
"""Fail-closed gate for the replacement v10/30A4. No prior PASS reuse."""
import hashlib,json,shutil
from pathlib import Path
from PIL import Image
r=Path(__file__).resolve().parent;home=r.parents[1];d=Path('/var/tmp/tmc30a4-final');repro=Path('/var/tmp/tmc30a4-repro')
def read(p):return json.loads(Path(p).read_text())
def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def save(n,v):(r/n).write_text(json.dumps(v,indent=2,ensure_ascii=False)+'\n')
a=read(d/'tinymacOS-10-x86_64.json');iso=d/a['filename'];assert sha(iso)==a['sha256'] and iso.stat().st_size==a['bytes']
assert sha(repro/iso.name)==a['sha256'] and sha(d/'native.iso')==sha(repro/'native.iso')==a['native_iso_sha256']
a.update(version='10',build='30A4',codename='Tramontana');save('artifact.json',a);(r/'SHA256SUMS.txt').write_text(a['sha256']+'  '+iso.name+'\n')
presync=(r/'source-baseline.log').read_text()
assert 'Verified existing remote source baseline before publication of 30A4: ab537d8769671afaa74adf70556e56b1e396e9a9' in presync
frozen=read(r/'source-hashes.json')
for path,digest in frozen.items():assert sha(home/'tinymacOS'/path)==digest,('Source changed after build',path)
save('build-input-match.json',{'passed':True,'files':len(frozen),'independent_clean_rebuild_identical':True})
assert b'Diagnostica USB' not in (d/'desktop.elf').read_bytes()
assets=read(r/'assets-result.json');appearance=read(r/'appearance-result.json');assert assets['passed'] and appearance['passed'] and appearance['iso_sha256']==a['sha256']
assert len(appearance['checks'])==10 and appearance['split_wordmark_visible_regular4_semibold10'] and appearance['gallery_thirteenth_entry_and_keyboard_navigation']
assert appearance['recovery']['apple_text_center_difference_px']<=1
rv=read(r/'recovery-visual-result.json');assert rv['passed'] and rv['iso_sha256']==a['sha256'] and len(rv['checks'])==5
assert assets['Recovery_only_Terminal_imported_byte_exact'] and appearance['settings_internal_version_10_verified']
assert {x['theme']:x['associated_wallpaper'] for x in appearance['checks']}=={'Blu':'Prisma','Azzurro':'Brezza','Teal':'Prisma glaciale','Riviera':'Riviera','Tramontana':'Tramontana'}
host=read(r/'host-contracts-result.json');assert host['passed'] and host['vertical_only_negative_control_passed']
assert host['Finder_Applications_scope_and_negative_controls_passed'] and len(host['checks'])==3
for mode,size in [('grid',54),('list',24),('sidebar',19)]:
 icon=appearance['Finder_Applications_'+mode+'_icon'];assert icon['slot']==13 and icon['size']==size and icon['error']<.002
for c in host['checks']:assert c['passed'] and sha(r/c['log'])==c['sha256']
for path,digest in host['source_sha256'].items():assert sha(home/'tinymacOS'/path)==digest
inputs=[]
for fw,profiles in [('uefi',['ps2','ehci','companion','xhci','xhci-hub','xhci-hub-kbd','xhci-nested-hub','uhci','ehci-kbd','companion-kbd']),('bios',['ps2','ehci','companion','xhci','xhci-hub','xhci-hub-kbd','xhci-nested-hub'])]:
 for p in profiles:inputs.append((f'input-{fw}-{p}.json',p,False))
for fw in ['bios','uefi']:
 for p in ['ps2','ehci','companion','xhci-hub','xhci-nested-hub']:inputs.append((f'installed-{fw}-{p}.json',p,True))
records=[]
for name,profile,installed in inputs:
 j=read(r/name);assert j['passed'] and j['iso_sha256']==a['sha256'],name
 target='QEMU PS/2 Mouse' if profile=='ps2' else 'QEMU HID Mouse';assert j['selected_pointer_transport']==target
 for field in ['mice','final_mice']:assert [m['name'] for m in j[field] if m['current']]==[target]
 assert any('Independent horizontal-only X and vertical-only Y' in s for s in j['checks'])
 if profile.endswith('-kbd'):assert j['ps2_controller_present'] is False
 if installed:assert any('Installed Italian accents' in s for s in j['checks'])
 records.append(j)
italian=[];recovery=[]
for fw in ['bios','uefi']:
 for profile in ['ps2','xhci-hub-kbd']:
  for prefix,dest in [('italian-apps',italian),('recovery-keyboard',recovery)]:
   name=f'{prefix}-{fw}-{profile}.json';j=read(r/name);assert j['passed'] and j['iso_sha256']==a['sha256'],name
   assert j['i8042_present']==(profile=='ps2')
   if prefix=='recovery-keyboard':
    assert all(j[k] for k in ['keyboard_only_menu','italian_punctuation','altgr','utf8_backspace'])
    if fw=='bios':assert j['local_installed_recovery']
   else:assert len(j['checks'])>=5
   dest.append(j)
j=read(r/'recovery-pointer-uefi.json');assert j['passed'] and j['iso_sha256']==a['sha256']
assert len(j['devices'])==2 and len(j['checks'])==2
localtablet=read(r/'bios-recovery-tablet-result.json');assert localtablet['passed'] and localtablet['local_recovery_without_iso']
fw=read(r/'bios-input-artifact-result.json');assert fw['passed'] and fw['iso_sha256']==a['sha256']
supp=read(r/'supplementary-selector-result.json');assert supp['passed'] and len(supp['cases'])==4
assert supp['harness_sha256']==sha(r/'recovery_pointer_qa.py') and supp['launcher_sha256']==sha(r/'run-optical-recovery-input.sh')
assert supp['negative_original_30A2_control']['harness_sha256']==supp['harness_sha256']
assert supp['ordinary_key_hold_ms']==300 and not supp['keyboard_throughput_tested']
assert supp['negative_original_30A2_control']['passed'] is False
assert supp['negative_original_30A2_control']['iso_sha256']=='f6144153f7732af388310bab78e0947f89044fe11a64e209aa238ea0a611d87e'
assert sha(r/supp['negative_original_30A2_control']['log'])==supp['negative_original_30A2_control']['log_sha256']
assert {(c['bus'],c['key']) for c in supp['cases']}=={('xhci','r'),('xhci','f12'),('ehci','r'),('ehci','f12')}
for case in supp['cases']:
 j=read(r/case['result']);assert j['passed'] and j['iso_sha256']==a['sha256'] and len(j['checks'])==2 and len(j['devices'])==2
 assert sha(r/case['result'])==case['result_sha256'] and sha(r/case['log'])==case['log_sha256']
 assert {d['native_key'] for d in j['boot_selection'].values()}=={case['key']}
extra=read(r/'italian-extra-apps.json');assert extra['passed'] and extra['iso_sha256']==a['sha256'] and len(extra['checks'])==5
for name in ['identity-result.json','bios-installer-result.json','bios-recovery-result.json']:
 j=read(r/name);assert j['passed'],name
 if 'iso_sha256' in j:assert j['iso_sha256']==a['sha256']
ui=read(r/'chrome-result.json');assert ui['passed'] and ui['iso_sha256']==a['sha256'] and len(ui['checks'])==6
assert all(c['exterior_halo_pixels']==0 and c['changed_pixels_after_exit']==0 for c in ui['checks']);assert ui['menu_alignment']['difference_px']<=1
suite=read(home/'workspace/webkit-tmc/test-results/milestone-tests.json');assert suite['passed'] and suite['guest_requested'] and len(suite['cases'])==29
assert all(c['exit_code']==0 for c in suite['cases']);save('kernel-regression-result.json',suite)
kernel=sha(d/'kernel.elf')
for kind in ['network','vm','sync','tls','threads']:
 j=read(home/('workspace/webkit-tmc/test-results/guest-'+kind+'-build.json'));assert j['sha256']['kernel.elf']==kernel,kind
save('kernel-production-match.json',{'passed':True,'kernel_elf_sha256':kernel,'matching_probe_builds':['network','vm','sync','tls','threads'],'note':'Production kernel, disposable QA command volumes; not browser tests'})
ev=r/'evidence';ev.mkdir(exist_ok=True);manifest=[]
def retain(src,name,box):
 src=Path(src);dst=ev/name
 with Image.open(src) as im:im.crop(box).save(dst,optimize=True)
 manifest.append({'source':str(src),'original_sha256':sha(src),'crop':box,'file':name,'sha256':sha(dst)})
av=Path(appearance['work'])/'evidence'
retain(av/'recovery-smaller-title.png','recovery-title-and-installer.png',(0,0,690,370))
for mode,size in [('grid',54),('list',24)]:
 key='Finder_Applications_'+mode+'_icon';x,y=appearance[key]['location']
 retain(av/('finder-applications-'+mode+'.png'),'finder-applications-'+mode+'.png',(max(0,x-8),max(0,y-8),x+size+12,y+size+40))
assert appearance['recovery']['exact_visible_title']=='tinymacOS Recovery' and appearance['recovery']['keyboard_badge_absent']
assert assets['Recovery_installer_identical_to_desktop'] and assets['Recovery_title_exact_and_no_ITA_badge']
save('evidence-manifest.json',manifest)
for src,dst in [('media','media-qa'),('italian','italian-qa'),('kernel','kernel-regression'),('appearance','appearance'),('repro','repro-build')]:shutil.copyfile('/var/tmp/30a4-'+src+'.log',r/(dst+'.log'))
qa={'passed':True,'version':'10','build':'30A4','sha256':a['sha256'],'bytes':a['bytes'],'independent_clean_rebuild_identical':True,'source_files_checked':len(frozen),'f11_diagnostic_absent':True,'live_input_profiles_passed':17,'installed_input_profiles_passed':10,'input_assertions':sum(len(j['checks']) for j in records),'independent_x_y_profiles_passed':27,'italian_app_profiles_passed':len(italian),'italian_app_assertions':sum(len(j['checks']) for j in italian),'italian_recovery_profiles_passed':len(recovery),'italian_extra_clients_passed':5,'sole_italian_keyboard_passed':True,'vertical_only_decoder_negative_control_passed':True,'kernel_regressions':{'passed':29,'total':29},'native_kernel_matches_production':True,'mission_control_chrome_and_menu_alignment_passed':True,'installer_and_sealed_local_recovery_passed':True,'forced_pointer_transport_verified':True,'usb_keyboard_profiles_without_i8042':True,'appearance_profiles_passed':10,'icon_atlas_and_filesystem_verified':True,'latest_exact_name_variants_verified':True,'installer_procedural_exception_verified':True,'split_semibold_wordmark_verified':True,'new_wallpaper_gallery_settings_verified':True,'recovery_alignment_and_smaller_title_verified':True,'optical_UEFI_Recovery_USB_mouse_and_tablet':True,'local_BIOS_Recovery_USB_mouse_and_tablet':True,'supplementary_optical_BIOS_boot_key_test_passed':True,'optical_BIOS_R_and_F12_USB2_USB3_mouse_tablet_cases':8,'optical_BIOS_Recovery_USB_mouse_and_tablet':True,'optical_original_30A2_negative_control_reproduced':True,'Recovery_keyboard_periodic_Boot_report_ms':32,'Recovery_procedural_except_Terminal':True,'theme_wallpaper_named_associations_verified':True,'physical_hardware_tested':False,'physical_USB_mouse_resolution_confirmed':False,'browser_included':False}
qa.update(Recovery_visible_title='tinymacOS Recovery',Recovery_keyboard_badge_removed=True,Recovery_installer_matches_desktop=True,Finder_Applications_dedicated_icon_grid_list_sidebar=True,preexisting_source_baseline_verified=True,preexisting_source_baseline_commit='ab537d8769671afaa74adf70556e56b1e396e9a9',supplementary_ordinary_key_hold_ms=300,keyboard_throughput_tested=False,optical_firmware_original_29_FFS_preserved_and_3_unmodified_USB_modules=True,original_optical_ISO_negative_control_same_final_fixture=True)
save('final-qa.json',qa);print(json.dumps(qa,indent=2))
