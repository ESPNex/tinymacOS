#!/usr/bin/env python3
"""Validate pinned upstream files and complete-function extractions before build.
Does not fetch code or assert that a fragment is a complete Linux HCD port.
"""
import hashlib,json
from pathlib import Path
root=Path(__file__).resolve().parents[1];vendor=root/'vendor/linux-v6.12'
lock=json.loads((vendor/'manifest.json').read_text())
for row in lock['files']:
    p=root.parent/row['path']
    if hashlib.sha256(p.read_bytes()).hexdigest()!=row['sha256']:
        raise SystemExit('Upstream source mismatch: '+str(p))
for row in lock.get('extractions',[]):
    data=(vendor/row['source']).read_bytes()
    start=data.index(row['start_marker'].encode())
    end=data.index(row['end_marker'].encode(),start)
    expected=data[start:end]
    if expected!=(vendor/row['output']).read_bytes() or hashlib.sha256(expected).hexdigest()!=row['sha256']:
        raise SystemExit('Upstream function extraction mismatch: '+row['function'])
print('PASS pinned Linux sources and complete function extractions (not a full HCD claim)')

for manifest in ('usbcore-manifest.json','usbhid-manifest.json'):
    rows=json.loads((vendor/manifest).read_text())['files']
    for row in rows:
        p=root.parent/row['path']
        if hashlib.sha256(p.read_bytes()).hexdigest()!=row['sha256']:
            raise SystemExit('Canonical upstream mismatch: '+str(p))
    print('PASS canonical upstream pins:',manifest,len(rows))
