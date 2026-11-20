#!/usr/bin/env python3
"""Host VM model and backend contracts; NOT hardware/MMU/SMP validation."""
import json,subprocess
from pathlib import Path
r=Path(__file__).resolve().parents[1]
os=r.parent.parent/'tinymacOS'
out=Path('/var/tmp/tmc-webkit-vm-tests');out.mkdir(exist_ok=True)
fixture=(r/'tests/vm_kernel_contract.c').read_text()
source=(os/'kernel/uservm.c').read_text()
assert source.startswith('#include "k.h"\n')
source=source.removeprefix('#include "k.h"\n')
(out/'kernel-vm-model.c').write_text(fixture.replace('/* KERNEL_SOURCE */',source))
flags=['gcc','-std=c11','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(r/'native-abi'),'-I'+str(r/'posix/include')]
records=[]
for name,files in [('kernel-model',[out/'kernel-vm-model.c']),('backend',[r/'tests/vm_backend_contract.c',r/'posix/src/vm.c'])]:
 subprocess.run(flags+list(map(str,files))+['-o',str(out/name)],check=True)
 x=subprocess.run([str(out/name)],capture_output=True,text=True)
 print(x.stdout,end='');print(x.stderr,end='')
 records.append({'name':name,'exit_code':x.returncode,'stdout':x.stdout,'stderr':x.stderr})
(r/'test-results/vm-host.json').write_text(json.dumps({'scope':'host page table/lock model and syscall contract; not real MMU protection','passed':all(x['exit_code']==0 for x in records),'cases':records},indent=2))
assert all(x['exit_code']==0 for x in records)
