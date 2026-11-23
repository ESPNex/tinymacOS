#!/usr/bin/env python3
"""Complete original Linux v6.12 core/urb.c, bounded native backend, not full core/HCD."""
from pathlib import Path
import hashlib,json,os,subprocess,tempfile
q=Path(__file__).resolve().parent;home=q.parents[1];root=home/'tinymacOS'
paths=['vendor/linux-v6.12/urb.c','vendor/linux-v6.12/manifest.json','tests/linux_urb.c','tests/linux_usbmouse.c','compat/linux/usb_input.c','compat/linux/usb_keyboard.c','compat/linux/include/linux/tmc_compat.h','compat/linux/include/linux/tmc_urb_types.h','compat/linux/include/linux/tmc_urb_core.h','include/linux_usb.h']
paths+=['compat/linux/hcd_runtime.inc', 'compat/linux/include/linux/tmc_hcd_core.h', 'vendor/linux-v6.12/hcd.c', 'vendor/linux-v6.12/hcd-endpoint.inc', 'vendor/linux-v6.12/hcd-lifecycle.inc']
r={'passed':False,'scope':'Complete byte-exact upstream core/urb.c replaces handwritten URB APIs; native HCD backend remains bounded interrupt-IN/EP0-OUT only','full_linux_usb_core_port':False,'linux_hcd_port':False,'physical_hardware_tested':False,'sources':{name:hashlib.sha256((root/name).read_bytes()).hexdigest() for name in paths}}
try:
 subprocess.run(['python3','tools/linux-source-contract.py'],cwd=root,check=True)
 with tempfile.TemporaryDirectory(prefix='tmc-urb-contract-',dir='/var/tmp') as tmp:
  out=Path(tmp)/'contract'
  command=['gcc','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-pthread','-Wall','-Wextra','-Werror','-Wno-sign-compare','-Wno-unused-parameter','-Wno-misleading-indentation','-Iinclude','-Icompat/linux/include','tests/linux_urb.c','compat/linux/usb_keyboard.c','vendor/linux-v6.12/urb.c','vendor/linux-v6.12/usbmouse.c','-o',str(out)]
  subprocess.run(command,cwd=root,check=True,capture_output=True)
  result=subprocess.run([str(out)],cwd=root,text=True,capture_output=True,check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'})
  r.update(passed=True,sanitizers=['address','undefined'],stdout=result.stdout);print(result.stdout,end='')
finally:(q/'linux-urb-contract-result.json').write_text(json.dumps(r,indent=2)+'\n')
