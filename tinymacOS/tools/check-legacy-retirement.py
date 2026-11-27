#!/usr/bin/env python3
"""TMCFS2-only source/binary guard. Historical docs/git are not rewritten."""
from pathlib import Path
import re, subprocess, sys
root=Path(__file__).resolve().parents[1]
retired=['fs/btrfs.c','storage/native_cow.c','storage/native_resize.c','include/nativecow.h','include/nativefs.h','crypto/vault.c']
for name in retired:
    assert not (root/name).exists(), 'Retired implementation returned: '+name
for folder in ['fs','storage','kernel','crypto','include']:
    for p in (root/folder).rglob('*'):
        if p.suffix not in ('.c','.h'): continue
        s=p.read_text()
        assert not re.search(r'\b(native_geometry\w*|native_cow_\w*|native_resize|tiny_check|fs_stat_i|fs_create_i|vault_probe|vault_enable)\s*\(',s), p
        if p != root/'storage/partition.c':
            assert 'TMCBFS1' not in s and 'TINYFS1' not in s, p
p=(root/'include/partition.h').read_text()
assert 'return f==FS_TMC2;' in p and 'non supportato' in p
assert 'return FS_UNSUPPORTED_NATIVE;' in (root/'storage/partition.c').read_text()
assert 'TMC_FS2_BACKEND' not in (root/'fs/namespace.c').read_text()
if len(sys.argv)>1:
    build=Path(sys.argv[1])
    for name in ['kernel.debug','recovery-kernel.debug']:
        symbols=subprocess.check_output(['nm',str(build/name)],text=True)
        assert not re.search(r'\b(native_geometry\w*|native_cow_\w*|native_resize|tiny_check|fs_stat_i|fs_create_i|vault_probe|vault_enable)$',symbols,re.M), name
print('PASS TMCFS2-only retirement: old driver/formatter/CoW/resize/FV002 removed; old magic confined to unsupported-format detection; no backend fallback'+('; both native kernels checked' if len(sys.argv)>1 else ''))
