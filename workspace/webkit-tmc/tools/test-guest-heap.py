#!/usr/bin/env python3
"""Actual native heap: production kernel, native VM/locks/TCBs, no host shim."""
import hashlib,json,os,sys,tempfile,time,subprocess
from pathlib import Path
r=Path(__file__).resolve().parents[1];sys.path.insert(0,str(r.parent.parent/'tinymacOS/tests'))
from retail_guest import Retail
from pixels import locate
cpus=int(os.environ.get('TMC_SMP','1'));assert cpus in (1,2,4,8)
iso=Path(os.environ.get('TMC_PROBE_BUILD','/var/tmp/tmc-webkit-heap-guest'))/'tinymacOS-qa-probe-x86_64.iso'
from probe_media import verify_probe_media
verify_probe_media(iso,'heap')
w=Path(tempfile.mkdtemp(prefix='tmc-heap-qa-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
os.environ.update(TOOLROOT=os.environ.get('TOOLROOT','/var/tmp/tmc-tools'),TMC_BUILD=str(iso.parent),TMC_SMP=str(cpus),TMC_POINTER='mouse')
if (Path(os.environ['TOOLROOT'])/'usr/bin/tesseract').is_file():os.environ.setdefault('OCR_ROOT',os.environ['TOOLROOT'])
record={'passed':False,'requested_cpus':cpus,'scope':'native VM-backed allocator and C allocation exports with a freestanding C++ RAII consumer; NOT full libc/libc++, bmalloc, WTF/JSC/WebCore or Safari','iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'external_evidence':str(ev)}
try:
 undefined=subprocess.check_output(['nm','-u',str(iso.parent/'command.debug')],text=True).strip();assert not undefined,undefined
 with Retail(iso,w,ev,'heap',live=True) as g:
  g.enroll(name='Heap');g.key('f2');g.visible('Sessione locale');g.cmd('fetch');end=time.monotonic()+145
  while time.monotonic()<end:
   im=g.snap()
   if locate(im,'HEAP NATIVE FAIL'):raise AssertionError('Native heap failure marker')
   if locate(im,'HEAP NATIVE SMP PASS'):break
   time.sleep(.2)
  else:raise AssertionError('Native heap timeout/deadlock; not a pass')
  g.visible('cache-reuse=2000 reserve-delta=0 trim=live-safe');g.visible('cpus='+str(cpus)+' workers=4 random-ops=9000');g.visible('libc=malloc/calloc/realloc/free/aligned_alloc');g.visible('CXX=RAII/C-allocation ABI');g.visible('VM-spans=0 live=0 reserved=0');g.visible('first-failure-line=0');g.shot('heap-pass')
  record.update(passed=True,empty_span_cache_reuses=2000,cache_reuse_reserve_delta=0,live_object_trim_safe=True,randomized_operations=9000,concurrent_workers=4,cross_thread_free=True,zero_final_vm_reservations=True,native_libc_allocation_symbols=True,freestanding_cpp_raii=True,no_unresolved_executable_symbols=True,alignment_up_to_2MiB=True,calloc_zero_and_overflow=True,failed_realloc_preserves_object=True,split_coalesce_and_in_place_realloc=True,cross_cpu_execution_required=cpus>1)
finally:(Path(os.environ.get('TMC_QA_RESULTS',str(r/'test-results')))/('guest-heap-smp'+str(cpus)+'.json')).write_text(json.dumps(record,indent=2)+'\n')
print('PASS native heap on',cpus,'CPUs: 9000 operations, four workers, C allocation ABI, C++ RAII, zero final reservations')
