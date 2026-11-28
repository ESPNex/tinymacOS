#!/usr/bin/env python3
"""Explicit M8 bulk-only exception; the historical M0 hashes remain immutable.
No arbitrary ignores or blanket regeneration. Exactly five integration files,
new bulk/class sources, and independently pinned unchanged input functions.
"""
from pathlib import Path
import hashlib,json,re
ROOT=Path(__file__).resolve().parents[1]
ALLOWED=frozenset(('compat/linux/ehci_runtime.c','compat/linux/xhci_runtime.c','compat/linux/usbcore_bridge.c','compat/linux/usbcore_stack.c','tools/build-usbcore.sh'))
def sha(data):return hashlib.sha256(data).hexdigest()
def function(source,name):
    m=re.search(r'\b'+re.escape(name)+r'\([^;{}]*\)\s*\{',source)
    assert m,('missing unchanged input definition',name)
    i=m.end();depth=1
    while depth:
        if source[i]=='{':depth+=1
        elif source[i]=='}':depth-=1
        i+=1
    return source[m.start():i]
def verify(policy,read=None):
    read=read or (lambda n:(ROOT/n).read_bytes())
    manifest=json.loads((ROOT/'tests/fixtures/usb8-bulk-extension.json').read_text())
    rows=manifest['modified_protected_files'];assert set(rows)==ALLOWED
    assert set(manifest['new_production_files'])=={'compat/linux/bulk_runtime.inc','compat/linux/usbcore_storage.c'}
    for name,row in rows.items():
        assert policy['protected_files'][name]==row['original_sha256'],('M0 baseline must not be refreshed',name)
        data=read(name);assert sha(data)==row['reviewed_bulk_sha256'],('unreviewed bulk extension',name)
        for symbol,digest in row['unchanged_input_functions'].items():
            assert sha(function(data.decode(),symbol).encode())==digest,('input/HID function changed',name,symbol)
    for name,digest in manifest['new_production_files'].items():
        assert sha(read(name))==digest,('unreviewed bulk/class source',name)
    return ALLOWED
if __name__=='__main__':
    policy=json.loads((ROOT/'tests/fixtures/m0-contract.json').read_text());verify(policy)
    # Mutation test: adding ANY code to a reviewed adapter must be rejected.
    target='compat/linux/xhci_runtime.c'
    try:verify(policy,lambda n:(ROOT/n).read_bytes()+(b'\n/* mutation */' if n==target else b''))
    except AssertionError:pass
    else:raise AssertionError('M8 gate accepted an unreviewed adapter mutation')
    print('PASS explicit M8 bulk extension: five narrow integration exceptions, original M0 hashes preserved, pinned input functions unchanged, new bulk/class sources and mutation rejection')
