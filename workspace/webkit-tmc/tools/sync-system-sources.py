#!/usr/bin/env python3
"""Keep authored OS integration sources durable in the WebKit workspace.

--check is read-only. --capture refreshes the overlay from authoritative OS sources.
--apply explicitly installs the workspace overlay in
TinymacOS. Nothing is deleted; no media or binaries are copied into /home/user.
"""
import argparse
import hashlib
import json
import shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OS=Path('/home/user/tinymacOS')
FILES=['include/window_update.h','include/base.h','include/diskedit.h','include/partition.h','include/clipboard.h','include/inputdiag.h','include/threadfinishabi.h','include/wordwaitabi.h','include/tlsabi.h','include/percpu.h','arch/cpu.c','kernel/sched.c','include/uaccess_gate.h','arch/smp.c','include/spinlock.h','include/tlb_mailbox.h','include/netabi.h','include/system_palette.h','kernel/net.c','kernel/syscall.c','kernel/k.h','desktop/gfx.c','kernel/memory.c','kernel/uservm.c','include/abi.h','include/vmabi.h']
p=argparse.ArgumentParser();g=p.add_mutually_exclusive_group();g.add_argument('--apply',action='store_true');g.add_argument('--capture',action='store_true');g.add_argument('--check',action='store_true');a=p.parse_args()
records=[]
for name in FILES:
    source=ROOT/'kernel-overlay'/name;target=OS/name
    if a.capture:
        if not target.is_file():raise SystemExit('Missing authoritative source: '+name)
        source.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(target,source)
    if a.apply:
        target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target)
    good=source.is_file() and target.is_file() and source.read_bytes()==target.read_bytes()
    if not good:raise SystemExit('Out of sync: '+name)
    records.append({'path':name,'sha256':hashlib.sha256(source.read_bytes()).hexdigest()})
ABI_HEADERS=['base.h','diskedit.h','partition.h','clipboard.h','netabi.h','system_palette.h','abi.h','vmabi.h','tlsabi.h','wordwaitabi.h','threadfinishabi.h']
for name in ABI_HEADERS:
    if a.capture:shutil.copy2(OS/'include'/name,ROOT/'native-abi'/name)
    if (ROOT/'native-abi'/name).read_bytes()!=(ROOT/'kernel-overlay/include'/name).read_bytes():
        raise SystemExit('Native ABI header out of sync: '+name)
if a.capture or a.apply:(ROOT/'test-results/source-sync.json').write_text(json.dumps(records,indent=2))
print('PASS: all '+str(len(FILES))+' OS integration sources and '+str(len(ABI_HEADERS))+' native ABI copies match the workspace overlay.')
