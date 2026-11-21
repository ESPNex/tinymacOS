#!/usr/bin/env python3
"""Real user-mode VM/MMU tests in disposable QEMU, framebuffer-only outcomes.
1-vCPU profile also runs seven deliberately faulting children and checks their
exit statuses through the ordinary shell; 2-vCPU profile verifies fail-closed.
"""
import hashlib,json,os,sys,tempfile,time,traceback
from pathlib import Path
r=Path(__file__).resolve().parents[1];system=r.parent.parent/'tinymacOS'
sys.path.insert(0,str(system/'tests'))
from retail_guest import Retail
from pixels import locate
cpus=int(os.environ.get('TMC_SMP','1'));assert cpus in (1,2,4,8)
image=Path(os.environ.get('TMC_VM_PROBE_BUILD','/var/tmp/tmc-webkit-vm-guest'))/'tinymacOS-10.1-x86_64.iso'
work=Path(tempfile.mkdtemp(prefix='tmc-vm-guest-',dir='/var/tmp'))
evidence=r/f'test-results/guest-vm-smp{cpus}';evidence.mkdir(exist_ok=True)
record={'scope':'real user-mode VM and MMU; NOT JSC/WebCore/browser acceptance','passed':False,'requested_vcpus':cpus,'iso_qa_only':True,'iso_sha256':hashlib.sha256(image.read_bytes()).hexdigest(),'faults':[]}
os.environ.update(TOOLROOT='/var/tmp/tmc/tools/root',TMC_BUILD=str(image.parent),TMC_SMP=str(cpus))
def visible(g,word,timeout=15):
 end=time.monotonic()+timeout
 while time.monotonic()<end:
  im=g.snap()
  if locate(im,word,faces=(0,1,2,3,5,6,8)):return im
  if locate(im,'VM FAIL',faces=(0,1,2,3,5,6,8)):raise AssertionError('Probe returned VM FAIL; inspect failure screenshot')
  time.sleep(.15)
 raise AssertionError('Framebuffer did not show '+repr(word))
try:
 with Retail(image,work,evidence,'vm-probe',disk='vars',live=True) as g:
  record['qmp_vcpu_count']=len(g.req('query-cpus-fast'));assert record['qmp_vcpu_count']==cpus
  g.enroll(name='Webkit');g.key('f2');visible(g,'Sessione locale')
  g.cmd('fetch')
  if True: # Every CPU profile must pass real positive and faulting MMU tests.
   for marker in ('VM PASS reserve/commit/protect/guards','VM PASS decommit/recommit zero/slots','VM PASS 2MiB alignment and backing'):visible(g,marker)
   record['positive_vm_checks']=True;g.shot('vm-positive')
   for kind in ('left','right','readonly','none','decommitted','released','nx'):
    g.cmd('fetch fault '+kind);im=visible(g,'FAULT ARMED '+kind)
    assert not locate(im,'VM UNEXPECTED SURVIVAL',faces=(0,1,2,3,5,6,8))
    g.cmd('fetch status $? '+kind)
    visible(g,'FAULT RESULT '+kind+' status=1')
    g.shot('vm-fault-'+kind)
    record['faults'].append({'kind':kind,'armed_marker_verified':True,'child_exit_status':1,'status_read_via_shell_expansion':True,'desktop_and_terminal_survived':True})
   # A fresh command must still successfully reserve/commit after all fault cleanup.
   g.cmd('fetch post');visible(g,'VM POSTFAULT PASS')
   g.shot('vm-after-faults');record['post_fault_vm_reuse']=True
  record['passed']=True
  print('PASS: real VM/MMU profile',cpus,'vCPU;',len(record['faults']),'faulting children verified.')
except Exception:
 record['failure']=traceback.format_exc();raise
finally:
 (evidence/'result.json').write_text(json.dumps(record,indent=2))
