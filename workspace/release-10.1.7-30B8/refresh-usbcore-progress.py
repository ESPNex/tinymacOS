#!/usr/bin/env python3
"""Verify the experimental canonical kernel increment without rebinding v19.
Never publishes, approves a release, or treats controller MODEL as hardware.
"""
from pathlib import Path
import hashlib,json
q=Path(__file__).resolve().parent
root=q.parents[1]/'tinymacOS'
def sha(p):return hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
def load(n):return json.loads((q/n).read_text())
r=load('usbcore-kernel-integration-result.json')
manifest=load(r['source_manifest'])
manifest_intact=sha(q/r['source_manifest'])==r['source_manifest_sha256']
def documentation(n):
 return n=='README.md' or (n.startswith('docs/') and Path(n).suffix in ('.md','.txt')) or n=='compat/linux/README.md'
changed=[n for n,h in manifest.items() if not (root/n).is_file() or sha(root/n)!=h]
added=[str(p.relative_to(root)) for p in root.rglob('*') if p.is_file() and '__pycache__' not in p.parts and str(p.relative_to(root)) not in manifest]
source_current=manifest_intact and not changed and not added
runtime_sources_current=manifest_intact and not any(not documentation(n) for n in changed+added)
documentation_delta=[n for n in changed+added if documentation(n)]
stage=Path(r['stage'])
artifacts_current=all((stage/n).is_file() and sha(stage/n)==a['sha256'] for n,a in r['artifacts'].items())
iso_hash=r['artifacts']['tinymacOS-10.1.7-x86_64.iso']['sha256']
guests_current=True
for row in r['guest_receipts']:
 p=q/row['file'];v=load(row['file'])
 guests_current &= sha(p)==row['sha256'] and v['passed'] and v['actual_iso_kernel_equals_build'] and v['iso_sha256']==iso_hash
canonical=load('usbcore-canonical-host-result.json')
host_current=canonical['passed'] and all((root/n).is_file() and sha(root/n)==h for n,h in canonical['source_files'].items())
model=load('usbcore-enumeration-model-result.json')
model_current=model['passed'] and all((root/n).is_file() and sha(root/n)==h for n,h in model['sources'].items())
verified=bool(r['passed'] and runtime_sources_current and artifacts_current and guests_current)
status=load('STATUS.json')
status['canonical_usb_abi_development']={'source_bound_host_contracts_passed':bool(host_current),'receipt':'usbcore-canonical-host-result.json','optimizations':canonical['optimizations'],'original_bus_binding_authorization_dynamic_ids_tested':bool(host_current),'kernel_linked':verified,'full_core_runtime_tested':verified and model_current,'hub_enumeration_tested':verified,'hub_hotplug_tested':verified,'usbhid_tested':False,'release_gate':False,'kernel_receipt':'usbcore-kernel-integration-result.json','explicit_controller_model_receipt':'usbcore-enumeration-model-result.json','explicit_controller_model_sanitized_passed':bool(model_current),'same_dualboot_artifact_guests_passed':6 if verified else 0,'boot_class_drivers_only':True,'old_native_class_path_inactive_in_experiment':verified,'kernel_build_is_opt_in':'TMC_USB_CANONICAL=1','source_snapshot_current':bool(source_current),'runtime_sources_match_frozen_snapshot':bool(runtime_sources_current),'documentation_delta_from_tested_snapshot':documentation_delta,'artifacts_match_receipt':bool(artifacts_current),'limitations':r['limitations']}
status['state']='UNRELEASED / canonical USB core + controller + hub kernel integration experiment'
status['experimental_canonical_increment']={'receipt':'usbcore-kernel-integration-result.json','verified':verified,'iso_sha256':iso_hash,'build':str(stage/'build'),'guest_matrix':['BIOS/UEFI xHCI two mice + keyboard','BIOS/UEFI EHCI mouse + keyboard','BIOS/UEFI xHCI external hub + mouse + keyboard'],'scope':'Original USB core/HCD/hub + original xHCI/EHCI + original Boot classes; NOT full usbhid'}
status['requested_usb_macroblock_complete']=False
status['published']=False
status['full_kernel_input_install_final_gate_passed']=False
status['blocking_work']=['Finish whole canonical HID core/usbhid/hid-input path; current experimental kernel uses original Boot mouse/keyboard drivers.','Complete scheduler/workqueue context, controller-init and endpoint rollback, integrated reset/clear-halt and genuine entropy-ingestion boundary before enabling this path by default.','Run final installation, separate Recovery firmware input, kernel, UI/layout/deletion and license/matching-source gates on the fully integrated artifact before release.']
(q/'STATUS.json').write_text(json.dumps(status,indent=2)+'\n')
print(json.dumps({'canonical_experiment_verified':verified,'source_snapshot_current':source_current,'runtime_sources_current':runtime_sources_current,'documentation_delta':documentation_delta,'artifacts_current':artifacts_current,'same_artifact_guests':6 if verified else 0,'host_contracts_current':bool(host_current),'model_sanitized_current':bool(model_current),'whole_usbhid':False,'release_approved':False},indent=2))
assert verified and host_current and model_current
