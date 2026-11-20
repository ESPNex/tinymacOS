#!/usr/bin/env python3
"""Portable source distribution of Riviera. Never bundles a CD, account or personal disk."""
import sys,zipfile
from media_path import external
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=external(sys.argv[1]);out.parent.mkdir(parents=True,exist_ok=True);dirs='arch assets boot crypto desktop docs drivers fs include kernel licenses recovery storage tests tools userspace'.split();files=[root/n for n in ('README.md','build.sh','run.sh')]
for n in dirs:
 for p in sorted((root/n).rglob('*')):
    if p.is_file() and not set(p.parts)&{'__pycache__','storico','evidence'} and p.suffix not in ('.pyc','.orig'):
        assert p.suffix.lower() not in ('.iso','.img','.fd','.deb','.tgz','.o','.elf','.efi')
        assert p.suffix.lower()!='.zip'
        files.append(p)
# Include the textual matching QA record, not many megabytes of screenshots.
for p in sorted((root/'tests/evidence/riviera-2.3.0').glob('*.json')):files.append(p)
with zipfile.ZipFile(out,'w') as z:
 for p in files:
    rel=p.relative_to(root).as_posix();i=zipfile.ZipInfo('tinymacOS/'+rel,(2025,1,1,0,0,0));i.create_system=3;i.external_attr=(0o100755 if p.stat().st_mode&0o111 else 0o100644)<<16;i.compress_type=zipfile.ZIP_DEFLATED;z.writestr(i,p.read_bytes(),compresslevel=9)
with zipfile.ZipFile(out) as z:assert z.testzip() is None and 'tinymacOS/crypto/pbkdf.c' in z.namelist()
print('Source-only ZIP:',len(files),'files;',out.stat().st_size,'bytes')
