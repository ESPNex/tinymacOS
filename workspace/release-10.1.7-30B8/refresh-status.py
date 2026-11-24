#!/usr/bin/env python3
"""Refresh truthful development status; this NEVER commits, uploads or publishes."""
from pathlib import Path
import filecmp,hashlib,json,os,subprocess
q=Path(__file__).resolve().parent;root=q.parents[1]/'tinymacOS';base=Path(os.environ.get('TMC_LINUX_STAGE','/var/tmp/tmc-10.1.7/linux-message-v15'));iso=base/'tinymacOS-10.1.7-x86_64.iso'
def sha(p):return hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
def load(name):
 p=q/name;return json.loads(p.read_text()) if p.exists() else {}
identity=sha(iso)
def current(name):
 r=load(name);return bool(r.get('passed') and r.get('iso_sha256')==identity)
manifest={str(p.relative_to(root)):sha(p) for p in sorted(root.rglob('*')) if p.is_file() and '__pycache__' not in p.parts}
(q/'staging-source-hashes.json').write_text(json.dumps(manifest,indent=2)+'\n')
other=Path('/var/tmp/tmc-10.1.7/linux-message-v15-independent/tinymacOS-10.1.7-x86_64.iso');repro=other.exists() and filecmp.cmp(iso,other,shallow=False)
(q/'reproducible-build-result.json').write_text(json.dumps({'passed':repro,'scope':'Two normal builds + dualboot packaging; updated About, firmware-only Recovery keyboard and upstream Linux Boot mouse/keyboard/core urb.c/config.c/EHCI qTD function and complete libps2 plus psmouse packet/detect and hid-core descriptor parser functions; all component notices in optical/installed FAT; not an HCD/atkbd/psmouse/full-stack port; bounded original hid-generic/hid-input now active','sha256':identity,'bytes':iso.stat().st_size,'builds':[str(base/'build'),str(other.parent/'build')],'byte_comparison_exit_code':0 if repro else 1,'published':False},indent=2)+'\n')
names=[f'linux-usbmouse-{fw}-{p}.json' for fw in ['uefi','bios'] for p in ['xhci','ehci','uhci','companion','xhci-hub','xhci-nested-hub']]
ports=sum(current(n) for n in names)
symbols=subprocess.check_output(['nm',str(base/'build/kernel.debug')],text=True)
linked=all(any(line.endswith(' '+name) for line in symbols.splitlines()) for name in ['usb_mouse_irq','usb_mouse_probe','tmc_lx_usbmouse_driver'])
contract=load('linux-usbmouse-contract-result.json');host_current=bool(contract.get('passed'))
for name,digest in contract.get('sources',{}).items():
 path=root.parent/name if name.startswith('tinymacOS/') else root/name
 if not path.exists() or sha(path)!=digest:host_current=False
