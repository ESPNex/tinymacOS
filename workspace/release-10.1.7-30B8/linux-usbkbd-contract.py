#!/usr/bin/env python3
"""Reproducible host contract for the complete pinned upstream Boot keyboard."""
from pathlib import Path
import hashlib,json,os,subprocess,tempfile
q=Path(__file__).resolve().parent;home=q.parents[1];root=home/'tinymacOS'
paths=['tests/linux_usbkbd.c','drivers/input.c','tests/input_test_support.h','compat/linux/usb_input.c','compat/linux/usb_keyboard.c','compat/linux/include/linux/tmc_compat.h','include/linux_usb.h','vendor/linux-v6.12/usbkbd.c','vendor/linux-v6.12/usbmouse.c','vendor/linux-v6.12/urb.c','compat/linux/include/linux/tmc_urb_types.h','compat/linux/include/linux/tmc_urb_core.h','vendor/linux-v6.12/manifest.json']
paths+=['compat/linux/hcd_runtime.inc', 'compat/linux/include/linux/tmc_hcd_core.h', 'vendor/linux-v6.12/hcd.c', 'vendor/linux-v6.12/hcd-endpoint.inc', 'vendor/linux-v6.12/hcd-lifecycle.inc']
r={'passed':False,'scope':'Complete upstream Linux Boot keyboard + actual native Italian input + deferred native-HCD control adapter; not full HCD/core/generic HID','asan':True,'ubsan':True,'physical_hardware_tested':False,'sources':{p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in paths}}
try:
 subprocess.run(['python3',str(root/'tools/linux-source-contract.py')],check=True)
 with tempfile.TemporaryDirectory(prefix='tmc-linux-kbd-host-',dir='/var/tmp') as tmp:
  out=Path(tmp)/'usbkbd'
  cmd=['gcc','-std=gnu11','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-pthread','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Wno-sign-compare','-Wno-unused-parameter','-Iinclude','-Icompat/linux/include','tests/linux_usbkbd.c','compat/linux/usb_keyboard.c','vendor/linux-v6.12/usbmouse.c','vendor/linux-v6.12/urb.c','-o',str(out)]
  subprocess.run(cmd,cwd=root,check=True,capture_output=True)
  result=subprocess.run([str(out)],cwd=root,check=True,capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'})
  r['output']=result.stdout;r['passed']=True;print(result.stdout,end='')
finally:(q/'linux-usbkbd-contract-result.json').write_text(json.dumps(r,indent=2)+'\n')
