#!/usr/bin/env python3
"""Source-bound native generic/HID/input transport and ownership host proof.
Scripted EP0, actual upstream HID code, actual shared native input consumer.
"""
from pathlib import Path
import hashlib,json,os,subprocess,tempfile
q=Path(__file__).resolve().parent;root=q.parents[1]/'tinymacOS'
paths=['compat/linux/usb_generic.c','compat/linux/usb_input.c','compat/linux/usb_keyboard.c','compat/linux/hid_parser.c','compat/linux/hid_generic_runtime.inc','compat/linux/usb_config.c','include/linux_usb.h','include/linux_hid_generic.h','include/usbhost.h','drivers/input.c','drivers/usbinput.c','drivers/xhci.c','drivers/ehci.c','drivers/uhci.c','tests/linux_usb_generic.c','tests/input_enumeration.c','tests/usbinput_contract.c','tests/linux_hid_runtime.c','tests/linux_usb_host_runtime.c','tests/test-input-maintenance.sh','tests/test-legacy-input.sh','build.sh']
paths+=sorted(str(p.relative_to(root)) for p in (root/'compat/linux/include').rglob('*') if p.is_file())
paths+=sorted(str(p.relative_to(root)) for p in (root/'vendor/linux-v6.12').glob('*') if p.is_file())
r={'passed':False,'scope':'Native generic adapter, whole original Linux generic/input and actual shared Italian keyboard/pointer consumer; host EP0 fault injection, not complete Linux HCD/core/hub','native_endpoint_wiring_present':True,'native_hcd_transport':True,'full_linux_hcd':False,'full_linux_usb_core_hub':False,'ps2_class_drivers_deferred':True,'physical_hardware_tested':False,'sources':{p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in paths},'runs':[]}
try:
 with tempfile.TemporaryDirectory(prefix='tmc-generic-native-contract-',dir='/var/tmp') as tmp:
  for optimization in ['-O1','-O2']:
   out=Path(tmp)/('test'+optimization)
   command=['gcc',optimization,'-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Wall','-Wextra','-Werror','-Wno-sign-compare','-Wno-unused-parameter','-Wno-misleading-indentation','-Iinclude','-Icompat/linux/include','tests/linux_usb_generic.c','tests/linux_usb_host_runtime.c','tests/linux_hid_runtime.c','compat/linux/hid_parser.c','compat/linux/usb_generic.c','compat/linux/usb_input.c','compat/linux/usb_keyboard.c','vendor/linux-v6.12/urb.c','vendor/linux-v6.12/usbmouse.c','-o',str(out)]
   subprocess.run(command,cwd=root,check=True,capture_output=True)
   result=subprocess.run([str(out)],cwd=root,capture_output=True,text=True,check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'})
   r['runs'].append({'optimization':optimization,'sanitizers':['address','undefined'],'passed':True,'stdout':result.stdout});print(result.stdout,end='')
 r['passed']=True
finally:(q/'linux-generic-native-contract-result.json').write_text(json.dumps(r,indent=2)+'\n')
