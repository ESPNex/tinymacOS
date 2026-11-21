#!/usr/bin/env python3
"""Host regression: immutable upstream USB module derivation; never boots a guest."""
from pathlib import Path
import importlib.util,lzma,struct,hashlib
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('openduet_input',root/'tools/openduet-input.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
v=root/'vendor/opencore-1.0.8';full=(v/'bootX64').read_bytes();block=(v/'bootX64-blockio').read_bytes()
a,j=m.derive(full,block);b,k=m.derive(full,block)
assert a==b and j==k and m.sha(a)=='a226d3726b97315ca92d375d03723853c636fdede06135ef8d6f0b92b17e0ba7'
new,_,_,_=m.unpack_boot(a);old,_,_,_=m.unpack_boot(block);nf,_=m.ffs_files(new);of,_=m.ffs_files(old)
assert len(of)==29 and len(nf)==32 and set(nf)-set(of)=={g for g,_ in m.MODULES.values()}
for g,f in of.items():assert nf[g]==f
assert j['upstream_module_code_unchanged'] and j['installed_disk_firmware_unchanged']
# The three earlier EFILDR executable payloads and their directory entries remain identical.
_,fv_off,_,_=m.unpack_boot(block)
assert a[m.PREFIX+16:m.PREFIX+16+3*64]==block[m.PREFIX+16:m.PREFIX+16+3*64]
assert a[m.PREFIX+272:m.PREFIX+fv_off]==block[m.PREFIX+272:m.PREFIX+fv_off]
print('PASS optical firmware: deterministic derivation, 29 original FFS and 3 earlier executables byte-exact, only 3 pinned USB modules added')
for label,bad in [('truncated',block[:400]),('bad paging',block[:-1]+bytes([block[-1]^1])),('bad signature',block[:m.PREFIX]+b'NOPE'+block[m.PREFIX+4:])]:
 try:m.derive(full,bad)
 except (ValueError,lzma.LZMAError,struct.error):pass
 else:raise AssertionError(label)
print('PASS fail-closed on truncated bootstrap, invalid EFILDR signature and corrupted startup page tables')
# Corrupt only the vendor USB module data, maintaining valid container metadata;
# the fixed per-module digest, not just an outer image signature, must reject it.
fv,off,_,_=m.unpack_boot(full);src,end=m.ffs_files(fv);guid=next(iter(m.MODULES.values()))[0];file=src[guid];at=fv.index(file);bad=bytearray(fv);bad[at+100]^=1
compressed=bytearray(lzma.compress(bad,format=lzma.FORMAT_ALONE,preset=7));compressed[5:13]=struct.pack('<Q',len(bad))
e=bytearray(full[m.PREFIX:m.PREFIX+off])+compressed;struct.pack_into('<I',e,8,len(e));struct.pack_into('<I',e,16+3*64+8,len(compressed))
cr3=0x20000+((len(e)+8192+4095)&~4095);prefix=bytearray(full[:m.PREFIX]);struct.pack_into('<I',prefix,153,cr3);blob=prefix+e;blob+=bytes(cr3-0x20000-512-len(blob));blob+=m.tables(cr3)
try:m.derive(blob,block)
except ValueError as exc:assert 'digest/type' in str(exc)
else:raise AssertionError('tampered vendor module accepted')
assert (v/'bootX64').read_bytes()==full and (v/'bootX64-blockio').read_bytes()==block
print('PASS pinned FFS digest rejects modified vendor module; both original vendor images remain unchanged')
