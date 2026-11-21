#!/usr/bin/env python3
"""Fail closed: collect only completed, hash-bound 10.1.1 QA receipts."""
import hashlib,json,subprocess
from pathlib import Path
q=Path(__file__).resolve().parent;root=q.parents[1]
def h(p):return hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
a=json.loads((q/'artifact.json').read_text());digest=a['sha256'];iso=Path('/var/tmp/tmc-10.1.1')/a['filename']
assert h(iso)==digest and iso.stat().st_size==a['bytes']
assert h('/var/tmp/tmc-10.1.1/repro.iso')==digest
frozen=json.loads((q/'source-hashes.json').read_text());assert len(frozen)==575
for name,expected in frozen.items():assert h(root/'tinymacOS'/name)==expected,name
files=['title-result.json','control-result.json','identity-result.json','bios-installer-result.json','bios-recovery-result.json','pointer-contract-result.json','control-contract-result.json','kernel-regression-result.json']
live=[];installed=[]
for fw in ['uefi','bios']:
 profiles=['ps2','ehci','companion','xhci','xhci-hub','xhci-hub-kbd','xhci-nested-hub']
 if fw=='uefi':profiles+=['uhci','ehci-kbd','companion-kbd']
 live += [f'input-{fw}-{p}.json' for p in profiles]
 installed += [f'installed-{fw}-{p}.json' for p in ['ps2','ehci','companion','xhci-hub','xhci-nested-hub']]
files+=live+installed+[f'recovery-keyboard-{fw}-{p}.json' for fw in ['uefi','bios'] for p in ['ps2','xhci-hub-kbd']]
files += [f'recovery-pointer-bios-{bus}-{key}.json' for bus in ['ehci','xhci'] for key in ['r','f12']]+['recovery-pointer-uefi.json']
records={}
for name in files:
 r=json.loads((q/name).read_text());assert r['passed'],name
 if 'iso_sha256' in r:assert r['iso_sha256']==digest,name
 records[name]=r
k=records['kernel-regression-result.json'];assert k['guest_requested'] and len(k['cases'])==29 and all(c['exit_code']==0 for c in k['cases'])
assert records['pointer-contract-result.json']['negative_control_failed_as_expected']
for filename in ['host-input.log','host-legacy.log']:
 text=(q/filename).read_text();assert 'PASS' in text and 'FAIL' not in text,filename
 files.append(filename)
# Preserve actual image evidence; no reconstruction, annotations or resampling.
ev=q/'evidence';ev.mkdir(exist_ok=True)
import shutil
for p in (Path(records['title-result.json']['work'])/'evidence').glob('*.png'):shutil.copyfile(p,ev/p.name)
subprocess.run(['python3',str(root/'workspace/check-snapshot-budget.py')],check=True)
out={**a,'passed':True,'source_files_checked':len(frozen),'reproducible_independent_build':True,'live_input_profiles':len(live),'hdd_only_input_profiles':len(installed),'input_assertions':sum(len(records[n]['checks']) for n in live+installed),'optical_recovery_controller_key_combinations':5,'optical_recovery_mouse_tablet_boots':10,'recovery_italian_profiles':4,'kernel_regressions':29,'actual_bios_install_hdd_reboot_uefi_crossboot':True,'sealed_offline_recovery_reinstall':True,'title_pixel_center_and_drag':True,'control_center_10_1_preserved':True,'sanitizers':['address','undefined'],'physical_hardware_tested':False,'historical_mouse_fix_exact_version_identified':False,'browser_included':False,'publication_state':'Not a publication receipt; see github-release-verification.json after source-first upload','result_files_sha256':{n:h(q/n) for n in files}}
(q/'final-qa.json').write_text(json.dumps(out,indent=2)+'\n');print('PASS complete final QA:',len(files),'receipts;',out['input_assertions'],'native input assertions')
