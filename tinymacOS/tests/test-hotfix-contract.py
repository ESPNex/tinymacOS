#!/usr/bin/env python3
"""10.2.6 source contract: exclusive canonical USB, restored DnD, safe PMBR."""
from pathlib import Path
import hashlib,json
r=Path(__file__).resolve().parents[1]
lock=json.loads((r/'assets/interface-font-lock.json').read_text());b=(r/lock['path']).read_bytes()
assert len(b)==lock['bytes'] and hashlib.sha256(b).hexdigest()==lock['sha256']
retired=json.loads((r.parent/'workspace/local-10.2.6/retired-native-usb.json').read_text())
for row in retired['removed']:assert not (r/row['path']).exists(),row['path']
build=(r/'build.sh').read_text();assert 'export TMC_FS2_BACKEND=1' in build
assert 'bash tools/build-usbcore.sh' in build and 'if [[ "${TMC_USB_CANONICAL' not in build
assert 'python3 tools/check-usb-profile.py "$D"' in build
main=(r/'desktop/main.c').read_text();assert 'drag_deliver' in main and 'EV_DRAG_READY' in main
assert 'TMC_INPUT_FRAME_BUDGET' in main and (r/'desktop/input_budget.h').exists()
assert 'scanout_refresh' in main and 'scanout_heartbeat=s.ticks;dirty=1' not in main
assert 'window_commands_equal' in (r/'kernel/syscall.c').read_text()
assert 'pmbr_repair|=2' in (r/'storage/partition.c').read_text()
print('PASS 10.2.6 sources: retired USB facade absent, canonical unconditional, DnD/event budgets restored, font/FS2 and consent-only PMBR checks retained')
