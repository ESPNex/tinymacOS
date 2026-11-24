#!/usr/bin/env python3
"""Whole original mm/dmapool.c + actual native backend/controller TD consumer.
This is a DMA prerequisite, not a claim of complete Linux physical HCDs.
"""
from pathlib import Path
import hashlib,json,os,subprocess,tempfile
q=Path(__file__).resolve().parent;root=q.parents[1]/'tinymacOS'
paths=['compat/linux/dma_pool.c','include/linux_dma_pool.h','drivers/linux_dma.c','drivers/ehci.c','drivers/uhci.c','drivers/linux_ehci_qtd.c','include/linux_ehci.h','include/usbhost.h','tests/linux_dma_pool.c','tests/linux_controller_dma.c','tests/linux_dma_native.c','tests/controller_dma_support.h','vendor/linux-v6.12/dmapool.c','vendor/linux-v6.12/manifest.json','build.sh','tools/linux-source-contract.py']
paths+=sorted(str(p.relative_to(root)) for p in (root/'compat/linux/dma-include').rglob('*') if p.is_file())
r={'passed':False,'scope':'Whole byte-exact original Linux v6.12 mm/dmapool.c with real coherent allocation, locks, registered original read attribute and managed resources; actual EHCI/UHCI TD consumers preserve CPU/bus distinction. Physical schedulers remain native','whole_original_dma_pool_file':True,'full_linux_hcd':False,'iommu_supported':False,'physical_hardware_tested':False,'sources':{p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in paths},'runs':[]}
try:
 subprocess.run(['python3','tools/linux-source-contract.py'],cwd=root,check=True)
 with tempfile.TemporaryDirectory(prefix='tmc-dma-contract-',dir='/var/tmp') as tmp:
  for optimization in ['-O1','-O2']:
   for name,sources in [('pool',['tests/linux_dma_pool.c']),('controller',['tests/linux_controller_dma.c','drivers/linux_ehci_qtd.c']),('native',['tests/linux_dma_native.c'])]:
    output=Path(tmp)/(name+optimization)
    command=['gcc','-std=gnu11',optimization,'-g','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-pthread','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-sign-compare','-Wno-unused-parameter','-Wno-misleading-indentation','-Iinclude','-Icompat/linux/dma-include',*sources,'-o',str(output)]
    subprocess.run(command,cwd=root,check=True,capture_output=True)
    result=subprocess.run([str(output)],cwd=root,check=True,capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'})
    r['runs'].append({'component':name,'optimization':optimization,'sanitizers':['address','undefined'],'passed':True,'stdout':result.stdout});print(result.stdout,end='')
 r['passed']=True
finally:(q/'linux-dma-pool-contract-result.json').write_text(json.dumps(r,indent=2)+'\n')
