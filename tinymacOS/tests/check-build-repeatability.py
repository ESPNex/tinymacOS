#!/usr/bin/env python3
"""Compare two separately built artifact sets, including complete ISO bytes.
Does not equate reproducible output with functional or hardware acceptance.
"""
import argparse
import hashlib
import json
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('first', type=Path)
parser.add_argument('second', type=Path)
parser.add_argument('--report', type=Path, required=True)
a = parser.parse_args()
assert a.first.resolve() != a.second.resolve(), 'two distinct build directories required'
extensions = {'.elf', '.EFI', '.vol', '.bin', '.img', '.iso', '.sha'}
names = sorted({p.name for directory in (a.first, a.second) for p in directory.iterdir()
                if p.is_file() and (p.suffix in extensions or p.name == 'KernelCache')})
required = {'KernelCache', 'kernel.elf', 'RecoveryKernel.elf', 'BOOTX64.EFI', 'RECBOOT.EFI',
            'System-live.vol', 'System-disk.vol', 'System-recovery.vol'}
assert required.issubset(names) and any(n.endswith('.iso') for n in names), 'incomplete build set'
rows = []
for name in names:
    values = []
    for directory in (a.first, a.second):
        path = directory / name
        if path.is_file():
            with path.open('rb') as stream:
                values.append({'bytes': path.stat().st_size, 'sha256': hashlib.file_digest(stream, 'sha256').hexdigest()})
        else:
            values.append(None)
    rows.append({'name': name, 'first': values[0], 'second': values[1], 'identical': values[0] is not None and values[0] == values[1]})
report = {'passed': all(row['identical'] for row in rows), 'first': str(a.first), 'second': str(a.second),
          'same_toolchain_environment_only': True, 'cross_machine_reproducibility': 'NOT TESTED',
          'physical_hardware_tested': False, 'artifacts': rows}
a.report.parent.mkdir(parents=True, exist_ok=True)
a.report.write_text(json.dumps(report, indent=2) + '\n')
if not report['passed']:
    raise SystemExit('FAIL build repeatability: ' + ', '.join(row['name'] for row in rows if not row['identical']))
print(f'PASS two independent build directories: {len(rows)} artifacts byte-identical, including complete ISO; not cross-toolchain or hardware acceptance')