status={'state':'UNRELEASED / partial Linux USB and PS/2 port','requested_version':'10.1.7','requested_build':'30B8','published':False,'stable_release_unchanged':'10.1.6','development_iso_sha256':identity,'development_iso_bytes':iso.stat().st_size,'development_build':str(base/'build'),'source_files':len(manifest),'source_manifest_sha256':sha(q/'staging-source-hashes.json'),'about_version_updated_to_10_1_7':current('identity-contract-result.json') and load('identity-contract-result.json').get('about_version_updated',False),'about_layout_unchanged_except_version_build':load('identity-contract-result.json').get('about_layout_source_unchanged_except_version_and_build',False),'actual_upstream_linux_usbmouse_compiled_and_linked':bool(linked),'actual_linux_hcd_compiled_or_linked':False,'upstream_linux_ehci_qtd_fill_compiled_and_linked':all(any(line.endswith(' '+name) for line in symbols.splitlines()) for name in ['qtd_fill','tmc_linux_ehci_qtd_build']),'linux_ehci_qtd_host_contract_passed':load('linux-ehci-qtd-contract-result.json').get('passed',False),'intel_shared_port_routing_host_contract_passed':load('intel-usb-routing-contract-result.json').get('passed',False),'general_linux_usb_core_or_generic_hid_ported':False,'posix_compatibility_alone_sufficient':False,'linux_usbmouse_host_contract_passed':host_current,'linux_usbmouse_current_guest_topologies_passed':ports,'linux_usbmouse_current_guest_topologies_expected':12,'host_deletion_contracts_passed':load('deletion-contract-result.json').get('passed',False),'actual_opencore_media_contract_passed':current('media-contract-result.json'),'identity_media_contract_passed':current('identity-contract-result.json'),'full_staging_uefi_passed':current('staging-uefi-result.json'),'full_staging_bios_passed':current('staging-bios-result.json'),'recovery_firmware_mouse_xy_passed':all(current(f'recovery-firmware-{fw}-result.json') for fw in ['uefi','bios']),'recovery_keyboard_uses_native_decoder':False,'recovery_keyboard_firmware_contract_passed':current('firmware-keyboard-contract-result.json'),'recovery_keyboard_current_firmware_guest_profiles_passed':sum(current(f'recovery-keyboard-{fw}-{profile}.json') for fw in ['uefi','bios'] for profile in ['ps2','xhci-hub-kbd']),'control_center_guest_passed':current('control-result.json'),'independent_reproducible_build_run':repro,'current_kernel_runtime_regression_passed':load('kernel-current-source-result.json').get('passed',False) and load('kernel-current-source-result.json').get('kernelcache_sha256')==sha(base/'build/KernelCache'),'full_kernel_input_install_final_gate_passed':False,'current_kernel_binary_identical_to_v2':Path('/var/tmp/tmc-10.1.7/linux-usb-v2/build/KernelCache').exists() and filecmp.cmp(base/'build/KernelCache',Path('/var/tmp/tmc-10.1.7/linux-usb-v2/build/KernelCache'),shallow=False),'physical_hardware_tested':False,'blocking_work':['Complete actual Linux xHCI/EHCI HCDs and USB core/hub/IRQ/DMA lifecycle; only EHCI qtd_fill is now upstream, controller/scheduler remain native','Port generic Linux HID; usbmouse/usbkbd are Boot drivers, not generic HID','Rerun the entire install, native input and Recovery release matrix on the fully integrated result; audit source/license correspondence before binary publication'],'evidence_sha256':{p.name:sha(p) for p in sorted((q/'evidence').glob('*.png'))}}
status['actual_upstream_linux_usbkbd_compiled_and_linked']=all(any(line.endswith(' '+name) for line in symbols.splitlines()) for name in ['usb_kbd_irq','usb_kbd_probe','usb_kbd_event','usb_kbd_led','usb_kbd_disconnect','tmc_lx_usbkbd_driver'])
kbd_contract=load('linux-usbkbd-contract-result.json')
status['linux_usbkbd_host_contract_passed']=bool(kbd_contract.get('passed')) and all((root/name).exists() and sha(root/name)==digest for name,digest in kbd_contract.get('sources',{}).items())
status['linux_usbkbd_current_guest_topologies_passed']=sum(current(f'linux-usbkbd-{fw}-{profile}.json') for fw in ['uefi','bios'] for profile in ['ehci-kbd','companion-kbd','xhci-hub-kbd'])
status['linux_usbkbd_current_guest_topologies_expected']=6
status['actual_upstream_linux_usb_core_urb_file_compiled_and_linked']=all(any(line.endswith(' '+name) for line in symbols.splitlines()) for name in ['usb_alloc_urb','usb_init_urb','usb_get_urb','usb_submit_urb','usb_kill_urb','usb_poison_anchored_urbs','usb_wait_anchor_empty_timeout'])
urb_contract=load('linux-urb-contract-result.json')
status['linux_urb_host_contract_passed']=bool(urb_contract.get('passed')) and all((root/name).exists() and sha(root/name)==digest for name,digest in urb_contract.get('sources',{}).items())
status['linux_urb_current_guest_topologies_passed']=sum(current(n) and load(n).get('actual_upstream_core_urb_file_executed',False) for n in names+[f'linux-usbkbd-{fw}-{p}.json' for fw in ['uefi','bios'] for p in ['ehci-kbd','companion-kbd','xhci-hub-kbd']])
status['linux_hcd_backend_scope']='Native, one logical interrupt-IN and EP0-OUT slot per binding, maximum 8 payload bytes; not Linux hcd.c'
status['blocking_work'][0]='Complete actual Linux xHCI/EHCI HCDs and remaining USB core/hubs/IRQ/DMA lifecycle; only core urb.c, Boot drivers and EHCI qtd_fill are upstream so far'
mouse_lifecycle=load('linux-mouse-lifecycle-contract-result.json')
status['linux_mouse_lifecycle_host_contract_passed']=bool(mouse_lifecycle.get('passed')) and all((root/name).exists() and sha(root/name)==digest for name,digest in mouse_lifecycle.get('sources',{}).items())
status['linux_two_mice_current_guest_profiles_passed']=sum(current(f'linux-two-mice-{fw}.json') for fw in ['uefi','bios'])
status['linux_two_mice_current_guest_profiles_expected']=2
status['actual_upstream_linux_libps2_compiled_and_linked']=all(any(line.endswith(' '+name) for line in symbols.splitlines()) for name in ['ps2_command','__ps2_command','ps2_interrupt','ps2_sendbyte','ps2_sliced_command','tmc_linux_ps2_init'])
ps2_contract=load('linux-ps2-contract-result.json')
status['linux_libps2_host_contract_passed']=bool(ps2_contract.get('passed')) and all((root/name).exists() and sha(root/name)==digest for name,digest in ps2_contract.get('sources',{}).items())
status['ps2_keyboard_and_mouse_current_guest_profiles_passed']=sum(current(f'linux-ps2-{fw}.json') for fw in ['uefi','bios'])
status['ps2_keyboard_and_mouse_current_guest_profiles_expected']=2
status['full_linux_i8042_driver_port']=False
status['linux_atkbd_driver_port']=False
status['linux_psmouse_driver_port']=False
status['input_port_scope']={
 'usb_mouse':'Upstream Boot usbmouse + URB core; native HCD; no general HID',
 'usb_keyboard':'Upstream Boot usbkbd + URB core; native HCD; no general HID',
 'ps2_mouse':'Upstream complete libps2 + original psmouse packet and IMPS/Explorer detect functions; native i8042/framing, not complete psmouse',
 'ps2_keyboard':'Upstream complete libps2 command/RX; native i8042/stream decoder/LED, not atkbd'}
