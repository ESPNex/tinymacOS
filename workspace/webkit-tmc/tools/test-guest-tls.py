#!/usr/bin/env python3
"""Real native FS/thread-pointer and errno-cell isolation, not libc/pthread/JSC."""
import hashlib,json,os,sys,tempfile,time
from pathlib import Path
r=Path(__file__).resolve().parents[1];sys.path.insert(0,str(r.parent.parent/'tinymacOS/tests'))
from retail_guest import Retail
from pixels import locate
cpus=int(os.environ.get('TMC_SMP','2'));assert cpus in (1,2,4,8)
iso=Path('/var/tmp/tmc-tls-guest/tinymacOS-10-x86_64.iso');w=Path(tempfile.mkdtemp(prefix='tmc-tls-qa-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
os.environ.update(TOOLROOT='/var/tmp/tmc/tools/root',TMC_BUILD=str(iso.parent),TMC_SMP=str(cpus),TMC_POINTER='mouse')
record={'passed':False,'requested_cpus':cpus,'scope':'native FS + caller-owned TCB + real target errno macro in VM/descriptor adapters, not pthread/C++ ELF TLS/JSC/HTTPS','iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'external_evidence':str(ev)}
try:
 with Retail(iso,w,ev,'tls',live=True) as g:
  g.enroll(name='Tls');g.key('f2');g.visible('Sessione locale');g.cmd('fetch');end=time.monotonic()+130
  while time.monotonic()<end:
   im=g.snap()
   if locate(im,'TLS FS FAIL'):raise AssertionError('Guest TLS failure marker')
   if locate(im,'TLS FS SMP PASS'):break
   time.sleep(.2)
  else:raise AssertionError('Guest TLS marker timeout; not a pass')
  g.visible('checks=600 workers=4 cpus='+str(cpus));g.visible('errno-per-thread=isolated');g.visible('adapter-errno=thread-local');g.shot('tls-pass');record.update(passed=True,workers=4,post_yield_isolation_checks=600,invalid_base_rejected=True,unset_thread_starts_with_zero_fs=True,child_does_not_inherit_parent_tcb=True,main_errno_preserved=True,thread_cleanup_clears_fs=True,target_errno_macro_and_adapter_errors_isolated=True,global_errno_symbol_absent=True,cross_cpu_execution_required=cpus>1)
finally:(r/'test-results'/('guest-tls-smp'+str(cpus)+'.json')).write_text(json.dumps(record,indent=2)+'\n')
print('PASS real guest native FS/TLS isolation on',cpus,'vCPUs; 4 workers, 600 post-yield checks')
