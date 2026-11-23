#!/usr/bin/env python3
"""Genuine pinned Linux HID parser function block, NOT full HID/HCD port."""
from pathlib import Path
import hashlib,json,os,subprocess,tempfile
q=Path(__file__).resolve().parent;root=q.parents[1]/'tinymacOS'
paths=['vendor/linux-v6.12/hid-core.c','vendor/linux-v6.12/hid-upstream.h','vendor/linux-v6.12/manifest.json','compat/linux/hid_parser.c','compat/linux/include/linux/tmc_hid_parser.h','include/linux_hid_parser.h','include/linux_usb.h','drivers/xhci.c','drivers/usbinput.c','drivers/linux_usb.c','drivers/hidtablet.c','tests/linux_hid_parser.c','tests/linux_hid_runtime.c','build.sh']
paths+=sorted(str(p.relative_to(root)) for p in (root/'vendor/linux-v6.12').glob('hid-parser-*.inc'))
r={'passed':False,'scope':'Byte-exact Linux v6.12 hid-core descriptor parser/field/collection/open-close/signed/multiplier function extractions; real native HCD pointer-layout consumer; not whole hid-core/hid-input/hid-generic','whole_linux_hid_core':False,'linux_hid_generic_driver':False,'linux_hid_input_events':False,'linux_hcd_port':False,'physical_hardware_tested':False,'sources':{p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in paths},'runs':[]}
try:
 subprocess.run(['python3','tools/linux-source-contract.py'],cwd=root,check=True)
 # Prove model and constants are derived from the original header, not the
 # preexisting Boot-driver compatibility ABI. Device/driver subset is explicit.
 upstream=(root/'vendor/linux-v6.12/hid-upstream.h').read_text();abi=(root/'compat/linux/include/linux/tmc_hid_parser.h').read_text()
 for first,last in [('struct hid_item {','/*\n * HID device quirks.'),('struct hid_global {','struct hid_control_fifo {'),('#define HID_GLOBAL_STACK_SIZE','struct hid_class_descriptor {')]:
  if last not in upstream:last='#define HID_QUIRK_NOTOUCH'
  block=upstream[upstream.index(first):upstream.index(last)]
  assert block in abi,first
 r['upstream_model_and_constants_byte_exact']=True
 with tempfile.TemporaryDirectory(prefix='tmc-linux-hid-contract-',dir='/var/tmp') as tmp:
  for optimization in ['-O1','-O2']:
   out=Path(tmp)/('test'+optimization)
   command=['gcc',optimization,'-g','-ffreestanding','-mgeneral-regs-only','-fno-builtin','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-pthread','-Wall','-Wextra','-Werror','-Wno-sign-compare','-Wno-unused-parameter','-Wno-misleading-indentation','-Iinclude','-Icompat/linux/include','tests/linux_hid_parser.c','tests/linux_hid_runtime.c','compat/linux/hid_parser.c','drivers/hidtablet.c','-o',str(out)]
   subprocess.run(command,cwd=root,check=True,capture_output=True)
   result=subprocess.run([str(out)],cwd=root,capture_output=True,text=True,check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'})
   r['runs'].append({'optimization':optimization,'freestanding':True,'sanitizers':['address','undefined'],'passed':True,'stdout':result.stdout});print(result.stdout,end='')
  r['passed']=True
finally:(q/'linux-hid-parser-contract-result.json').write_text(json.dumps(r,indent=2)+'\n')
