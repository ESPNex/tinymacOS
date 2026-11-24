#!/usr/bin/env python3
"""Whole byte-exact UHCI, original core lifecycle, real polled runtime test."""
from pathlib import Path
import hashlib,json,os,subprocess,tempfile
q=Path(__file__).resolve().parent;root=q.parents[1]/'tinymacOS'
paths=['compat/linux/uhci_runtime.c','drivers/uhci.c','include/linux_uhci.h','include/usbhost.h','compat/linux/dma_pool.c','compat/linux/dma-include/tmc_dma_compat.h','compat/linux/include/linux/tmc_urb_types.h','compat/linux/include/linux/tmc_urb_core.h','tests/linux_uhci.c','build.sh','vendor/linux-v6.12/manifest.json','vendor/linux-v6.12/urb.c','vendor/linux-v6.12/dmapool.c','vendor/linux-v6.12/hcd-lifecycle.inc','vendor/linux-v6.12/hcd-endpoint.inc','vendor/linux-v6.12/hcd-bus-time.inc','vendor/linux-v6.12/uhci-reset.inc','vendor/linux-v6.12/pci-quirks.h']
paths+=['vendor/linux-v6.12/'+n for n in ['uhci-hcd.c','uhci-hcd.h','uhci-q.c','uhci-hub.c','uhci-debug.c','uhci-pci.c']]
paths+=sorted(str(p.relative_to(root)) for p in (root/'compat/linux/uhci-include').rglob('*') if p.is_file())
r={'passed':False,'scope':'Whole original Linux UHCI HCD/queue/root-hub/PCI plus original URB/core-lifecycle functions, real coherent DMA, locks, cancellable timers, PCI and polled hardware dispatch. Host model executes actual DMA schedule; native class/enumeration bridge tested separately in QEMU.','whole_original_uhci_files':True,'hardware_irq_routing':False,'system_pm':False,'full_linux_usb_core':False,'streaming_sg_or_external_mappings':False,'physical_hardware_tested':False,'sources':{p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in paths},'runs':[]}
try:
 subprocess.run(['python3','tools/linux-source-contract.py'],cwd=root,check=True)
 with tempfile.TemporaryDirectory(prefix='tmc-uhci-contract-',dir='/var/tmp') as tmp:
  for opt in ['-O1','-O2']:
   output=Path(tmp)/('uhci'+opt)
   command=['gcc','-std=gnu11',opt,'-g','-fwrapv','-fsanitize=address,undefined','-fno-sanitize=alignment','-fno-omit-frame-pointer','-fno-pie','-no-pie','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-but-set-variable','-Wno-misleading-indentation','-Wno-sign-compare','-Iinclude','-Icompat/linux/uhci-include','-Icompat/linux/dma-include','tests/linux_uhci.c','-o',str(output)]
   subprocess.run(command,cwd=root,check=True,capture_output=True)
   result=subprocess.run([str(output)],cwd=root,check=True,capture_output=True,text=True,timeout=60,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'})
   r['runs'].append({'optimization':opt,'passed':True,'sanitizers':'ASan + UBSan except alignment: upstream list_entry creates non-dereferenced, potentially unaligned sentinel pseudo-objects; actual DMA object alignment asserted','stdout':result.stdout});print(result.stdout,end='')
 r['passed']=True
finally:(q/'linux-uhci-contract-result.json').write_text(json.dumps(r,indent=2)+'\n')