status['blocking_work'].insert(2,'Complete actual Linux i8042/serio/atkbd/psmouse driver port; current PS/2 increment is the complete original libps2 library, not the class drivers')
psmouse_contract=load('linux-psmouse-contract-result.json')
status['actual_upstream_psmouse_packet_and_detect_functions_linked']=all(any(line.endswith(' '+name) for line in symbols.splitlines()) for name in ['psmouse_process_byte','psmouse_report_standard_motion','psmouse_report_standard_buttons'])
status['linux_psmouse_functions_host_contract_passed']=bool(psmouse_contract.get('passed')) and all((root/name).exists() and sha(root/name)==digest for name,digest in psmouse_contract.get('sources',{}).items())
status['ps2_usb_shared_pointer_guest_profiles_passed']=sum(current(f'linux-mixed-mice-{fw}.json') for fw in ['uefi','bios'])
status['ps2_usb_shared_pointer_guest_profiles_expected']=2
config_contract=load('linux-config-contract-result.json')
status['actual_upstream_linux_usb_core_config_file_compiled_and_linked']=all(any(line.endswith(' '+name) for line in symbols.splitlines()) for name in ['tmc_config_get_configuration','tmc_config_destroy_configuration','tmc_config_get_bos_descriptor','tmc_config_release_bos_descriptor'])
status['linux_config_host_contract_passed']=bool(config_contract.get('passed')) and all((root/name).exists() and sha(root/name)==digest for name,digest in config_contract.get('sources',{}).items())
status['linux_config_current_guest_topologies_passed']=sum(current(n) and load(n).get('actual_upstream_core_config_file_executed',False) for n in names+[f'linux-usbkbd-{fw}-{p}.json' for fw in ['uefi','bios'] for p in ['ehci-kbd','companion-kbd','xhci-hub-kbd']])
status['linux_config_current_guest_topologies_expected']=18
status['native_configuration_policy']='First configuration, alternate-zero inputs, 1024 bytes; original config.c parses/selects both HID and hub classes'
status['complete_linux_hub_driver_port']=False
status['blocking_work'][0]='Complete actual Linux xHCI/EHCI HCDs and remaining USB core/hub/IRQ/DMA lifecycle; core urb.c/config.c, Boot input drivers and EHCI qtd_fill are upstream so far'
status['current_installer_bios_hdd_reboot_uefi_crossboot_passed']=current('install-current-result.json') and load('install-current-result.json').get('installed_esp_kernel_readback_equals_current_build',False) and load('install-current-result.json').get('installed_kernelcache_sha256')==sha(base/'build/KernelCache')
status['current_runtime_kernel_guest_profiles_passed']=load('kernel-current-source-result.json').get('guest_profiles_passed',0) if status['current_kernel_runtime_regression_passed'] else 0
status['current_runtime_kernel_guest_profiles_expected']=13
status['input_port_scope']['usb_mouse']='Upstream Boot usbmouse + core URB/config; native HCD/hub/report HID'
status['input_port_scope']['usb_keyboard']='Upstream Boot usbkbd + core URB/config; native HCD/hub/report HID'
hid_contract=load('linux-hid-parser-contract-result.json')
status['upstream_linux_hid_parser_functions_compiled_and_linked']=all(any(line.endswith(' '+name) for line in symbols.splitlines()) for name in ['hid_open_report','hid_register_report','hid_setup_resolution_multiplier','tmc_linux_hid_pointer_layout'])
status['linux_hid_parser_host_contract_passed']=bool(hid_contract.get('passed')) and all((root/name).exists() and sha(root/name)==digest for name,digest in hid_contract.get('sources',{}).items())
status['linux_hid_parser_current_guest_topologies_passed']=sum(current(n) and load(n).get('actual_upstream_hid_parser_functions_executed',False) for n in names+[f'linux-usbkbd-{fw}-{p}.json' for fw in ['uefi','bios'] for p in ['ehci-kbd','companion-kbd','xhci-hub-kbd']])
status['linux_hid_parser_current_guest_topologies_expected']=18
generic_contract=load('linux-hid-generic-contract-result.json')
status['linux_hid_generic_and_input_whole_source_host_contract_passed']=bool(generic_contract.get('passed')) and all((root/name).exists() and sha(root/name)==digest for name,digest in generic_contract.get('sources',{}).items())
status['upstream_hid_generic_and_hid_input_whole_files_compiled_and_linked']=all(any(line.endswith(' '+name) for line in symbols.splitlines()) for name in ['hid_generic_probe','hid_generic_match','hidinput_connect','hidinput_disconnect','hidinput_hid_event','hidinput_report_event'])
native_contract=load('linux-generic-native-contract-result.json')
status['native_generic_host_contract_passed']=bool(native_contract.get('passed')) and all((root/name).exists() and sha(root/name)==digest for name,digest in native_contract.get('sources',{}).items())
status['native_usb_endpoints_wired_to_generic_hid_input']=status['native_generic_host_contract_passed'] and all(any(line.endswith(' '+name) for line in symbols.splitlines()) for name in ['tmc_linux_generic_attach','tmc_linux_generic_report','tmc_linux_generic_service'])
status['native_generic_current_guest_profiles_passed']=sum(current(f'linux-generic-native-{fw}-{p}.json') for fw in ['uefi','bios'] for p in ['xhci-tablet','ehci-tablet','uhci-tablet','companion-tablet','xhci-tablet-hub'])
status['native_generic_current_guest_profiles_expected']=10
status['ps2_class_driver_work_deferred_by_user']=True
status['requested_four_block_complete']=False
status['four_block_scope']={'linux_controllers':'not complete; native HCDs remain','usb_core_hub':'not complete; original URB/config only, native enumeration/hub remain','hid_generic':'complete original driver active through bounded native HCD transport; not Linux usbhid/HCD or HID bus','hid_input':'complete original input mapping/event unit active through shared native event consumers; supported applications only'}
status['whole_linux_hid_core_ported']=False
status['linux_hid_generic_driver_ported']=status['native_usb_endpoints_wired_to_generic_hid_input']
status['linux_hid_input_event_dispatch_ported']=status['native_usb_endpoints_wired_to_generic_hid_input']
status['input_port_scope']['usb_mouse']='Upstream Boot usbmouse + core URB/config + whole generic/input for Report-only relative/absolute pointers; native HCD/hub'
status['input_port_scope']['usb_keyboard']='Upstream Boot usbkbd + core URB/config; whole generic keyboard events through shared native sink host-tested; native HCD/hub'
status['blocking_work'][1]='Complete remaining Linux HID core/bus/usbhid/lifecycle and general transport support; bounded generic endpoint/native event integration is now active'
status['installed_and_optical_all_component_notices_byte_exact']=current('media-contract-result.json') and load('media-contract-result.json').get('all_component_notices_in_three_fat_media',False)
status['actual_installed_component_notices_byte_exact']=current('install-current-result.json') and load('install-current-result.json').get('actual_installed_component_notices_byte_exact',False)
hcd_hub=load('linux-hcd-hub-contract-result.json')
status['linux_hcd_hub_functions_host_contract_passed']=bool(hcd_hub.get('passed')) and all((root/name).exists() and sha(root/name)==digest for name,digest in hcd_hub.get('sources',{}).items())
status['original_linux_hcd_lifecycle_functions_active']=all(any(line.endswith(' '+name) for line in symbols.splitlines()) for name in ['usb_hcd_submit_urb','usb_hcd_link_urb_to_ep','usb_hcd_check_unlink_urb','usb_hcd_unlink_urb_from_ep','usb_hcd_unlink_urb'])
status['original_linux_hub_debounce_reset_functions_active']=status['linux_hcd_hub_functions_host_contract_passed'] and all(any(line.endswith(' '+name) for line in symbols.splitlines()) for name in ['tmc_linux_hub_port_init','hub_port_debounce'])
status['linux_hcd_lifecycle_current_guest_profiles_passed']=sum(current(n) and load(n).get('actual_upstream_hcd_lifecycle_functions_executed',False) for n in names)
status['linux_hub_reset_current_guest_profiles_passed']=sum(current(f'linux-usbmouse-{fw}-{p}.json') and load(f'linux-usbmouse-{fw}-{p}.json').get('actual_upstream_hub_debounce_reset_functions_executed',False) for fw in ['uefi','bios'] for p in ['xhci-hub','xhci-nested-hub'])
status['four_block_scope']['usb_core_hub']='not complete; original URB/config and HCD lifecycle/USB2 hub debounce-reset functions active; remaining core/hub/root-hub/device lifecycle missing'
status['blocking_work'][0]='Complete physical Linux xHCI/EHCI/UHCI drivers and remaining USB core/hub/IRQ/DMA/device/root-hub lifecycle; core URB/config, HCD lifecycle functions and USB2 hub debounce/reset are upstream so far'
message=load('linux-message-contract-result.json')
status['linux_message_control_host_contract_passed']=bool(message.get('passed')) and bool(message.get('sources')) and all((root/name).exists() and sha(root/name)==digest for name,digest in message.get('sources',{}).items())
status['original_linux_message_control_functions_linked']=all(any(line.endswith(' '+name) for line in symbols.splitlines()) for name in ['usb_control_msg','usb_control_msg_send','usb_control_msg_recv','usb_get_descriptor','tmc_linux_core_control'])
message_guests=names+[f'linux-usbkbd-{fw}-{p}.json' for fw in ['uefi','bios'] for p in ['ehci-kbd','companion-kbd','xhci-hub-kbd']]+[f'linux-generic-native-{fw}-{p}.json' for fw in ['uefi','bios'] for p in ['xhci-tablet','ehci-tablet','uhci-tablet','companion-tablet','xhci-tablet-hub']]
status['linux_message_control_current_guest_profiles_passed']=sum(current(n) and load(n).get('actual_upstream_message_control_functions_executed',False) for n in message_guests)
status['complete_linux_message_file_ported']=False
status['linux_hcd_backend_scope']='Native copying physical controllers; original URB/HCD lifecycle, eight-byte Boot interrupt/LED slots and serialized message-only EP0 IN/OUT <=1024 bytes; not physical Linux HCDs'
status['four_block_scope']['usb_core_hub']='not complete; original URB/config, synchronous message/descriptor functions, HCD lifecycle and USB2 debounce/reset active; remaining core/hub/root-hub/device lifecycle missing'
status['blocking_work'][0]='Complete physical Linux xHCI/EHCI/UHCI and remaining USB core/hub/IRQ/DMA/device/root-hub lifecycle; original URB/config, sync message/descriptor, HCD lifecycle and USB2 hub debounce/reset are integrated'
(q/'STATUS.json').write_text(json.dumps(status,indent=2)+'\n');print(json.dumps({k:v for k,v in status.items() if k not in ['evidence_sha256','blocking_work']},indent=2))
