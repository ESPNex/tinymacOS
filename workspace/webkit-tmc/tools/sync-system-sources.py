#!/usr/bin/env python3
"""Keep authored OS integration sources durable in the WebKit workspace.

--check is read-only. --apply explicitly installs the workspace overlay in
TinymacOS. Nothing is deleted; no media or binaries are copied into /home/user.
"""
import argparse
import hashlib
import json
import shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OS=Path('/home/user/tinymacOS')
FILES=['include/threadfinishabi.h','include/wordwaitabi.h','include/tlsabi.h','include/percpu.h','arch/cpu.c','kernel/sched.c','include/uaccess_gate.h','arch/smp.c','include/spinlock.h','include/tlb_mailbox.h','include/netabi.h','include/system_palette.h','kernel/net.c','kernel/syscall.c','kernel/k.h','desktop/gfx.c','kernel/memory.c','kernel/uservm.c','include/abi.h','include/vmabi.h']
p=argparse.ArgumentParser();p.add_argument('--apply',action='store_true');p.add_argument('--check',action='store_true');a=p.parse_args()
records=[]
for name in FILES:
    source=ROOT/'kernel-overlay'/name;target=OS/name
    if a.apply:
        target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target)
    good=source.is_file() and target.is_file() and source.read_bytes()==target.read_bytes()
    if not good:raise SystemExit('Out of sync: '+name)
    records.append({'path':name,'sha256':hashlib.sha256(source.read_bytes()).hexdigest()})
for name in ['netabi.h','system_palette.h','abi.h','vmabi.h','tlsabi.h','wordwaitabi.h','threadfinishabi.h']:
    if (ROOT/'native-abi'/name).read_bytes()!=(ROOT/'kernel-overlay/include'/name).read_bytes():
        raise SystemExit('Native ABI header out of sync: '+name)
(ROOT/'test-results/source-sync.json').write_text(json.dumps(records,indent=2))
print('PASS: all '+str(len(FILES))+' OS integration sources and 7 native ABI copies match the workspace overlay.')
