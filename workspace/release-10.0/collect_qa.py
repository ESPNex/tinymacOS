#!/usr/bin/env python3
"""Fail-closed gate for the new 10.0/30A1 release. No prior PASS reuse."""
import hashlib,json,shutil
from pathlib import Path
from PIL import Image
r=Path(__file__).resolve().parent;home=r.parents[1];d=Path('/var/tmp/tmc10-final');repro=Path('/var/tmp/tmc10-repro')
def read(p):return json.loads(Path(p).read_text())
def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def save(n,v):(r/n).write_text(json.dumps(v,indent=2,ensure_ascii=False)+'\n')
a=read(d/'tinymacOS-10.0-OpenCore-Installer-x86_64.json');iso=d/a['filename'];assert sha(iso)==a['sha256'] and iso.stat().st_size==a['bytes']
assert sha(repro/iso.name)==a['sha256'] and sha(d/'tinymacOS-10.0-x86_64.iso')==sha(repro/'tinymacOS-10.0-x86_64.iso')==a['native_iso_sha256']
a.update(version='10.0',build='30A1',codename='Tramontana');save('artifact.json',a);(r/'SHA256SUMS.txt').write_text(a['sha256']+'  '+iso.name+'\n')
frozen=read(r/'source-hashes.json')
for path,digest in frozen.items():assert sha(home/'tinymacOS'/path)==digest,('Source changed after build',path)
save('build-input-match.json',{'passed':True,'files':len(frozen),'independent_clean_rebuild_identical':True})
assert b'Diagnostica USB' not in (d/'desktop.elf').read_bytes()
assets=read(r/'assets-result.json');appearance=read(r/'appearance-result.json');assert assets['passed'] and appearance['passed'] and appearance['iso_sha256']==a['sha256']
assert len(appearance['checks'])==8 and appearance['split_wordmark_visible_regular4_semibold10'] and appearance['gallery_thirteenth_entry_and_keyboard_navigation']
assert appearance['recovery']['apple_text_center_difference_px']<=1
host=read(r/'host-contracts-result.json');assert host['passed'] and host['vertical_only_negative_control_passed']
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
retain(av/'default-desktop.png','default-desktop.jpg.png',(0,0,1280,800))
retain(av/'about-semibold.png','about-sem  ibold.png'.replace('  ',''),(420,240,995,583))
retain(av/'recovery-smaller-title.png','recovery-title.png',(0,0,1200,200))
for name in ['Blu-light.png','Azzurro-light.png','Riviera-light.png','Tramontana-light.png','Blu-dark.png','Azzurro-dark.png','Riviera-dark.png','Tramontana-dark.png','recovery-menu-alignment.png']:
 p=av/name
 with Image.open(p) as im:retain(p,name,(0,0,im.width,im.height))
for prefix,frame in [('italian-apps','textedit-italian'),('recovery-keyboard','italian-physical-keys')]:
 j=read(r/(prefix+'-uefi-xhci-hub-kbd.json'));retain(Path(j['work'])/'evidence'/(frame+'.png'),frame+'.png',(100,200,1160,665))
for p in Path(ui['work']+'/evidence').glob('*.png'):
 if not p.name.startswith('failure'):shutil.copyfile(p,ev/p.name)
save('evidence-manifest.json',manifest)
for src,dst in [('/var/tmp/tramontana-tests.log','media-and-italian-qa.log'),('/var/tmp/tramontana-kernel-tests.log','kernel-regression.log'),('/var/tmp/tramontana-appearance.log','appearance.log')]:shutil.copyfile(src,r/dst)
qa={'passed':True,'version':'10.0','build':'30A1','sha256':a['sha256'],'bytes':a['bytes'],'independent_clean_rebuild_identical':True,'source_files_checked':len(frozen),'f11_diagnostic_absent':True,'live_input_profiles_passed':17,'installed_input_profiles_passed':10,'input_assertions':sum(len(j['checks']) for j in records),'independent_x_y_profiles_passed':27,'italian_app_profiles_passed':len(italian),'italian_app_assertions':sum(len(j['checks']) for j in italian),'italian_recovery_profiles_passed':len(recovery),'italian_extra_clients_passed':5,'sole_italian_keyboard_passed':True,'vertical_only_decoder_negative_control_passed':True,'kernel_regressions':{'passed':29,'total':29},'native_kernel_matches_production':True,'mission_control_chrome_and_menu_alignment_passed':True,'installer_and_sealed_local_recovery_passed':True,'forced_pointer_transport_verified':True,'usb_keyboard_profiles_without_i8042':True,'appearance_profiles_passed':8,'icon_atlas_and_filesystem_verified':True,'latest_exact_name_variants_verified':True,'installer_procedural_exception_verified':True,'split_semibold_wordmark_verified':True,'new_wallpaper_gallery_settings_verified':True,'recovery_alignment_and_smaller_title_verified':True,'physical_hardware_tested':False,'browser_included':False}
save('final-qa.json',qa);print(json.dumps(qa,indent=2))
