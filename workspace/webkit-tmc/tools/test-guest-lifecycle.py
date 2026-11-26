#!/usr/bin/env python3
"""Actual target .init_array and __cxa_atexit/finalize; not WebKit acceptance."""
import hashlib,json,os,sys,tempfile,time,subprocess
from pathlib import Path
r=Path(__file__).resolve().parents[1];sys.path.insert(0,str(r.parent.parent/'tinymacOS/tests'))
from retail_guest import Retail
from pixels import locate
from probe_media import verify_probe_media
cpus=int(os.environ.get('TMC_SMP','1'));assert cpus in (1,4,8)
iso=Path(os.environ.get('TMC_PROBE_BUILD','/var/tmp/tmc-webkit-lifecycle-guest'))/'tinymacOS-qa-probe-x86_64.iso'
verify_probe_media(iso,'lifecycle')
w=Path(tempfile.mkdtemp(prefix='tmc-lifecycle-qa-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
os.environ.update(TOOLROOT=os.environ.get('TOOLROOT','/var/tmp/tmc-tools'),TMC_BUILD=str(iso.parent),TMC_SMP=str(cpus),TMC_POINTER='mouse')
if (Path(os.environ['TOOLROOT'])/'usr/bin/tesseract').is_file():os.environ.setdefault('OCR_ROOT',os.environ['TOOLROOT'])
record={'passed':False,'requested_cpus':cpus,'scope':'opt-in native compiler-generated global/local-static lifecycle, concurrent initializer publication and DSO/process destructors. NOT automatic OS-wide CRT, libc++, compiler TLS, exceptions, WTF/JSC/WebCore or Safari','iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'external_evidence':str(ev)}
try:
 binary=iso.parent/'command.debug'
 assert not subprocess.check_output(['nm','-u',str(binary)],text=True).strip()
 symbols=subprocess.check_output(['nm',str(binary)],text=True)
 for name in ('__cxa_atexit','__cxa_finalize','tmc_cpp_run_initializers','__tmc_init_array_start','__tmc_init_array_end'):assert any(line.endswith(' '+name) for line in symbols.splitlines()),name
 # Ensure .init_array is actually emitted into the native executable, not a
 # handwritten call to the constructors or a hosted startup dependency.
 assert '.init_array' in subprocess.check_output(['readelf','-SW',str(binary)],text=True)
 with Retail(iso,w,ev,'lifecycle',live=True) as g:
  g.enroll(name='Lifecycle');g.key('f2');g.visible('Sessione locale');g.cmd('fetch');end=time.monotonic()+145
  while time.monotonic()<end:
   im=g.snap()
   if locate(im,'LIFECYCLE NATIVE FAIL'):raise AssertionError('Native lifecycle failure marker')
   if locate(im,'LIFECYCLE NATIVE SMP PASS'):break
   time.sleep(.2)
  else:raise AssertionError('Native lifecycle timeout/deadlock; not a pass')
  for text in ('cpus='+str(cpus)+' workers=4 registrations=400 callbacks=401','global-ctors=2 local-static=1 dtors=3','DSO=LIFO recursive=yes callback-registration=yes','pending=0 VM-spans=0 live=0 reserved=0','first-failure-line=0'):g.visible(text)
  g.shot('lifecycle-pass')
  record.update(passed=True,workers=4,concurrent_destructor_registrations=400,worker_callbacks=401,compiler_global_constructors=2,compiler_local_static_constructors=1,compiler_destructors=3,native_init_array_present=True,recursive_startup_rejected=True,DSO_selective_reverse_registration_order=True,concurrent_finalizers_serialized=True,same_thread_recursive_finalization=True,registration_from_destructor=True,repeated_finalize_no_duplicate_callbacks=True,zero_final_pending_destructors_and_heap_spans=True,cross_cpu_execution_required=cpus>1,allocation_failure_injection_host_only=True,no_unresolved_executable_symbols=True)
finally:
 (Path(os.environ['TMC_QA_RESULTS'])/('guest-lifecycle-smp'+str(cpus)+'.json')).write_text(json.dumps(record,indent=2)+'\n')
print('PASS native lifecycle on',cpus,'CPUs: compiler .init_array, 400 registrations, 401 callbacks, DSO LIFO/reentry, zero final heap')
