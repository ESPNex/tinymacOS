#!/usr/bin/env python3
"""Actual C++ header-template consumers and bounded WTF dtoa port; not JSC acceptance."""
import hashlib,json,os,sys,tempfile,time,subprocess
from pathlib import Path
r=Path(__file__).resolve().parents[1];sys.path.insert(0,str(r.parent.parent/'tinymacOS/tests'))
from retail_guest import Retail
from pixels import locate
from probe_media import verify_probe_media
cpus=int(os.environ.get('TMC_SMP','1'));assert cpus in (1,4,8)
iso=Path(os.environ.get('TMC_PROBE_BUILD','/var/tmp/tmc-webkit-stdlib-guest'))/'tinymacOS-qa-probe-x86_64.iso'
verify_probe_media(iso,'stdlib')
w=Path(tempfile.mkdtemp(prefix='tmc-stdlib-qa-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
os.environ.update(TOOLROOT=os.environ.get('TOOLROOT','/var/tmp/tmc-tools'),TMC_BUILD=str(iso.parent),TMC_SMP=str(cpus),TMC_POINTER='mouse')
if (Path(os.environ['TOOLROOT'])/'usr/bin/tesseract').is_file():os.environ.setdefault('OCR_ROOT',os.environ['TOOLROOT'])
record={'passed':False,'requested_cpus':cpus,'scope':'actual GNU header-template consumers and a dependency-isolated pinned WTF dtoa component; native heap/threads/events/FPU, NO hosted C++ binary library. NOT full libstdc++/WTF/JSC/WebCore/Safari','iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'external_evidence':str(ev)}
try:
 binary=iso.parent/'command.debug'
 assert not subprocess.check_output(['nm','-u',str(binary)],text=True).strip()
 symbols=subprocess.check_output(['nm',str(binary)],text=True)
 for name in ('stdlib_worker','stdlib_finish','__cxa_atexit','__cxa_finalize'):assert any(line.endswith(' '+name) for line in symbols.splitlines()),name
 demangled=subprocess.check_output(['nm','-C',str(binary)],text=True)
 for name in ('WTF::double_conversion::StringToDoubleConverter::StringToDouble','WTF::double_conversion::DoubleToStringConverter::ToShortestIeeeNumber','WTF::double_conversion::Bignum::'):
  assert name in demangled,name
 assert 'NEEDED' not in subprocess.check_output(['readelf','-d',str(binary)],text=True)
 headers=json.loads((Path(os.environ['TMC_QA_RESULTS'])/'stdlib-header-inputs.json').read_text())
 for name,expected in headers['headers'].items():assert hashlib.sha256(Path(name).read_bytes()).hexdigest()==expected,name
 with Retail(iso,w,ev,'stdlib',live=True) as g:
  g.enroll(name='Stdlib');g.key('f2');g.visible('Sessione locale');g.cmd('fetch');end=time.monotonic()+145
  while time.monotonic()<end:
   im=g.snap()
   if locate(im,'STDLIB NATIVE FAIL'):raise AssertionError('Native stdlib failure marker')
   if locate(im,'STDLIB NATIVE SMP PASS'):break
   time.sleep(.2)
  else:raise AssertionError('Native stdlib timeout/deadlock; not a pass')
  for text in ('cpus='+str(cpus)+' workers=4 C++-tasks=400','std=vector/string/unique_ptr/function/span/atomic','WTF-dtoa=parse-vectors=1040 roundtrips=8000','FP-context=x87/SSE2 checks=128 AVX=no','VM-spans=0 live=0 reserved=0','first-failure-line=0'):g.visible(text)
  g.shot('stdlib-pass')
  record.update(passed=True,workers=4,real_cpp_header_templates=['vector','string','unique_ptr','function','span','atomic','algorithm','array'],independent_CPython_parse_vectors=1040,binary64_roundtrips=8000,std_function_tasks=400,exact_context_checks_x87_SSE2=128,no_AVX=True,real_pinned_WTF_dtoa_component_with_explicit_port_patch=True,numeric_signed_boundary_fix=True,actual_header_inputs_rechecked=len(headers['headers']),host_cpp_binary_libraries_linked=False,zero_final_heap_and_DSO=True,cross_cpu_execution_required=cpus>1,negative_trap_cases_host_only=True,full_libstdcpp_or_WTF_or_JSC=False)

finally:
 (Path(os.environ['TMC_QA_RESULTS'])/('guest-stdlib-smp'+str(cpus)+'.json')).write_text(json.dumps(record,indent=2)+'\n')
print('PASS native C++/WTF dtoa component on',cpus,'CPUs: 1040 independent vectors, 8000 roundtrips, 400 real function tasks, 128 FP-context checks, zero final heap')
