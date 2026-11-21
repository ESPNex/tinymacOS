#!/usr/bin/env python3
"""Native thread lifetime, TLS startup, TSD cleanup and C++ C-ABI consumer."""
import hashlib,json,os,sys,tempfile,time,subprocess
from pathlib import Path
r=Path(__file__).resolve().parents[1];sys.path.insert(0,str(r.parent.parent/'tinymacOS/tests'))
from retail_guest import Retail
from pixels import locate
cpus=int(os.environ.get('TMC_SMP','2'));assert cpus in (1,2,4,8)
iso=Path('/var/tmp/tmc-threads-guest/tinymacOS-10-x86_64.iso');w=Path(tempfile.mkdtemp(prefix='tmc-threads-qa-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
os.environ.update(TOOLROOT='/var/tmp/tmc/tools/root',TMC_BUILD=str(iso.parent),TMC_SMP=str(cpus),TMC_POINTER='mouse')
record={'passed':False,'requested_cpus':cpus,'scope':'native managed child startup/exit/join, caller-owned control blocks, generation TSD/destructors and limited freestanding C++ C-ABI/RAII; not full pthread, libc++, compiler TLS, WTF/JSC or browser','iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'external_evidence':str(ev)}
try:
 # The freestanding diagnostic really links without C++ library or ABI runtime.
 undefined=subprocess.check_output(['nm','-u',str(iso.parent/'command.debug')],text=True).strip()
 assert not undefined,undefined
 with Retail(iso,w,ev,'threads',live=True) as g:
  g.enroll(name='Threads');g.key('f2');g.visible('Sessione locale');g.cmd('fetch');end=time.monotonic()+145
  while time.monotonic()<end:
   im=g.snap()
   if locate(im,'THREAD RUNTIME FAIL'):raise AssertionError('Guest thread-runtime failure marker')
   if locate(im,'THREAD RUNTIME SMP PASS'):break
   time.sleep(.2)
  else:raise AssertionError('Guest thread runtime timeout/deadlock; not a pass')
  g.visible('base-created=90 cpus='+str(cpus)+' updates=320');g.visible('startup=TLS join=kernel-final-handoff');g.visible('TSD=generations dtors=4-bounded');g.visible('CXX=RAII/C-ABI VM-unmap=16');g.visible('first-failure-line=0');g.visible('failure=EAGAIN recovered');g.shot('threads-pass')
  record.update(passed=True,base_created_threads=90,additional_real_task_slot_exhaustion_children=True,failed_create_object_reset_and_subsequent_creation_recovered=True,initial_thread_control_block_reuse_after_leave=True,concurrent_updates=320,automatic_child_tcb_before_callback=True,unique_process_local_identity=True,exclusive_join=True,full_width_and_null_result_preserved=True,timeout_does_not_modify_result=True,timeout_retry=True,self_join_rejected=True,destructors_before_completion=True,tsd_generation_stale_key_rejected=True,tsd_capacity_failure=True,reentrant_key_creation_from_destructor=True,destructor_repopulation_two_and_four_passes=True,explicit_exit_and_return_cleanup=True,control_block_reused_after_join=64,actual_child_tcb_vm_unmap_after_join=16,initial_thread_leave_cleanup=True,freestanding_cpp_raii_and_native_c_abi=True,no_unresolved_executable_symbols=True,cross_cpu_execution_required=cpus>1)
finally:(r/'test-results'/('guest-threads-smp'+str(cpus)+'.json')).write_text(json.dumps(record,indent=2)+'\n')
print('PASS native managed thread runtime on',cpus,'CPUs: 90 base creations plus slot exhaustion, safe join/timeout, TSD destructors/generations, 16 TCB unmaps and freestanding C++ RAII')
