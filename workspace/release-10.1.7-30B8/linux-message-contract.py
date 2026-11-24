#!/usr/bin/env python3
"""Original synchronous message/URB/HCD control pipeline; NOT complete core/HCD."""
from pathlib import Path
import hashlib,json,os,subprocess,tempfile
q=Path(__file__).resolve().parent;root=q.parents[1]/'tinymacOS'
paths=['compat/linux/usb_input.c','compat/linux/hcd_runtime.inc','compat/linux/usb_hub.c','compat/linux/usb_keyboard.c','drivers/usbinput.c','drivers/xhci.c','drivers/ehci.c','include/linux_usb.h','include/usbhost.h','tests/linux_urb.c','tests/linux_usbmouse.c','tests/linux_hub.c','build.sh','compat/linux/message_runtime.inc','compat/linux/usb_generic.c','tests/linux_message.c','tests/test-input-maintenance.sh']
paths+=sorted(str(p.relative_to(root)) for p in (root/'compat/linux/include').rglob('*') if p.is_file())
paths+=sorted(str(p.relative_to(root)) for p in (root/'vendor/linux-v6.12').glob('*') if p.is_file())
r={'passed':False,'scope':'Byte-exact complete Linux message.c synchronous control/send/recv/get-descriptor functions on original urb.c/hcd.c and bounded native EP0; native enumeration/config/hub/generic call sites wired, NOT complete message.c/core/HCD','complete_message_file_ported':False,'complete_physical_linux_hcd':False,'physical_hardware_tested':False,'sources':{p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in paths},'runs':[]}
try:
 subprocess.run(['python3','tools/linux-source-contract.py'],cwd=root,check=True)
 with tempfile.TemporaryDirectory(prefix='tmc-hcd-hub-contract-',dir='/var/tmp') as tmp:
  for optimization in ['-O1','-O2']:
   for name,sources in [('message',['tests/linux_message.c','compat/linux/usb_keyboard.c','vendor/linux-v6.12/urb.c','vendor/linux-v6.12/usbmouse.c'])]:
    out=Path(tmp)/(name+optimization)
    command=['gcc',optimization,'-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-pthread','-Wall','-Wextra','-Werror','-Wno-sign-compare','-Wno-unused-parameter','-Wno-misleading-indentation','-Iinclude','-Icompat/linux/include',*sources,'-o',str(out)]
    subprocess.run(command,cwd=root,check=True,capture_output=True)
    result=subprocess.run([str(out)],cwd=root,capture_output=True,text=True,check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'})
    r['runs'].append({'component':name,'optimization':optimization,'sanitizers':['address','undefined'],'passed':True,'stdout':result.stdout});print(result.stdout,end='')
 r['passed']=True
finally:(q/'linux-message-contract-result.json').write_text(json.dumps(r,indent=2)+'\n')
