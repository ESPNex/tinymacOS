#!/usr/bin/env python3
"""Lossless terminal crops for ONLY active backend network/VM probe evidence.
Never touches frozen releases, desktop/recovery visual QA or input attachments.
Full originals are copied externally first; existing Git history also retains
previous full-frame captures. No claim that external scratch persists snapshots.
"""
import hashlib,json,shutil,tempfile
from pathlib import Path
from PIL import Image
r=Path(__file__).resolve().parents[1];base=r/'test-results';manifest=base/'terminal-crops.json'
old=json.loads(manifest.read_text()) if manifest.exists() else {}
external=None;count=saved=0
for pattern in ['guest-vm-smp[12]/*.png','guest-network-smp[12]/*.png']:
 for p in sorted(base.glob(pattern)):
  with Image.open(p) as image:
   if image.size!=(1280,800):continue
   data=p.read_bytes();before=hashlib.sha256(data).hexdigest()
   if external is None:external=Path(tempfile.mkdtemp(prefix='tmc-qa-full-frames-',dir='/var/tmp'))
   original=external/p.parent.name/p.name;original.parent.mkdir(exist_ok=True);shutil.copyfile(p,original)
   # The actual fixed retail Terminal frame and all text; no resize, repaint,
   # color quantization, annotation or synthetic pixel content.
   image.crop((297,160,1030,630)).save(p,optimize=True)
  name=str(p.relative_to(base));entry=old.get(name,{})
  ref=entry.get('original_github_commit') if entry.get('full_frame_sha256')==before else None
  old[name]={'full_frame_sha256':before,'crop_sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'crop_box':[297,160,1030,630],'external_full_frame':str(original),'original_github_commit':ref}
  saved+=len(data)-p.stat().st_size;count+=1
if count:manifest.write_text(json.dumps(old,indent=2)+'\n')
print('Lossless terminal evidence crops:',count,'frames;',saved,'bytes workspace margin recovered. Full-frame scratch:',external)
