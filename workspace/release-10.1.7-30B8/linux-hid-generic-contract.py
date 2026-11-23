#!/usr/bin/env python3
"""Complete original hid-generic/hid-input execution on scripted host transport.
This deliberately does not assert native-endpoint wiring, Linux HCDs or core/hub.
"""
from pathlib import Path
import hashlib,json,os,subprocess,tempfile
q=Path(__file__).resolve().parent;root=q.parents[1]/'tinymacOS'
paths=['compat/linux/hid_parser.c','compat/linux/hid_generic_runtime.inc','compat/linux/include/linux/tmc_hid_parser.h','compat/linux/include/linux/tmc_hid_input.h','include/linux_hid_parser.h','include/linux_hid_generic.h','tests/linux_hid_generic.c','tests/linux_hid_runtime.c','vendor/linux-v6.12/manifest.json']
paths+=sorted(str(p.relative_to(root)) for p in (root/'vendor/linux-v6.12').glob('hid*'))
paths+=['vendor/linux-v6.12/input-event-codes.h','licenses/LINUX-syscall-note.txt']
paths+=['compat/linux/include/linux/'+p for p in ['module.h','slab.h','kernel.h','hid.h','hid-debug.h','unaligned.h']]+['compat/linux/include/asm/byteorder.h']
r={'passed':False,'scope':'Complete unchanged Linux v6.12 hid-generic.c and hid-input.c, exact core event/bit/output function ranges, real bounded transport and event sink services; host fault-injection proof only','original_generic_driver_whole_file':True,'original_hid_input_whole_file':True,'native_usb_endpoints_wired_to_generic':False,'full_linux_hid_core_or_bus':False,'full_linux_usb_core_hub':False,'full_linux_hcd':False,'ps2_class_driver_work_deferred':True,'physical_hardware_tested':False,'sources':{p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in paths},'runs':[]}
try:
 subprocess.run(['python3','tools/linux-source-contract.py'],cwd=root,check=True)
 with tempfile.TemporaryDirectory(prefix='tmc-hid-generic-contract-',dir='/var/tmp') as tmp:
  for optimization in ['-O1','-O2']:
   out=Path(tmp)/('test'+optimization)
   command=['gcc',optimization,'-g','-ffreestanding','-mgeneral-regs-only','-fno-builtin','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-Wall','-Wextra','-Werror','-Wno-sign-compare','-Wno-unused-parameter','-Wno-misleading-indentation','-Iinclude','-Icompat/linux/include','tests/linux_hid_generic.c','tests/linux_hid_runtime.c','-o',str(out)]
   subprocess.run(command,cwd=root,check=True,capture_output=True)
   symbols=subprocess.check_output(['nm',str(out)],text=True).splitlines()
   for name in ['hidinput_connect','hidinput_disconnect','hidinput_hid_event','hidinput_report_event','hidinput_calc_abs_res','hid_generic_probe','hid_generic_match','tmc_hid_generic_attach','tmc_hid_generic_report']:
    assert any(s.endswith(' '+name) for s in symbols),name
   result=subprocess.run([str(out)],cwd=root,capture_output=True,text=True,check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'})
   r['runs'].append({'optimization':optimization,'whole_original_units_linked':True,'freestanding':True,'sanitizers':['address','undefined'],'upstream_signed_overflow_compiler_semantics':'hid-input compiled with fwrapv, matching Linux no-strict-overflow semantics','passed':True,'stdout':result.stdout});print(result.stdout,end='')
  r['passed']=True
finally:(q/'linux-hid-generic-contract-result.json').write_text(json.dumps(r,indent=2)+'\n')
