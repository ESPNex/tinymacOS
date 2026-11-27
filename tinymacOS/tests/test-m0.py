#!/usr/bin/env python3
"""M0 source/wording contract. Does not certify runtime layout or hardware."""
from pathlib import Path
import hashlib
import json
import runpy
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
policy = json.loads((ROOT / 'tests/fixtures/m0-contract.json').read_text())
module = runpy.run_path(str(ROOT / 'tools/audit-ui-strings.py'))
lex = module['literals']
# No false positives from comments, character constants or include names;
# preserve Unicode, escaped C strings, line numbers and adjacent literal parts.
fixture = '''#include "private.h"
// "not a string"
/* "not a string either" */
char quote = '"';
const char *s = "Caffè\\n" "\\u00e0"; // "ignored"
'''
assert [(line, text) for line, raw, text, ctx in lex(fixture)] == [(5, 'Caffè\n'), (5, 'à')]
for name, digest in policy['protected_files'].items():
    path = ROOT / name
    assert path.is_file() and hashlib.sha256(path.read_bytes()).hexdigest() == digest, name


def check_wording(read):
    for name, messages in policy['required_safety_text'].items():
        source = read(name)
        for text in messages:
            assert text in source, (name, 'required safety text missing', text)
    for row in policy['wording_changes']:
        source = read(row['file'])
        assert row['before'] not in source, (row['file'], 'old wording returned')
        assert row['after'] in source, (row['file'], 'replacement missing')


read = lambda name: (ROOT / name).read_text()
check_wording(read)
# Mutation self-test: the checker MUST reject removal of a destructive warning.
warning = policy['required_safety_text']['userspace/installer.c'][0]
try:
    check_wording(lambda name: read(name).replace(warning, '') if name == 'userspace/installer.c' else read(name))
except AssertionError:
    pass
else:
    raise AssertionError('Safety gate accepted a missing destructive warning')

# Compatibility spellings remain command aliases; not a claim of APFS format.
command = read('userspace/command.c')
for alias in ('fsck_apfs', 'mount_apfs', 'newfs_apfs'):
    assert f'IS("{alias}")' in command
partition = read('include/partition.h')
assert '"tinymacOS (nativo)"' in partition and '"BTRFS"' in partition
for path in ('userspace/about.c', 'userspace/settings.c', 'desktop/recovery.c'):
    assert '10.2.6' in read(path), ('public version changed before release gates', path)

# Optional workspace check: these are NOT additional OS build dependencies.
if '--workspace' in sys.argv:
    for name, digest in policy['webkit_source_resources'].items():
        path = ROOT.parent / name
        assert path.is_file() and hashlib.sha256(path.read_bytes()).hexdigest() == digest, name
    subprocess.run([sys.executable, str(ROOT.parent / 'workspace/webkit-tmc/tools/sync-system-sources.py'), '--check'], check=True)
subprocess.run([sys.executable, str(ROOT / 'tools/check-usb-retirement.py')], check=True)
subprocess.run([sys.executable, str(ROOT / 'tools/audit-ui-strings.py'), '--check'], check=True)
print(f"PASS M0: lexer + safety mutation test, {len(policy['protected_files'])} locked font/asset/vendor/USB inputs, truthful native FS labels and preserved public version; no hardware claim")
