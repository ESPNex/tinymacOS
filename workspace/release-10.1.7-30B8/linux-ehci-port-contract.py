#!/usr/bin/env python3
"""Repeatable host checks for the PARTIAL Linux EHCI qTD port and Intel mux.
Neither fixture emulates Bay Trail silicon or supplies a complete Linux HCD.
"""
from pathlib import Path
import hashlib,json,re,subprocess,tempfile
q=Path(__file__).resolve().parent;root=q.parents[1]/'tinymacOS'
w=Path(tempfile.mkdtemp(prefix='tmc-linux-ehci-host-',dir='/var/tmp'))
def sha(p):return hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
def source_hashes(names):return {name:sha(root/name) for name in names}
subprocess.run(['python3',str(root/'tools/linux-source-contract.py')],check=True)
common=['gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-I'+str(root/'include')]
checks=[('linux-ehci-qtd',['tests/linux_ehci_qtd.c','drivers/linux_ehci_qtd.c'],['include/linux_ehci.h','drivers/ehci.c','vendor/linux-v6.12/ehci-q.c','vendor/linux-v6.12/ehci-qtd-fill.inc','vendor/linux-v6.12/manifest.json','tools/linux-source-contract.py']),('intel-usb-routing',['tests/usbroute.c'],['drivers/usbroute.c','include/usbroute.h'])]
for name,compile_files,additional in checks:
 r={'passed':False,'scope':'Host contract, not full HCD/general compatibility/hardware acceptance','asan_ubsan':True,'physical_hardware_tested':False,'work':str(w),'sources':source_hashes(compile_files+additional)}
 try:
  exe=w/name;subprocess.run(common+[str(root/f) for f in compile_files]+['-o',str(exe)],check=True)
  output=subprocess.check_output([str(exe)],text=True);r.update(passed=True,stdout=output);print(output,end='')
  if name=='linux-ehci-qtd':r['parameter_cases']=int(re.search(r'(\d+) page/alignment',output).group(1));r['complete_linux_hcd_port']=False
  else:r['controller_family_ids']=['8086:0f35','8086:1e31','8086:8c31','8086:8cb1','8086:9c31','8086:9cb1'];r['bay_trail_electrical_emulation']=False
 finally:(q/f'{name}-contract-result.json').write_text(json.dumps(r,indent=2)+'\n')
