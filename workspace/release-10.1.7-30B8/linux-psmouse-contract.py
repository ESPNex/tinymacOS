#!/usr/bin/env python3
"""Original packet/detection functions, not the complete Linux psmouse driver."""
from pathlib import Path
import hashlib,json,os,subprocess,tempfile
q=Path(__file__).resolve().parent;root=q.parents[1]/'tinymacOS'
paths=['vendor/linux-v6.12/psmouse-base.c','vendor/linux-v6.12/psmouse.h','vendor/linux-v6.12/psmouse-packets.inc','vendor/linux-v6.12/psmouse-detect.inc','vendor/linux-v6.12/manifest.json','compat/linux/psmouse_input.c','compat/linux/ps2_protocol.c','compat/linux/ps2_input.c','compat/linux/usb_input.c','compat/linux/include/linux/tmc_compat.h','include/linux_ps2.h','tests/linux_psmouse.c','tests/linux_usbmouse.c','drivers/ps2.c','drivers/input.c','drivers/linux_usb.c','kernel/k.h']
paths+=['compat/linux/hcd_runtime.inc', 'compat/linux/include/linux/tmc_hcd_core.h', 'vendor/linux-v6.12/hcd.c', 'vendor/linux-v6.12/hcd-endpoint.inc', 'vendor/linux-v6.12/hcd-lifecycle.inc']
r={'passed':False,'scope':'Complete unchanged Linux psmouse standard/extended packet functions and IntelliMouse/Explorer detect functions, executed with original libps2 and actual native adapter','complete_linux_psmouse_driver':False,'full_linux_i8042_driver':False,'linux_atkbd_driver':False,'physical_hardware_tested':False,'sources':{p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in paths},'runs':[]}
try:
 subprocess.run(['python3','tools/linux-source-contract.py'],cwd=root,check=True)
 with tempfile.TemporaryDirectory(prefix='tmc-psmouse-contract-',dir='/var/tmp') as tmp:
  for optimization in ['-O1','-O2']:
   out=Path(tmp)/('test'+optimization)
   command=['gcc',optimization,'-g','-ffreestanding','-mgeneral-regs-only','-fno-builtin','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-pthread','-Wall','-Wextra','-Werror','-Wno-sign-compare','-Wno-unused-parameter','-Wno-misleading-indentation','-Iinclude','-Icompat/linux/include','tests/linux_psmouse.c','compat/linux/ps2_protocol.c','compat/linux/psmouse_input.c','compat/linux/usb_keyboard.c','vendor/linux-v6.12/urb.c','vendor/linux-v6.12/usbmouse.c','-o',str(out)]
   subprocess.run(command,cwd=root,check=True,capture_output=True)
   result=subprocess.run([str(out)],cwd=root,capture_output=True,text=True,check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'})
   r['runs'].append({'optimization':optimization,'freestanding':True,'sanitizers':['address','undefined'],'passed':True,'stdout':result.stdout});print(result.stdout,end='')
  r['passed']=True
finally:(q/'linux-psmouse-contract-result.json').write_text(json.dumps(r,indent=2)+'\n')
