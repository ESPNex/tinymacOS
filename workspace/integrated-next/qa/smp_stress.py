#!/usr/bin/env python3
import os,sys,time,json,tempfile
from pathlib import Path
sys.path.insert(0,'/home/user/tinymacOS/tests')
from retail_guest import Retail
from pixels import locate
w=Path(__file__).resolve().parents[1];cpus=int(os.environ.get('TMC_SMP','4'));e=w/'evidence'/f'smp-stress-{cpus}';e.mkdir(exist_ok=True)
iso=Path('/var/tmp/tmc-smp-stress/tinymacOS-2.4.4-x86_64.iso');run=Path(tempfile.mkdtemp(prefix='tmc-smp-stress-',dir='/var/tmp'))
os.environ.update(TOOLROOT='/var/tmp/tmc/tools/root',TMC_BUILD=str(iso.parent),TMC_SMP=str(cpus))
record={'passed':False,'requested_cpus':cpus,'scope':'Native ABI VM/uaccess/thread stress, not pthread/WebKit acceptance'}
try:
 with Retail(iso,run,e,'stress',live=True) as g:
  assert len(g.req('query-cpus-fast'))==cpus;g.enroll(name='Smp');g.key('f2');g.visible('Sessione locale');g.cmd('fetch');start=time.monotonic();end=start+180
  while time.monotonic()<end:
   im=g.snap()
   if locate(im,'SMP FAIL',faces=(0,1,2,3,5,6,8)):raise AssertionError(g.ocr())
   if locate(im,'VM SMP STRESS PASS',faces=(0,1,2,3,5,6,8)):break
   time.sleep(.3)
  else:
   g.req('stop');(e/'registers.txt').write_text(g.hmp('info registers -a'))
   (e/'cpus.txt').write_text(g.hmp('info cpus'))
   import subprocess
   sym={line.split()[2]:int(line.split()[0],16) for line in subprocess.check_output(['nm','/var/tmp/tmc-integrated-system/kernel.debug'],text=True).splitlines() if len(line.split())==3}
   for name,size in [('shoot_mailbox',64),('uservm_lock',8),('kmem_lock',8),('sched_lock',8),('cpus',512),('proc',60000),('task',600000)]:
    if name in sym:g.req('pmemsave',val=sym[name],size=size,filename=str(run/(name+'.bin')))
   (e/'debug-path.txt').write_text(str(run))
   raise AssertionError('SMP stress timed out; not a pass')
  g.shot('stress-pass');g.visible('uaccess-races=1600');g.visible('cpus='+str(cpus));record.update(passed=True,workers=4,worker_rounds=400,uaccess_buffer_syscalls=1600,elapsed_seconds=round(time.monotonic()-start,2),verified_markers=['VM SMP STRESS PASS','uaccess-races=1600','cpus='+str(cpus)])

finally:(e/'result.json').write_text(json.dumps(record,indent=2))
print('PASS: concurrent VM/legacy allocation/uaccess stress on',cpus,'vCPUs')
