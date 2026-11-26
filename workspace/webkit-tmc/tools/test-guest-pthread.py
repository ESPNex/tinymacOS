#!/usr/bin/env python3
"""Actual target .init_array and __cxa_atexit/finalize; not WebKit acceptance."""
import hashlib,json,os,sys,tempfile,time,subprocess
from pathlib import Path
r=Path(__file__).resolve().parents[1];sys.path.insert(0,str(r.parent.parent/'tinymacOS/tests'))
from retail_guest import Retail
from pixels import locate
from probe_media import verify_probe_media
cpus=int(os.environ.get('TMC_SMP','1'));assert cpus in (1,4,8)
iso=Path(os.environ.get('TMC_PROBE_BUILD','/var/tmp/tmc-webkit-pthread-guest'))/'tinymacOS-qa-probe-x86_64.iso'
verify_probe_media(iso,'pthread')
w=Path(tempfile.mkdtemp(prefix='tmc-pthread-qa-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
os.environ.update(TOOLROOT=os.environ.get('TOOLROOT','/var/tmp/tmc-tools'),TMC_BUILD=str(iso.parent),TMC_SMP=str(cpus),TMC_POINTER='mouse')
if (Path(os.environ['TOOLROOT'])/'usr/bin/tesseract').is_file():os.environ.setdefault('OCR_ROOT',os.environ['TOOLROOT'])
record={'passed':False,'requested_cpus':cpus,'scope':'opt-in source-level pthread subset integrated with native heap and C++ RAII/guard/DSO lifecycle. NOT binary pthread ABI, full POSIX, detach/cancel, realtime timedwait, compiler TLS, WTF/JSC/WebCore or Safari','iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'external_evidence':str(ev)}
try:
 binary=iso.parent/'command.debug'
 assert not subprocess.check_output(['nm','-u',str(binary)],text=True).strip()
 symbols=subprocess.check_output(['nm',str(binary)],text=True)
 for name in ('tmc_pthread_create','tmc_pthread_join','tmc_pthread_once','__cxa_atexit','__cxa_finalize'):assert any(line.endswith(' '+name) for line in symbols.splitlines()),name
 assert not any(line.split()[-1].startswith('pthread_') and line.split()[-1] not in ('pthread_cases_run','pthread_cpp_touch','pthread_cpp_finish','pthread_test_pause','pthread_test_ticks') for line in symbols.splitlines() if len(line.split())==3 and line.split()[1]=='T'),'unexpected unprefixed pthread ABI'
 with Retail(iso,w,ev,'pthread',live=True) as g:
  g.enroll(name='Pthread');g.key('f2');g.visible('Sessione locale');g.cmd('fetch');end=time.monotonic()+145
  while time.monotonic()<end:
   im=g.snap()
   if locate(im,'PTHREAD NATIVE FAIL'):raise AssertionError('Native pthread failure marker')
   if locate(im,'PTHREAD NATIVE SMP PASS'):break
   time.sleep(.2)
  else:raise AssertionError('Native pthread timeout/deadlock; not a pass')
  for text in ('cpus='+str(cpus)+' workers=4 updates=2000','C++=RAII/local-static/new/DSO once=1 TSD-dtors=8','join=race/stale/reuse capacity=exhaust/recover','mutex=owner/recursive cond=predicate/timeout','VM-spans=0 live=0 reserved=0','first-failure-line=0'):g.visible(text)
  g.shot('pthread-pass')
  record.update(passed=True,workers=4,protected_updates=2000,TSD_destructor_calls=8,cpp_RAII_localstatic_new_DSO=True,condition_predicate_and_reacquired_timeout=True,once_recursive_rejection_and_publication=True,errorcheck_recursive_owner_mutexes=True,child_pthread_exit=True,stale_generation_handles_and_concurrent_join=True,sequential_thread_reuses=16,actual_native_task_exhaustion_and_recovery=True,zero_final_heap_spans=True,cross_cpu_execution_required=cpus>1,allocation_failure_injection_host_only=True,no_unresolved_executable_symbols=True,source_compatibility_only=True)

finally:
 (Path(os.environ['TMC_QA_RESULTS'])/('guest-pthread-smp'+str(cpus)+'.json')).write_text(json.dumps(record,indent=2)+'\n')
print('PASS native pthread subset on',cpus,'CPUs: C/C++ consumers, 2000 updates, owner mutex/condition/once/TSD/join and capacity recovery, zero final heap')
