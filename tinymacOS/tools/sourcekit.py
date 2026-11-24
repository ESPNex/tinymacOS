#!/usr/bin/env python3
"""Matching authored and Linux sources with pinned firmware dependencies. Never bundles a CD, account or personal disk."""
import sys,zipfile
from media_path import external
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=external(sys.argv[1]);out.parent.mkdir(parents=True,exist_ok=True);dirs='arch assets boot compat crypto desktop docs drivers fs include kernel licenses net recovery storage tests tools userspace vendor'.split();files=[root/n for n in ('README.md','build.sh','run.sh')]
for n in dirs:
 for p in sorted((root/n).rglob('*')):
    if p.is_file() and not set(p.parts)&{'__pycache__','storico','evidence'} and p.suffix not in ('.pyc','.orig'):
        assert p.suffix.lower() not in ('.iso','.img','.fd','.deb','.tgz','.o','.elf')
        assert p.suffix.lower()!='.efi' or p.relative_to(root).parts[0]=='vendor'
        assert p.suffix.lower()!='.zip'
        files.append(p)
# Include the textual matching QA record, not many megabytes of screenshots.
for p in sorted((root/'tests/evidence/riviera-2.3.0').glob('*.json')):files.append(p)
with zipfile.ZipFile(out,'w') as z:
 for p in files:
    rel=p.relative_to(root).as_posix();i=zipfile.ZipInfo('tinymacOS/'+rel,(2025,1,1,0,0,0));i.create_system=3;i.external_attr=(0o100755 if p.stat().st_mode&0o111 else 0o100644)<<16;i.compress_type=zipfile.ZIP_DEFLATED;z.writestr(i,p.read_bytes(),compresslevel=9)
with zipfile.ZipFile(out) as z:assert z.testzip() is None and all(n in z.namelist() for n in ['tinymacOS/crypto/pbkdf.c','tinymacOS/compat/linux/xhci_runtime.c','tinymacOS/vendor/linux-v6.12/xhci.c','tinymacOS/vendor/linux-v6.12/manifest.json'])
print('Matching-source/dependency ZIP:',len(files),'files;',out.stat().st_size,'bytes')
