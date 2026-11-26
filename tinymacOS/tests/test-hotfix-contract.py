#!/usr/bin/env python3
"""Exact mouse compatibility rollback source contract, not hardware acceptance.
Deliberately retires the former queue-fairness assertion: it did not fix the PC.
"""
from pathlib import Path
import hashlib,json
r=Path(__file__).resolve().parents[1]
lock=json.loads((r/'assets/interface-font-lock.json').read_text());b=(r/lock['path']).read_bytes()
assert len(b)==lock['bytes'] and hashlib.sha256(b).hexdigest()==lock['sha256']
assert 'export TMC_FS2_BACKEND=1' in (r/'build.sh').read_text()
receipt=json.loads((r.parent/'workspace/local-10.2.5/hotfix/mouse-1024-rollback-candidate.json').read_text())
assert receipt['baseline_commit']=='1a4ca9af4c4e7bb42cc819e7f931da34f6a4af69'
for name,entry in receipt['files'].items():
 assert hashlib.sha256((r/name).read_bytes()).hexdigest()==entry['restored_1024_sha256'],name
main=(r/'desktop/main.c').read_text()
assert 'filedrag' not in main and 'drag_deliver' not in main
assert 'while (sc(S_INPUT' in main and 'pointer_scale_axis' in main
assert not (r/'desktop/input_budget.h').exists()
pack=(r/'tools/system_volume.py').read_text()
assert "font_lock['sha256']" in pack and 'release packaging refuses silent font substitution' in pack
print('PASS local compatibility-source contract: exact 10.2.4 desktop and input queue, file DnD disabled, font lock and FS2 retained. NOT a physical mouse fix or new ISO acceptance.')
