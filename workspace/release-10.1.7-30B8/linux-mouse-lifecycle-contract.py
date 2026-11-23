#!/usr/bin/env python3
"""Real upstream mouse/core + real multi-device/error lifetime shim, not HCD port."""
from pathlib import Path
import hashlib,json,os,subprocess,tempfile
q=Path(__file__).resolve().parent;root=q.parents[1]/'tinymacOS'
paths=['tests/linux_mouse_lifecycle.c','tests/linux_usbmouse.c','compat/linux/usb_input.c','compat/linux/usb_keyboard.c','compat/linux/include/linux/tmc_compat.h','compat/linux/include/linux/tmc_urb_types.h','compat/linux/include/linux/tmc_urb_core.h','include/linux_usb.h','vendor/linux-v6.12/usbmouse.c','vendor/linux-v6.12/usbkbd.c','vendor/linux-v6.12/urb.c','drivers/xhci.c','drivers/ehci.c','drivers/uhci.c','tests/input_maintenance.c']
r={'passed':False,'scope':'Linux-bound mouse button aggregation and transport error handoff to original upstream URB core/IRQ; not a complete HCD port','physical_hardware_tested':False,'sources':{name:hashlib.sha256((root/name).read_bytes()).hexdigest() for name in paths}}
try:
 subprocess.run(['python3','tools/linux-source-contract.py'],cwd=root,check=True)
 with tempfile.TemporaryDirectory(prefix='tmc-mouse-life-contract-',dir='/var/tmp') as tmp:
  out=Path(tmp)/'contract';command=['gcc','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-pthread','-Wall','-Wextra','-Werror','-Wno-sign-compare','-Wno-unused-parameter','-Wno-misleading-indentation','-Iinclude','-Icompat/linux/include','tests/linux_mouse_lifecycle.c','compat/linux/usb_keyboard.c','vendor/linux-v6.12/urb.c','vendor/linux-v6.12/usbmouse.c','-o',str(out)]
  subprocess.run(command,cwd=root,check=True,capture_output=True)
  result=subprocess.run([str(out)],cwd=root,capture_output=True,text=True,check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'})
  r.update(passed=True,sanitizers=['address','undefined'],paired_five_button_states=1024,stdout=result.stdout);print(result.stdout,end='')
finally:(q/'linux-mouse-lifecycle-contract-result.json').write_text(json.dumps(r,indent=2)+'\n')
