#!/usr/bin/env python3
"""Current, source/ISO-bound development status. Never uploads or publishes.
Historical v17/v18 receipts are not accepted as current-kernel evidence.
"""
from pathlib import Path
import hashlib,json,os,subprocess
q=Path(__file__).resolve().parent
root=q.parents[1]/'tinymacOS'
stage=Path(os.environ.get('TMC_LINUX_STAGE','/var/tmp/tmc-10.1.7/linux-xhci-v19'))
iso=stage/'tinymacOS-10.1.7-x86_64.iso'
def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def load(name):
 p=q/name
 return json.loads(p.read_text()) if p.exists() else {}
identity=sha(iso)
# The v19 staging manifest is a FROZEN source receipt, not a writable view of
# an unrelated newer working tree. Never rebind an old ISO to new source files.
manifest=load('staging-source-hashes.json')
if not manifest:raise RuntimeError('Missing frozen stage source manifest')
working={str(p.relative_to(root)):sha(p) for p in sorted(root.rglob('*')) if p.is_file() and '__pycache__' not in p.parts}
working_delta={'changed_existing':[n for n,h in manifest.items() if n in working and working[n]!=h],
               'deleted':[n for n in manifest if n not in working],
               'added_count':sum(n not in manifest for n in working),'working_file_count':len(working)}
canonical=load('usbcore-canonical-host-result.json')
canonical_current=bool(canonical.get('passed') and canonical.get('source_files') and
 all((root/n).is_file() and sha(root/n)==h for n,h in canonical['source_files'].items()))

def source_current(record):
 if not record.get('passed') or not record.get('sources'):return False
 for name,digest in record['sources'].items():
  p=root.parent/name if name.startswith('tinymacOS/') else root/name
  if not p.is_file() or sha(p)!=digest:return False
 return True
def current(name):
 r=load(name)
 return bool(r.get('passed') and r.get('iso_sha256')==identity)
names=['config','dma-pool','ehci','xhci','generic-native','hcd-hub','hid-generic','hid-parser','message','mouse-lifecycle','uhci','urb','usbkbd','usbmouse','ehci-qtd']
host=[]
for name in ['linux-'+n+'-contract-result.json' for n in names]+['intel-usb-routing-contract-result.json']:
 record=load(name)
 host.append({'receipt':name,'passed':source_current(record),'sha256':sha(q/name) if (q/name).exists() else None,'source_hashes_match_current':source_current(record)})
guests=[]
profiles={'linux-usbmouse':['xhci','xhci-hub','xhci-nested-hub','ehci','uhci','companion'],
          'linux-usbkbd':['ehci-kbd','companion-kbd','xhci-hub-kbd'],
          'linux-generic-native':['xhci-tablet','ehci-tablet','uhci-tablet','companion-tablet','xhci-tablet-hub'],
          'linux-two-mice':['']}
for firmware in ['uefi','bios']:
 for script,options in profiles.items():
  for profile in options:
   name=script+'-'+firmware+('-'+profile if profile else '')+'.json'
   r=load(name)
   row={'receipt':name,'firmware':firmware,'profile':r.get('profile',profile),'passed':current(name) and bool(r.get('actual_iso_kernel_equals_build'))}
   if (q/name).exists():row['sha256']=sha(q/name)
   if profile.startswith('xhci') or script=='linux-two-mice':
    before=r.get('whole_original_xhci_runtime',{});after=r.get('whole_original_xhci_runtime_after',{})
    row['original_xhci_execution_observed']=bool(before.get('givebacks',0)>0 and after.get('givebacks',0)>before.get('givebacks',0) and not after.get('errors',1))
    row['passed']=row['passed'] and row['original_xhci_execution_observed']
   guests.append(row)
repro=load('v19-reproducible-build.json')
repro_current=bool(repro.get('passed') and any(a.get('path')==iso.name and a.get('sha256')==identity and a.get('byte_identical') for a in repro.get('artifacts',[])))
symbols=subprocess.check_output(['nm',str(stage/'build/kernel.debug')],text=True).splitlines()
def linked(*names):return all(any(line.split()[-1].split('.')[0]==n for line in symbols) for n in names)
controllers={'uhci':linked('uhci_irq','tmc_uhci_usb_hcd_submit_urb'),
             'ehci':linked('ehci_irq','tmc_ehci_usb_hcd_submit_urb'),
             'xhci':linked('tmc_xhci_xhci_irq','tmc_xhci_usb_hcd_submit_urb','tmc_xhci_xhci_queue_intr_tx')}
