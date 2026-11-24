#!/usr/bin/env python3
"""Whole pinned EHCI + original HCD_BH/TT workers; actual DMA hardware model."""
from pathlib import Path
import hashlib,json,os,subprocess,tempfile
q=Path(__file__).resolve().parent;root=q.parents[1]/'tinymacOS'
paths=['compat/linux/ehci_runtime.c','compat/linux/ehci_services.inc','compat/linux/ehci_pci.c','drivers/ehci.c','include/linux_ehci_hcd.h','compat/linux/dma_pool.c','compat/linux/dma-include/tmc_dma_compat.h','compat/linux/include/linux/tmc_urb_types.h','compat/linux/include/linux/tmc_urb_core.h','tests/linux_ehci_hcd.c','build.sh','drivers/linux_usb.c','vendor/linux-v6.12/manifest.json']
paths+=['vendor/linux-v6.12/'+n for n in ['urb.c','dmapool.c','hcd-lifecycle.inc','hcd-endpoint.inc','hcd-bus-time.inc','hcd-giveback-bh.inc','hub-tt.inc','ehci-hcd.c','ehci.h','ehci-q.c','ehci-mem.c','ehci-sched.c','ehci-hub.c','ehci-timer.c','ehci-pci.c','ehci-sysfs.c','ehci-dbg.c','ehci_def.h','ehci-dbgp.h']]
paths+=sorted(str(p.relative_to(root)) for p in (root/'compat/linux/ehci-include').rglob('*') if p.is_file())
r={'passed':False,'scope':'Whole original Linux EHCI core/queue/memory/schedule/root-hub/timer/PCI, original HCD_BH giveback and hub TT worker functions on a private serialized real-service runtime. Native UsbInput enumeration/class facade is separate; guest tests must additionally validate actual ISO input.','whole_original_ehci_files':True,'original_hcd_bh_functions':True,'hardware_irq_routing':False,'system_pm':False,'full_linux_usb_core_hub_usbhid':False,'physical_hardware_tested':False,'sources':{p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in paths},'runs':[]}
def checked(command,**kwargs):
 result=subprocess.run(command,cwd=root,capture_output=True,text=True,**kwargs)
 if result.returncode:
  print(result.stdout);print(result.stderr);result.check_returncode()
 return result
try:
 checked(['python3','tools/linux-source-contract.py'])
 with tempfile.TemporaryDirectory(prefix='tmc-ehci-contract-',dir='/var/tmp') as tmp:
  for opt in ['-O1','-O2']:
   output=Path(tmp)/('ehci'+opt);dma=Path(tmp)/('dma'+opt+'.o')
   flags=['gcc','-std=gnu11',opt,'-g','-fwrapv','-fsanitize=address,undefined','-fno-sanitize=alignment','-fno-omit-frame-pointer','-fno-pie','-no-pie','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-but-set-variable','-Wno-misleading-indentation','-Wno-sign-compare','-Iinclude']
   checked(flags+['-Icompat/linux/dma-include','-c','compat/linux/dma_pool.c','-o',str(dma)])
   checked(flags+['-Icompat/linux/ehci-include','tests/linux_ehci_hcd.c','compat/linux/ehci_pci.c',str(dma),'-o',str(output)])
   result=checked([str(output)],timeout=60,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'})
   r['runs'].append({'optimization':opt,'passed':True,'sanitizers':'ASan + UBSan except alignment: upstream list_entry sentinel pseudo-objects; actual DMA alignment asserted','stdout':result.stdout,'stderr':result.stderr});print(result.stdout,end='')
 r['passed']=True
finally:(q/'linux-ehci-contract-result.json').write_text(json.dumps(r,indent=2)+'\n')
