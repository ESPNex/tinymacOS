#!/usr/bin/env python3
"""Losslessly consolidate retired evidence, verifying every original byte before removal.
Usage: python3 workspace/archive-evidence.py workspace/old-release/evidence
Do not use on active QA directories. Restore with tar -xJf evidence.tar.xz.
"""
from pathlib import Path
import argparse,tarfile,hashlib,json,shutil
ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('directory',type=Path);args=ap.parse_args()
e=args.directory.resolve();root=Path('/home/user').resolve()
assert e.is_relative_to(root) and e.is_dir() and e.name=='evidence'
a=e.with_suffix('.tar.xz');assert not a.exists()
manifest={str(p.relative_to(e.parent)):hashlib.sha256(p.read_bytes()).hexdigest() for p in e.rglob('*') if p.is_file()}
assert manifest
with tarfile.open(a,'w:xz',preset=6) as t:t.add(e,arcname=e.name)
with tarfile.open(a) as t:
 for name,h in manifest.items():assert hashlib.sha256(t.extractfile(name).read()).hexdigest()==h,name
(e.parent/'evidence-archive.json').write_text(json.dumps({'archive':a.name,'lossless_verified':True,'sha256':hashlib.sha256(a.read_bytes()).hexdigest(),'files':manifest},indent=2)+'\n')
(e.parent/'EVIDENCE-ARCHIVE.md').write_text('# Evidenze consolidate senza perdita\n\nOriginali conservati in `'+a.name+'`; SHA-256 per file in `evidence-archive.json`. Ogni byte verificato prima del consolidamento per il budget persistente del workspace.\n\nRipristino dei percorsi originali da questa directory: `tar -xJf '+a.name+'`. Nessun sorgente o risultato storico rimosso.\n')
shutil.rmtree(e);print('Verified lossless archive:',a,len(manifest),'files,',a.stat().st_size,'bytes')
