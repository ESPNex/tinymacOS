"""Structural lock/producer contract; complements executable host/guest tests."""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
cpu=(r/'arch/cpu.c').read_text();sys=(r/'kernel/syscall.c').read_text();mm=(r/'kernel/memory.c').read_text()
assert cpu.count('cpu_account_tick();')==2
for block in cpu.split('cpu_account_tick();')[:-1]:assert block.rfind('spin_lock(&sched_lock')>block.rfind('spin_unlock(&sched_lock')
info=sys[sys.index('static void info('):sys.index('static i64 alloc(')]
assert 'mem_info(s);' in info
lock=info.index('spin_lock(&sched_lock');unlock=info.index('spin_unlock(&sched_lock')
for f in ['s->core_ticks','s->core_busy','s->busy_ticks']:
 assert lock<info.index(f)<unlock
assert 'z[r].sample_ticks = cpu_samples;' in sys and 'z[r].resident_bytes = vm_resident(q->root);' in sys
assert 'if(cur!=idle_slots[c->cpu])' in (r/'kernel/sched.c').read_text()
assert 's->free=freepages*PAGE' in mm and 's->user_bytes=userpages*PAGE' in mm
assert 'cpu_online_mask' in info
print('PASS M3 producer contract: LAPIC and PIT account under scheduler lock; CPU snapshots use same lock; allocator snapshot and process resident provenance intact')
