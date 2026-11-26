#!/usr/bin/env python3
"""Fail closed if cleanup removes active inputs or revives retired EFI runtime."""
from pathlib import Path
import json,hashlib
root=Path(__file__).resolve().parents[3];osroot=root/'tinymacOS';cleanup=root/'workspace/cleanup'
manifest=json.loads((cleanup/'protected-active-inputs.json').read_text())
for name,expected in manifest['unchanged_inputs'].items():
 p=osroot/name;assert p.is_file() and hashlib.sha256(p.read_bytes()).hexdigest()==expected,name
assert not (osroot/'recovery').exists(),'Retired firmware Recovery restored'
assert not (osroot/'boot/recovery_volume.c').exists()
assert hashlib.sha256((osroot/'desktop/recovery_background.h').read_bytes()).hexdigest()==manifest['relocated_background_sha256']
assert '#include "recovery_background.h"' in (osroot/'desktop/recovery.c').read_text()
for name in ('build.sh','tests/test-input-maintenance.sh'):
 text=(osroot/name).read_text();assert 'recovery/main.c' not in text and 'tests/recovery_usb.c' not in text and 'boot/recovery_volume.c' not in text,name
for name in ('fs/btrfs.c','fs/bootfs.c','fs/tmcbfs2_runtime.c','storage/fs2_convert.c','desktop/recovery.c','tests/native_recovery.py','tests/boot_recovery.py'):assert (osroot/name).is_file(),name
receipt=json.loads((cleanup/'github-archive-verification.json').read_text());assert receipt['passed'] and receipt['all_members_independently_verified_from_GitHub'] and receipt['files']==1945
assert all(p.name in ('webkit-tmc','local-10.2.5','cleanup') for p in (root/'workspace').iterdir() if p.is_dir()),'Historical workspace expanded again'
print('PASS cleanup: '+str(len(manifest['unchanged_inputs']))+' protected active inputs unchanged, background byte-equal, active FS/Recovery preserved, obsolete EFI implementation absent; archive prior verified member-by-member')
