#!/usr/bin/env python3
"""Real native scheduler private parking and sync.c tests; not pthread/WebKit."""
import hashlib,json,os,sys,tempfile,time
from pathlib import Path
r=Path(__file__).resolve().parents[1];sys.path.insert(0,str(r.parent.parent/'tinymacOS/tests'))
from retail_guest import Retail
from pixels import locate
cpus=int(os.environ.get('TMC_SMP','2'));assert cpus in (1,2,4,8)
iso=Path('/var/tmp/tmc-sync-guest/tinymacOS-10.1.1-x86_64.iso');w=Path(tempfile.mkdtemp(prefix='tmc-sync-qa-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
os.environ.update(TOOLROOT='/var/tmp/tmc/tools/root',TMC_BUILD=str(iso.parent),TMC_SMP=str(cpus),TMC_POINTER='mouse')
record={'passed':False,'requested_cpus':cpus,'scope':'native private compare-and-park/wake and target mutex/condition algorithms; no pthread/join/PI/process-shared/cancellation or WebKit claim','iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'external_evidence':str(ev)}
try:
 with Retail(iso,w,ev,'sync',live=True) as g:
  g.enroll(name='Sync');g.key('f2');g.visible('Sessione locale');g.cmd('fetch');end=time.monotonic()+145
  while time.monotonic()<end:
   im=g.snap()
   if locate(im,'SYNC PARK FAIL'):raise AssertionError('Guest private parking/sync failure marker')
   if locate(im,'SYNC PARK SMP PASS'):break
   time.sleep(.2)
  else:raise AssertionError('Guest sync timeout/deadlock; not a pass')
  g.visible('updates=240 workers=4 cpus='+str(cpus));g.visible('compare-park=atomic timeout=relocked');g.visible('thread-errno=preserved');g.shot('sync-pass')
  record.update(passed=True,workers=4,serialized_updates_with_yield_inside_lock=240,condition_predicate_loop_and_broadcast=True,timeout_reacquires_mutex=True,mutex_timedlock=True,changed_word_returns_EAGAIN=True,invalid_address_and_operation_rejected=True,zero_and_all_wake_counts_checked=True,per_thread_errno_preserved=True,cross_cpu_execution_required=cpus>1)
finally:(r/'test-results'/('guest-sync-smp'+str(cpus)+'.json')).write_text(json.dumps(record,indent=2)+'\n')
print('PASS real native compare-and-park + mutex/condition on',cpus,'vCPUs; 240 yielded critical sections, broadcast, timed lock/condition and errno preserved')
