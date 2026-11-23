#!/usr/bin/env python3
"""Execute complete pinned Linux USB config.c; never claim a full HCD/hub/HID port."""
from pathlib import Path
import hashlib,json,os,subprocess,tempfile
q=Path(__file__).resolve().parent;root=q.parents[1]/'tinymacOS'
paths=['vendor/linux-v6.12/config.c','vendor/linux-v6.12/manifest.json','compat/linux/usb_config.c','include/linux_usb_config.h','include/hidconfig.h','drivers/hidconfig.c','drivers/xhci.c','drivers/usbinput.c','drivers/linux_usb.c','tests/linux_config.c','tests/linux_config_runtime.c']
paths+=sorted(str(p.relative_to(root)) for p in (root/'compat/linux/include').rglob('*.h'))
r={'passed':False,'scope':'Complete original Linux v6.12 drivers/usb/core/config.c configuration/interface/endpoint/SS/SSP/BOS parser and cache lifetime, real native HID/hub-class selection','full_linux_hcd_port':False,'full_linux_hub_driver':False,'generic_linux_hid':False,'native_configuration_limit_bytes':1024,'physical_hardware_tested':False,'sources':{p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in paths},'runs':[]}
try:
 subprocess.run(['python3','tools/linux-source-contract.py'],cwd=root,check=True)
 with tempfile.TemporaryDirectory(prefix='tmc-linux-config-contract-',dir='/var/tmp') as tmp:
  for optimization in ['-O1','-O2']:
   out=Path(tmp)/('test'+optimization)
   command=['gcc',optimization,'-g','-ffreestanding','-mgeneral-regs-only','-fno-builtin','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-Wall','-Wextra','-Werror','-Wno-sign-compare','-Wno-unused-parameter','-Wno-misleading-indentation','-Iinclude','-Icompat/linux/include','tests/linux_config.c','tests/linux_config_runtime.c','drivers/hidconfig.c','-o',str(out)]
   subprocess.run(command,cwd=root,check=True,capture_output=True)
   result=subprocess.run([str(out)],cwd=root,capture_output=True,text=True,check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'})
   r['runs'].append({'optimization':optimization,'freestanding':True,'sanitizers':['address','undefined'],'passed':True,'stdout':result.stdout});print(result.stdout,end='')
  r['passed']=True
finally:(q/'linux-config-contract-result.json').write_text(json.dumps(r,indent=2)+'\n')
