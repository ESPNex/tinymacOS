#!/usr/bin/env python3
"""Whole pinned xHCI + original HCD_BH/TT workers; actual DMA hardware model."""
from pathlib import Path
import hashlib,json,os,subprocess,tempfile
q=Path(__file__).resolve().parent;root=q.parents[1]/'tinymacOS'
paths=['compat/linux/'+n for n in ['xhci_runtime.c','xhci_services.inc','xhci_services2.inc','xhci_pci.c','xhci_mem.c','xhci_ring.c','xhci_hub.c','xhci_extcaps.c','xhci_debug.c','dma_pool.c']]
paths+=['drivers/xhci.c','drivers/usbinput.c','include/linux_xhci_hcd.h','include/hidconfig.h','include/usbhost.h','tests/linux_xhci_hcd.c','tests/test-linux-xhci.sh','build.sh','tools/linux-source-contract.py','vendor/linux-v6.12/manifest.json']
paths+=sorted(str(p.relative_to(root)) for p in (root/'compat/linux/xhci-include').rglob('*') if p.is_file())
paths+=sorted(str(p.relative_to(root)) for p in (root/'vendor/linux-v6.12').glob('xhci*') if p.is_file())
paths+=['vendor/linux-v6.12/'+n for n in ['urb.c','dmapool.c','hcd-lifecycle.inc','hcd-endpoint.inc','hcd-bus-time.inc','hcd-giveback-bh.inc','hub-tt.inc']]
r={'passed':False,'scope':'Whole original Linux xHCI core/memory/ring/root-hub/PCI/ext-caps/debug, original HCD_BH giveback and hub TT worker functions on a private serialized real-service runtime. Native UsbInput enumeration/class facade is separate; guest tests must additionally validate actual ISO input.','whole_original_xhci_files':True,'original_hcd_bh_functions':True,'super_speed_host_model_only':True,'controller_allocation_budgets_per_optimization':96,'endpoint_allocation_budgets_per_optimization':32,'callback_requeues_per_optimization':600,'hardware_irq_routing':False,'system_pm':False,'full_linux_usb_core_hub_usbhid':False,'physical_hardware_tested':False,'sources':{p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in paths},'runs':[]}
def checked(command,**kwargs):
 result=subprocess.run(command,cwd=root,capture_output=True,text=True,**kwargs)
 if result.returncode:
  print(result.stdout);print(result.stderr);result.check_returncode()
 return result
try:
 checked(['python3','tools/linux-source-contract.py'])
 result=checked(['sh','tests/test-linux-xhci.sh'],timeout=120,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'})
 r['runs'].append({'optimizations':['-O1','-O2'],'passed':True,'sanitizers':'ASan + UBSan except alignment for original list sentinel pseudo-objects; actual DMA alignment/nonidentity mapping asserted','stdout':result.stdout,'stderr':result.stderr});print(result.stdout,end='')
 r['passed']=True
finally:(q/'linux-xhci-contract-result.json').write_text(json.dumps(r,indent=2)+'\n')
