#!/usr/bin/env python3
"""Compile/execute actual pinned upstream Linux Boot mouse, not an HCD receipt."""
from pathlib import Path
import hashlib,json,subprocess,tempfile
q=Path(__file__).resolve().parent;root=q.parents[1]/'tinymacOS';w=Path(tempfile.mkdtemp(prefix='tmc-linux-usb-contract-',dir='/var/tmp'));r={'passed':False,'linux_boot_mouse_port':True,'linux_hcd_port':False,'linux_full_usb_core_port':False,'physical_hardware_tested':False,'sources':{}}
try:
 manifest=json.loads((root/'vendor/linux-v6.12/manifest.json').read_text())
 for item in manifest['files']:
  p=root.parent/item['path'];assert hashlib.file_digest(p.open('rb'),'sha256').hexdigest()==item['sha256'];r['sources'][item['path']]=item['sha256']
 for name in ['compat/linux/message_runtime.inc', 'vendor/linux-v6.12/message-control.inc', 'vendor/linux-v6.12/message-descriptor.inc', 'vendor/linux-v6.12/message.c']+['compat/linux/hcd_runtime.inc', 'compat/linux/include/linux/tmc_hcd_core.h', 'vendor/linux-v6.12/hcd.c', 'vendor/linux-v6.12/hcd-endpoint.inc', 'vendor/linux-v6.12/hcd-lifecycle.inc']+['compat/linux/usb_input.c','drivers/linux_usb.c','drivers/usbinput.c','drivers/xhci.c','include/linux_usb.h','tests/linux_usbmouse.c','compat/linux/include/linux/tmc_compat.h','compat/linux/include/linux/tmc_urb_types.h','compat/linux/include/linux/tmc_urb_core.h']:
  r['sources'][name]=hashlib.file_digest((root/name).open('rb'),'sha256').hexdigest()
 command=['gcc','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-pthread','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-Wno-misleading-indentation','-Wno-sign-compare','-I'+str(root/'include'),'-I'+str(root/'compat/linux/include'),str(root/'tests/linux_usbmouse.c'),str(root/'vendor/linux-v6.12/usbmouse.c'),str(root/'compat/linux/usb_keyboard.c'),str(root/'vendor/linux-v6.12/urb.c'),'-o',str(w/'contract')]
 subprocess.run(command,check=True);result=subprocess.run([str(w/'contract')],capture_output=True,text=True,check=True);print(result.stdout);r.update(passed=True,stdout=result.stdout,stderr=result.stderr,sanitizers=['address','undefined'],concurrent_probe_report_disconnect_cycles=4000,original_upstream_source_byte_exact=True)
finally:(q/'linux-usbmouse-contract-result.json').write_text(json.dumps(r,indent=2)+'\n')