validation={'passed':all(r['passed'] for r in host+guests) and repro_current and all(controllers.values()),
 'scope':'Controller/input development increment, not USB core/hub/usbhid completion or release approval',
 'iso_sha256':identity,'iso_bytes':iso.stat().st_size,'source_manifest_sha256':sha(q/'staging-source-hashes.json'),
 'source_files':len(manifest),'source_manifest_scope':'Frozen v19 stage; not the newer canonical-ABI working tree','host_suites_expected':15,'host_receipts_expected':16,'host_receipts_passed':sum(r['passed'] for r in host),
 'host_receipts':host,'usb_guest_profiles_expected':30,'usb_guest_profiles_passed':sum(r['passed'] for r in guests),
 'usb_guest_receipts':guests,'whole_original_controllers_linked':controllers,'independent_build_byte_identical':repro_current,
 'ps2_in_acceptance_scope':False,'physical_hardware_tested':False,'full_linux_usb_core_hub_usbhid':False,'published':False}
(q/'v19-validation.json').write_text(json.dumps(validation,indent=2)+'\n')
status={'state':'UNRELEASED / USB integration development','requested_version':'10.1.7','requested_build':'30B8',
 'current_increment':'v19','current_validation':'v19-validation.json','published':False,'stable_release_unchanged':'10.1.6',
 'public_main':'edbfecf54074428f02b3e76e698d8ad5c67782ba','development_iso_sha256':identity,
 'development_iso_bytes':iso.stat().st_size,'development_build':str(stage/'build'),'source_files':len(manifest),
 'source_manifest_sha256':sha(q/'staging-source-hashes.json'),'controller_input_increment_validated':validation['passed'],
 'source_manifest_scope':'Frozen v19 stage; not the newer canonical-ABI working tree',
 'working_tree_delta_from_v19':working_delta,
 'canonical_usb_abi_development':{'source_bound_host_contracts_passed':canonical_current,
    'receipt':'usbcore-canonical-host-result.json','optimizations':canonical.get('optimizations',[]),
    'original_bus_binding_authorization_dynamic_ids_tested':bool(canonical_current and canonical.get('original_binding_host_O1_O2')),
    'kernel_linked':False,'full_core_runtime_tested':False,'hub_enumeration_tested':False,'usbhid_tested':False,'release_gate':False},
 'whole_original_controllers_linked':controllers,'current_usb_guest_profiles_passed':validation['usb_guest_profiles_passed'],
 'current_usb_guest_profiles_expected':30,'current_usb_host_suites_passed':15 if all(r['passed'] for r in host) else None,
 'current_usb_host_receipts_passed':validation['host_receipts_passed'],'independent_reproducible_build_run':repro_current,
 'ps2_required':False,'ps2_completion_excluded_by_user':True,'requested_usb_macroblock_complete':False,
 'native_class_enumeration_facade_remains':True,'complete_linux_hcd_core_file_ported':False,
 'complete_linux_usb_core_bus_model_ported':False,'complete_linux_hub_driver_ported':False,
 'complete_linux_hid_core_usbhid_ported':False,'linux_hid_generic_and_hid_input_whole_files_active':True,
 'identity_media_contract_passed':current('identity-contract-result.json'),'actual_opencore_media_contract_passed':current('media-contract-result.json'),
 'current_installer_gate_passed':current('install-current-result.json'),
 'current_kernel_runtime_gate_passed':bool(load('kernel-current-source-result.json').get('passed') and load('kernel-current-source-result.json').get('kernelcache_sha256')==sha(stage/'build/KernelCache')),
 'full_kernel_input_install_final_gate_passed':False,'physical_hardware_tested':False,
 'blocking_work':['Integrate remaining genuine Linux USB core/bus, complete hub enumeration/disconnect/hotplug and full HID-core/usbhid in one working stack; existing UsbInput/class and private ABI boundaries are not that stack.',
                  'Run final installation, separate Recovery firmware input, kernel, interface/layout/deletion and license/source gates on the fully integrated artifact before release.'],
 'current_controller_runtime_scope':'Whole original UHCI/EHCI/xHCI on serialized CPU0 polling and original URB/HCD functions. Actual coherent DMA, PCI, commands, rings, event completion, bandwidth, cancellation and workers. No claim of physical IRQ routing, IOMMU, system PM or physical ASRock proof.',
 'superspeed_validation_scope':'Original xHCI SuperSpeed EP0 and companion/ESIT contexts exercised by host hardware model; no SuperSpeed HID physical or QEMU-device proof.',
 'history_note':'v17/v18 and older non-USB receipts are historical. Current source/ISO-bound controller/input evidence is enumerated in v19-validation.json. No release or final-stack gate follows from this milestone.'}
(q/'STATUS.json').write_text(json.dumps(status,indent=2)+'\n')
print(json.dumps({k:v for k,v in validation.items() if k not in ['host_receipts','usb_guest_receipts']},indent=2))

# Preserve the independently verified newer canonical kernel experiment when
# refreshing the historical v19/controller report. Never infer usbhid/release.
if (q/'usbcore-kernel-integration-result.json').exists():
 import runpy
 runpy.run_path(str(q/'refresh-usbcore-progress.py'),run_name='__main__')
