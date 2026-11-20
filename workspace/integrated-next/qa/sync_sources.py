#!/usr/bin/env python3
"""Refresh authored development mirrors BEFORE builds. Does not freeze a release."""
from pathlib import Path
import shutil,hashlib,json
home=Path(__file__).resolve().parents[3];r=home/'tinymacOS';w=Path(__file__).resolve().parents[1];ov=home/'workspace/webkit-tmc/kernel-overlay'
for p in ov.rglob('*'):
 if p.is_file() and (r/p.relative_to(ov)).is_file():shutil.copyfile(r/p.relative_to(ov),p)
for name in ['abi.h','vmabi.h','tlsabi.h']:shutil.copyfile(r/'include'/name,home/'workspace/webkit-tmc/native-abi'/name)
before=json.loads((w/'baseline-hashes.json').read_text());records=[]
for p in sorted(r.rglob('*')):
 if p.is_file() and '__pycache__' not in p.parts:
  rel=str(p.relative_to(r));digest=hashlib.sha256(p.read_bytes()).hexdigest()
  if before.get(rel)!=digest:
   if not rel.startswith('vendor/'):
    q=w/'sources'/rel;q.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,q)
   records.append({'path':rel,'sha256':digest})
(w/'source-sync.json').write_text(json.dumps(records,indent=2)+'\n')
print('Source mirrors synchronized:',len(records),'changed files; vendor pinned in primary source tree.')
