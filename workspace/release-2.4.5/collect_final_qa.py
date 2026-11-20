#!/usr/bin/env python3
"""Collect actual final-candidate receipts. Fail closed; never marks a failed check PASS."""
import hashlib,json,shutil
from pathlib import Path
from PIL import Image
home=Path(__file__).resolve().parents[2];r=Path(__file__).resolve().parent
iso=Path('/var/tmp/tmc245-final/tinymacOS-2.4.5-OpenCore-Installer-x86_64.iso')
a=json.loads((r/'artifact.json').read_text());digest=hashlib.file_digest(iso.open('rb'),'sha256').hexdigest()
assert digest==a['sha256'] and iso.stat().st_size==a['bytes']
repro=Path('/var/tmp/tmc245-final-repro')/iso.name
assert hashlib.file_digest(repro.open('rb'),'sha256').hexdigest()==digest
for fw,count in [('uefi',12),('bios',10)]:
 p=Path('/var/tmp/tmc245-final-pointer-'+fw+'/evidence/pointer.json');j=json.loads(p.read_text())
 assert j['passed'] and len(j['checks'])==count and j['iso_sha256']==digest
 j['external_evidence']=str(p.parent)
 (r/('pointer-'+fw+'-result.json')).write_text(json.dumps(j,indent=2,ensure_ascii=False)+'\n')
for name in ['identity','bios-installer','installed-pointer','bios-recovery']:
 j=json.loads((r/(name+'-result.json')).read_text());assert j['passed'],name
 if name!='bios-recovery':assert j['iso_sha256']==digest,name
suite=json.loads((home/'workspace/webkit-tmc/test-results/milestone-tests.json').read_text())
assert suite['passed'] and suite['guest_requested'] and len(suite['cases'])==29
assert all(c['exit_code']==0 for c in suite['cases'])
(r/'kernel-regression-result.json').write_text(json.dumps(suite,indent=2)+'\n')
kernel_hash=hashlib.sha256((iso.parent/'kernel.elf').read_bytes()).hexdigest()
for kind in ['network','vm','sync','tls','threads']:
    build=json.loads((home/('workspace/webkit-tmc/test-results/guest-'+kind+'-build.json')).read_text())
    assert build['sha256']['kernel.elf']==kernel_hash,kind+' kernel mismatch'
(r/'kernel-production-match.json').write_text(json.dumps({'passed':True,'production_kernel_elf_sha256':kernel_hash,'matching_guest_probe_builds':['network','vm','sync','tls','threads'],'note':'Same kernel executable; separate diagnostic command volumes, not production-ISO browser tests'},indent=2)+'\n')
# The suite builds separate QA-only guest images. Their hashes are retained in
# WebKit test-results; the production ISO above is never replaced by a probe.
for source,dest in [('/var/tmp/tmc245-final-regression.log','kernel-regression.log'),('/var/tmp/tmc245-final-recovery.log','recovery.log'),('/var/tmp/tmc245-final-installed-pointer.log','installed-pointer.log')]:
 shutil.copyfile(source,r/dest)
ev=r/'evidence';ev.mkdir(exist_ok=True);manifest=[]
def retain(src,name,box=None):
 src=Path(src);dst=ev/name
 with Image.open(src) as im:
  if box:im=im.crop(box)
  im.save(dst,optimize=True)
 manifest.append({'source':str(src),'original_sha256':hashlib.sha256(src.read_bytes()).hexdigest(),'retained':str(dst.relative_to(r)),'crop':box,'sha256':hashlib.sha256(dst.read_bytes()).hexdigest()})
for fw in ['uefi','bios']:
 retain('/var/tmp/tmc245-final-pointer-'+fw+'/evidence/mouse-native-diagnostics.png',fw+'-live-input-diagnostics.png',(12,16,510,365))
j=json.loads((r/'installed-pointer-result.json').read_text());p=Path(j['external_evidence'])
for fw in ['bios','uefi']:
 retain(p/(fw+'-login-diagnostics.png'),fw+'-installed-input-diagnostics.png',(12,16,510,365))
retain(p/'bios-locked-cursor.png','bios-installed-locked-cursor.png')
j=json.loads((r/'bios-installer-result.json').read_text());p=Path(j['external_work'])/'evidence'
retain(p/'bios-install-complete.png','bios-install-complete.png')
(r/'evidence-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
summary={'passed':True,'version':'2.4.5','build':'21B6','production_iso_sha256':digest,'independent_rebuild_identical':True,'host_input_contracts':'ASan/UBSan queue, completions, recovery and actual enumeration with emulated MMIO','live_pointer_checks':{'uefi':12,'bios':10},'installed_login_mouse_bios_uefi':True,'installer_bios_then_uefi_crossboot':True,'sealed_local_recovery_documents_preserved':True,'kernel_runtime_regressions':{'passed':29,'total':29,'production_iso':False},'physical_hardware_verified':False,'excluded_not_passed':['BIOS optical R-key Recovery selection: automatic attempts reached native setup instead; installed local Recovery separately passed']}
(r/'final-qa.json').write_text(json.dumps(summary,indent=2)+'\n')
print('PASS final ISO QA gate: input, identity, install/login both firmwares, local restore, 29/29 kernel/runtime, identical rebuild')
