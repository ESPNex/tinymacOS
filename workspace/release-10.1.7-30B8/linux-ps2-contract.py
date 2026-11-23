#!/usr/bin/env python3
"""Complete byte-exact upstream libps2.c and the real polled native adapter.
This does not certify full Linux i8042, atkbd or psmouse drivers.
"""
from pathlib import Path
import hashlib,json,os,subprocess,tempfile
q=Path(__file__).resolve().parent;root=q.parents[1]/'tinymacOS'
paths=['vendor/linux-v6.12/libps2.c','vendor/linux-v6.12/libps2.h','vendor/linux-v6.12/manifest.json','compat/linux/ps2_protocol.c','compat/linux/ps2_input.c','compat/linux/include/linux/libps2.h','compat/linux/include/linux/tmc_ps2_compat.h','compat/linux/include/linux/serio.h','include/linux_ps2.h','tests/linux_ps2.c','tests/linux_usbmouse.c','compat/linux/usb_input.c','drivers/ps2.c','drivers/linux_usb.c']
paths+=['compat/linux/hcd_runtime.inc', 'compat/linux/include/linux/tmc_hcd_core.h', 'vendor/linux-v6.12/hcd.c', 'vendor/linux-v6.12/hcd-endpoint.inc', 'vendor/linux-v6.12/hcd-lifecycle.inc']
r={'passed':False,'scope':'Complete original Linux libps2 command/ACK/RX unit on native i8042 transport','full_linux_i8042_driver_port':False,'linux_atkbd_driver_port':False,'linux_psmouse_driver_port':False,'physical_hardware_tested':False,'sources':{name:hashlib.sha256((root/name).read_bytes()).hexdigest() for name in paths}}
try:
 subprocess.run(['python3','tools/linux-source-contract.py'],cwd=root,check=True)
 with tempfile.TemporaryDirectory(prefix='tmc-libps2-contract-',dir='/var/tmp') as tmp:
  out=Path(tmp)/'contract';command=['gcc','-O1','-g','-fno-builtin','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-pthread','-Wall','-Wextra','-Werror','-Wno-sign-compare','-Wno-unused-parameter','-Wno-misleading-indentation','-Iinclude','-Icompat/linux/include','tests/linux_ps2.c','compat/linux/ps2_protocol.c','compat/linux/usb_keyboard.c','vendor/linux-v6.12/urb.c','vendor/linux-v6.12/usbmouse.c','-o',str(out)]
  subprocess.run(command,cwd=root,check=True,capture_output=True)
  result=subprocess.run([str(out)],cwd=root,capture_output=True,text=True,check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'})
  r.update(passed=True,sanitizers=['address','undefined'],stdout=result.stdout);print(result.stdout,end='')
finally:(q/'linux-ps2-contract-result.json').write_text(json.dumps(r,indent=2)+'\n')
