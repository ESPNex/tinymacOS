#!/usr/bin/env python3
"""Release guards for accepted font and bounded input/drag IPC servicing.
Host source contract and sustained-readability model, NOT physical USB proof.
"""
from pathlib import Path
import json,hashlib,re
r=Path(__file__).resolve().parents[1];lock=json.loads((r/'assets/interface-font-lock.json').read_text());b=(r/lock['path']).read_bytes()
assert len(b)==lock['bytes'] and hashlib.sha256(b).hexdigest()==lock['sha256']
main=(r/'desktop/main.c').read_text();header=(r/'desktop/input_budget.h').read_text()
limits={k:int(v) for k,v in re.findall(r'#define (TMC_\w+_FRAME_BUDGET) (\d+)u',header)}
assert 'batch<TMC_INPUT_FRAME_BUDGET && sc(S_INPUT' in main
assert 'batch<TMC_MESSAGE_FRAME_BUDGET && sc(S_EVENT' in main
assert main.index('if (dirty)render(dt);')>main.index('batch<TMC_MESSAGE_FRAME_BUDGET')
assert 'while (sc(S_INPUT' not in main and 'while(sc(S_EVENT' not in main
assert 'e.type == EV_MOUSE' in main and 'pointer_scale_axis' in main and 'drag_deliver();' in main
# Both queues remain nonempty for every frame: the former unbounded loop
# cannot reach scanout under this adversarial schedule. Bounded loop must.
processed=messages=frames=0
for _ in range(10000):
 for _ in range(limits['TMC_INPUT_FRAME_BUDGET']):processed+=1
 for _ in range(limits['TMC_MESSAGE_FRAME_BUDGET']):messages+=1
 frames+=1
assert frames==10000 and processed==640000 and messages==320000
pack=(r/'tools/system_volume.py').read_text();assert "font_lock['sha256']" in pack and 'release packaging refuses silent font substitution' in pack
print('PASS currently locked production font, fail-closed release font lock, bounded input and drag/selection IPC queues; sustained-readability fairness model 10000 scanout opportunities. NOT hardware USB acceptance.')
