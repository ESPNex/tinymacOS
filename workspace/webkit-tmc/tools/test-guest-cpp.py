#!/usr/bin/env python3
"""Native C++ allocation/local-static ABI, not libc++ or WebKit acceptance."""
import hashlib,json,os,sys,tempfile,time,subprocess
from pathlib import Path
r=Path(__file__).resolve().parents[1];sys.path.insert(0,str(r.parent.parent/'tinymacOS/tests'))
from retail_guest import Retail
from pixels import locate
cpus=int(os.environ.get('TMC_SMP','1'));assert cpus in (1,2,4,8)
iso=Path(os.environ.get('TMC_PROBE_BUILD','/var/tmp/tmc-webkit-cpp-guest'))/'tinymacOS-qa-probe-x86_64.iso'
from probe_media import verify_probe_media
verify_probe_media(iso,'cpp')
w=Path(tempfile.mkdtemp(prefix='tmc-cpp-qa-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
os.environ.update(TOOLROOT=os.environ.get('TOOLROOT','/var/tmp/tmc-tools'),TMC_BUILD=str(iso.parent),TMC_SMP=str(cpus),TMC_POINTER='mouse')
if (Path(os.environ['TOOLROOT'])/'usr/bin/tesseract').is_file():os.environ.setdefault('OCR_ROOT',os.environ['TOOLROOT'])
record={'passed':False,'requested_cpus':cpus,'scope':'native no-exceptions new/delete and x86-64 Itanium local-static guards; NOT a complete C++ runtime, STL, compiler TLS, exceptions, WTF/JSC/WebCore or Safari UI','iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'external_evidence':str(ev)}
try:
 binary=iso.parent/'command.debug'
 undefined=subprocess.check_output(['nm','-u',str(binary)],text=True).strip();assert not undefined,undefined
 symbols=subprocess.check_output(['nm','-C',str(binary)],text=True).splitlines()
 new_count=sum(' T operator new' in line for line in symbols);delete_count=sum(' T operator delete' in line for line in symbols)
 assert (new_count,delete_count)==(8,12),(new_count,delete_count)
 for name in ('__cxa_guard_acquire','__cxa_guard_release','__cxa_guard_abort'):assert any(line.endswith(' T '+name) for line in symbols),name
 with Retail(iso,w,ev,'cpp',live=True) as g:
  g.enroll(name='Cpp');g.key('f2');g.visible('Sessione locale');g.cmd('fetch');end=time.monotonic()+145;captured=False
  while time.monotonic()<end:
   im=g.snap()
   if not captured and time.monotonic()>end-130:
    (w/'cpu-registers.txt').write_text(g.hmp('info registers -a'))
    print('CPP CPU register snapshot: '+str(w/'cpu-registers.txt'),flush=True)
    captured=True
   if locate(im,'CPP NATIVE FAIL'):raise AssertionError('Native C++ failure marker')
   if locate(im,'CPP NATIVE SMP PASS'):break
   time.sleep(.2)
  else:raise AssertionError('Native C++ timeout/deadlock; not a pass')
  g.visible('cpus='+str(cpus)+' workers=4 destructors=12000');g.visible('handler=OOM-recovery');g.visible('guards=once/nested/contended abort=retry');g.visible('VM-spans=0 live=0 reserved=0');g.visible('first-failure-line=0');g.shot('cpp-pass')
  record.update(passed=True,global_new_overloads=new_count,global_delete_overloads=delete_count,concurrent_workers=4,nontrivial_array_destructors=12000,cross_thread_cpp_deletion=True,new_handler_real_vm_slot_exhaustion_and_recovery=True,once_only_nested_static_initialization=True,guard_abort_retry_under_contention=True,release_acquire_object_publication=True,zero_final_vm_reservations=True,no_unresolved_executable_symbols=True,no_target_cpp_standard_library_linked=True,cross_cpu_execution_required=cpus>1,negative_sigill_cases_host_only=True)
finally:(Path(os.environ.get('TMC_QA_RESULTS',str(r/'test-results')))/('guest-cpp-smp'+str(cpus)+'.json')).write_text(json.dumps(record,indent=2)+'\n')
print('PASS native C++ on',cpus,'CPUs: four workers, 12000 destructors, real OOM handler recovery, nested contended guards, abort/retry and zero final reservations')
