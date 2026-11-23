#!/usr/bin/env python3
"""Measure the existing POSIX shim against pinned upstream driver translation units.
An expected compilation failure is evidence of a missing kernel compatibility
layer, NEVER a driver-port PASS. No stub implementation or Linux HCD is linked.
"""
from pathlib import Path
import hashlib,json,re,subprocess
q=Path(__file__).resolve().parent;home=q.parents[1];base=Path('/var/tmp/tmc-linux-v6.12');manifest=json.loads((base/'manifest.json').read_text());posix=home/'workspace/webkit-tmc/posix'
results=[]
for item in manifest:
 p=base/item['path'];assert hashlib.sha256(p.read_bytes()).hexdigest()==item['sha256'];source=p.read_text()
 command=['gcc','-fsyntax-only','-ffreestanding','-Werror=implicit-function-declaration','-I'+str(posix/'include'),'-I'+str(posix/'libc-headers'),str(p)]
 r=subprocess.run(command,capture_output=True,text=True)
 results.append({'path':item['path'],'upstream':item['url'],'sha256':item['sha256'],'includes':re.findall(r'^#include [<"]([^>"\n]+)',source,re.M),'exit_code':r.returncode,'stderr':r.stderr,'compiles_with_existing_posix_shim':r.returncode==0})
 print(('COMPILES' if r.returncode==0 else 'BLOCKED'),item['path'],r.stderr.splitlines()[0] if r.stderr else '',flush=True)
report={'scope':'Compatibility feasibility only; NOT a driver port or runtime acceptance','baseline':'Linux v6.12 (pinned reference, not a latest-version claim)','linux_hcd_linked_in_os':False,'linux_driver_runtime_tested':False,'posix_shim_sufficient':all(r['compiles_with_existing_posix_shim'] for r in results),'cases':results,'required_kernel_services':['PCI resource/ownership and reset lifecycle','DMA coherent/streaming mapping, IOMMU and cache/barrier semantics','IRQ registration and teardown, IRQ-safe locks','URB and usb_hcd/usb_bus/device/endpoint model','USB core enumeration, hubs, transaction translators and power lifecycle','timers, tasklets/workqueues, completions, wait queues and cancellation','HID core, USB HID transport, input-event delivery and Italian keyboard integration','GPL source/provenance and build/runtime regression coverage']}
(q/'linux-posix-feasibility.json').write_text(json.dumps(report,indent=2)+'\n')
