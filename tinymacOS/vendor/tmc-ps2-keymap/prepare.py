#!/usr/bin/env python3
"""Install this experimental derived module in an EXTERNAL, pinned audk tree.
Does not change the production driver, ISO, or OpenCore vendor lock.
"""
import hashlib, json, subprocess, sys
from pathlib import Path
v = Path(__file__).resolve().parent
if len(sys.argv) != 2:
    raise SystemExit('usage: prepare.py /external/path/to/audk')
udk = Path(sys.argv[1]).resolve()
repo = v.parents[2]
if udk == repo or repo in udk.parents:
    raise SystemExit('audk must be outside the tinymacOS source tree')
lock = json.loads((v/'upstream-lock.json').read_text())
dst = udk/'MdeModulePkg/Bus/Isa/Ps2KeyboardDxe'
if not (udk/'MdePkg/MdePkg.dec').is_file() or not dst.is_dir():
    raise SystemExit('not an audk/edk2 tree')
# Refuse applying the patch to a different upstream driver. The pinned files
# are also retained in this repository for corresponding-source inspection.
for row in lock['files']:
    src = v/'upstream'/row['path']
    if hashlib.sha256(src.read_bytes()).hexdigest() != row['sha256']:
        raise SystemExit('upstream source hash mismatch: '+row['path'])
    target = dst/row['path']
    if target.read_bytes() != src.read_bytes():
        raise SystemExit('external driver is not pristine pinned source: '+str(target))
for row in lock['files']:
    (dst/row['path']).write_text((v/'upstream'/row['path']).read_text(), newline='\n')
subprocess.run(['patch','--batch','--fuzz=0','-p1','-i',str(v/'raw-set1.patch')],cwd=dst,check=True)
for name in ['TmcRawPs2.h','raw_ps2_queue.h']:
    (dst/name).write_bytes((v/name).read_bytes())
print('Prepared experimental firmware module; production artifacts untouched:',dst)
