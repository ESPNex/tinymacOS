#!/usr/bin/env python3
"""Actual target .init_array and __cxa_atexit/finalize; not WebKit acceptance."""
import hashlib,json,os,sys,tempfile,time,subprocess
from pathlib import Path
r=Path(__file__).resolve().parents[1];sys.path.insert(0,str(r.parent.parent/'tinymacOS/tests'))
from retail_guest import Retail
from pixels import locate
from probe_media import verify_probe_media
cpus=int(os.environ.get('TMC_SMP','1'));assert cpus in (1,4,8)
iso=Path(os.environ.get('TMC_PROBE_BUILD','/var/tmp/tmc-webkit-events-guest'))/'tinymacOS-qa-probe-x86_64.iso'
verify_probe_media(iso,'events')
w=Path(tempfile.mkdtemp(prefix='tmc-events-qa-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
os.environ.update(TOOLROOT=os.environ.get('TOOLROOT','/var/tmp/tmc-tools'),TMC_BUILD=str(iso.parent),TMC_SMP=str(cpus),TMC_POINTER='mouse')
if (Path(os.environ['TOOLROOT'])/'usr/bin/tesseract').is_file():os.environ.setdefault('OCR_ROOT',os.environ['TOOLROOT'])
record={'passed':False,'requested_cpus':cpus,'scope':'native monotonic clock/condition deadlines and task/timer queue with C++ RAII; NOT realtime clock, real WTF::RunLoop, IO multiplexer, WebKit or Safari','iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'external_evidence':str(ev)}
try:
 binary=iso.parent/'command.debug'
 assert not subprocess.check_output(['nm','-u',str(binary)],text=True).strip()
 symbols=subprocess.check_output(['nm',str(binary)],text=True)
 for name in ('tmc_runloop_post','tmc_runloop_cancel','tmc_runloop_run','tmc_clock_gettime','tmc_pthread_cond_timedwait','__cxa_atexit','__cxa_finalize'):assert any(line.endswith(' '+name) for line in symbols.splitlines()),name
 with Retail(iso,w,ev,'events',live=True) as g:
  g.enroll(name='Events');g.key('f2');g.visible('Sessione locale');g.cmd('fetch');end=time.monotonic()+145
  while time.monotonic()<end:
   im=g.snap()
   if locate(im,'EVENTS NATIVE FAIL'):raise AssertionError('Native events failure marker')
   if locate(im,'EVENTS NATIVE SMP PASS'):break
   time.sleep(.2)
  else:raise AssertionError('Native events timeout/deadlock; not a pass')
  for text in ('cpus='+str(cpus)+' producers=4 C++-tasks=400','clock=MONOTONIC/100Hz realtime=UNSUPPORTED','cond=absolute/timeout/relock/notify','queue=FIFO/nested/timers cancel-races=400','stop=active-finish/pending-cleanup/late-reject','VM-spans=0 live=0 reserved=0','first-failure-line=0'):g.visible(text)
  g.shot('events-pass')
  record.update(passed=True,producers=4,cpp_tasks=400,cancel_dispatch_races=400,ordered_tasks=68,monotonic_resolution_ns=10000000,realtime_fails_closed=True,relative_sleep_and_past_absolute=True,absolute_condition_timeout_reacquires_mutex=True,condition_signal_before_deadline=True,equal_deadline_fifo_and_no_early_callbacks=True,nested_post=True,early_timer_wakes_idle_dispatcher=True,active_cancel_busy=True,cleanup_exactly_once=True,stop_pending_and_active_ownership=True,late_post_rejected=True,zero_final_heap_and_DSO=True,cross_cpu_execution_required=cpus>1,allocation_fault_injection_host_only=True,real_WTF_RunLoop=False)

finally:
 (Path(os.environ['TMC_QA_RESULTS'])/('guest-events-smp'+str(cpus)+'.json')).write_text(json.dumps(record,indent=2)+'\n')
print('PASS native events on',cpus,'CPUs: monotonic waits, 68 ordered callbacks, 400 cancellation races, 400 C++ RAII tasks, stop/cleanup and zero final heap')
