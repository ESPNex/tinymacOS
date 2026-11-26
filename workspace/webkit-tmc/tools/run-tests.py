#!/usr/bin/env python3
"""Run backend/runtime milestone tests, NOT WebKit/browser acceptance.
--guest also builds a disposable diagnostic image and boots 1/2-vCPU profiles.
It NEVER uploads an ISO. All authored sources must already be saved.
"""
import argparse,json,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser(description=__doc__)
ap.add_argument('--guest',action='store_true')
ap.add_argument('--events-megablock',action='store_true',help='Fresh clock/events + full prior native runtime matrix, 1/4/8 CPUs; external --output required')
ap.add_argument('--runtime-megablock',action='store_true',help='Fresh base plus heap/unanchored-C++/lifecycle/pthread native 1/4/8 CPU matrix; external --output required')
ap.add_argument('--lifecycle-guest',action='store_true',help='Fresh lifecycle base/build plus native 1/4/8 CPU tests; requires external --output')
ap.add_argument('--output',type=Path,default=root/'test-results/milestone-tests.json')
args=ap.parse_args()
cases=[
 ('workspace-budget',['python3',str(root.parent/'check-snapshot-budget.py')]),
 ('source-sync',['python3',str(root/'tools/sync-system-sources.py'),'--check']),
 ('backend-contract',['sh',str(root/'tools/test-backend.sh')]),
 ('time-contract',['sh',str(root/'tools/test-time.sh')]),
 ('events-contract',['sh',str(root/'tools/test-events.sh')]),
 ('events-upstream-requirements',['python3',str(root/'tools/audit-events-upstream.py')]),
 ('pthread-contract',['sh',str(root/'tools/test-pthread.sh')]),
 ('cpp-lifecycle-contract',['sh',str(root/'tools/test-cpp-lifecycle.sh')]),
 ('cpp-runtime-contract',['sh',str(root/'tools/test-cpp-runtime.sh')]),
 ('heap-contract',['sh',str(root/'tools/test-heap.sh')]),
 ('threads-contract',['sh',str(root/'tools/test-threads.sh')]),
 ('sync-contract',['sh',str(root/'tools/test-sync.sh')]),
 ('vm-contract',['python3',str(root/'tools/test-vm.py')]),
 ('descriptor-contract',['sh',str(root/'tools/test-descriptors.sh')]),
 ('kernel-stream',['python3',str(root/'tools/test-kernel-stream.py')]),
 ('system-appearance',['sh',str(root/'tools/test-appearance.sh')]),
 ('backend-target-compile',['sh',str(root/'tools/compile-backend.sh')]),
 ('system-target-compile',['sh',str(root/'tools/compile-system-integration.sh')]),
]
if args.guest:
 cases += [
  ('guest-vm-probe-build',['bash',str(root/'tools/build-guest-vm-probe.sh')]),
  ('guest-probe-build',['env','TMC_PROBE_BASE=/var/tmp/tmc-webkit-vm-system','bash',str(root/'tools/build-guest-network-probe.sh')]),
  ('guest-runtime-smp1',['env','TMC_SMP=1','python3',str(root/'tools/test-guest-network.py')]),
  ('guest-runtime-smp2',['env','TMC_SMP=2','python3',str(root/'tools/test-guest-network.py')]),
  ('guest-vm-smp1',['env','TMC_SMP=1','python3',str(root/'tools/test-guest-vm.py')]),
  ('guest-vm-smp2',['env','TMC_SMP=2','python3',str(root/'tools/test-guest-vm.py')]),
  ('guest-sync-probe-build',['env','TMC_PROBE_KIND=sync','TMC_PROBE_BASE=/var/tmp/tmc-webkit-vm-system','TMC_PROBE_BUILD=/var/tmp/tmc-sync-guest','bash',str(root/'tools/build-guest-network-probe.sh')]),
  ('guest-sync-smp1',['env','TMC_SMP=1','python3',str(root/'tools/test-guest-sync.py')]),
  ('guest-sync-smp4',['env','TMC_SMP=4','python3',str(root/'tools/test-guest-sync.py')]),
  ('guest-sync-smp8',['env','TMC_SMP=8','python3',str(root/'tools/test-guest-sync.py')]),
  ('guest-tls-probe-build',['env','TMC_PROBE_KIND=tls','TMC_PROBE_BASE=/var/tmp/tmc-webkit-vm-system','TMC_PROBE_BUILD=/var/tmp/tmc-tls-guest','bash',str(root/'tools/build-guest-network-probe.sh')]),
  ('guest-tls-smp1',['env','TMC_SMP=1','python3',str(root/'tools/test-guest-tls.py')]),
  ('guest-tls-smp4',['env','TMC_SMP=4','python3',str(root/'tools/test-guest-tls.py')]),
  ('guest-tls-smp8',['env','TMC_SMP=8','python3',str(root/'tools/test-guest-tls.py')]),
  ('guest-threads-probe-build',['env','TMC_PROBE_KIND=threads','TMC_PROBE_BASE=/var/tmp/tmc-webkit-vm-system','TMC_PROBE_BUILD=/var/tmp/tmc-threads-guest','bash',str(root/'tools/build-guest-network-probe.sh')]),
  ('guest-threads-smp1',['env','TMC_SMP=1','python3',str(root/'tools/test-guest-threads.py')]),
  ('guest-threads-smp4',['env','TMC_SMP=4','python3',str(root/'tools/test-guest-threads.py')]),
  ('guest-threads-smp8',['env','TMC_SMP=8','python3',str(root/'tools/test-guest-threads.py')]),
  ('guest-heap-probe-build',['env','TMC_PROBE_KIND=heap','TMC_PROBE_BASE=/var/tmp/tmc-webkit-vm-system','TMC_PROBE_BUILD=/var/tmp/tmc-webkit-heap-guest','bash',str(root/'tools/build-guest-network-probe.sh')]),
  ('guest-heap-smp1',['env','TMC_SMP=1','python3',str(root/'tools/test-guest-heap.py')]),
  ('guest-heap-smp4',['env','TMC_SMP=4','python3',str(root/'tools/test-guest-heap.py')]),
  ('guest-heap-smp8',['env','TMC_SMP=8','python3',str(root/'tools/test-guest-heap.py')]),
  ('guest-cpp-probe-build',['bash',str(root/'tools/build-guest-cpp-probe.sh')]),
  ('guest-cpp-smp1',['env','TMC_SMP=1','TMC_PRODUCTION_BUILD=/var/tmp/tmc-webkit-10.3-system','python3',str(root/'tools/test-guest-cpp.py')]),
  ('guest-cpp-smp4',['env','TMC_SMP=4','TMC_PRODUCTION_BUILD=/var/tmp/tmc-webkit-10.3-system','python3',str(root/'tools/test-guest-cpp.py')]),
  ('guest-cpp-smp8',['env','TMC_SMP=8','TMC_PRODUCTION_BUILD=/var/tmp/tmc-webkit-10.3-system','python3',str(root/'tools/test-guest-cpp.py')]),
 ]
if args.lifecycle_guest:
 if Path('/var/tmp') not in args.output.resolve().parents:ap.error('--lifecycle-guest requires --output under /var/tmp')
 args.output.parent.mkdir(parents=True,exist_ok=True)
 env=['env','TMC_QA_RESULTS='+str(args.output.resolve().parent),'TMC_PRODUCTION_BUILD=/var/tmp/tmc-webkit-lifecycle-system','TMC_PROBE_BUILD=/var/tmp/tmc-webkit-lifecycle-guest']
 cases += [('guest-lifecycle-probe-build',env+['bash',str(root/'tools/build-guest-lifecycle-probe.sh')])]
 cases += [('guest-lifecycle-smp'+str(n),env+['TMC_SMP='+str(n),'python3',str(root/'tools/test-guest-lifecycle.py')]) for n in (1,4,8)]
if args.runtime_megablock or args.events_megablock:
 if args.guest or args.lifecycle_guest or (args.runtime_megablock and args.events_megablock):ap.error('--runtime-megablock is a standalone full matrix')
 if Path('/var/tmp') not in args.output.resolve().parents:ap.error('--runtime-megablock requires --output under /var/tmp')
 args.output.parent.mkdir(parents=True,exist_ok=True)
 buildroot='/var/tmp/tmc-events-final' if args.events_megablock else '/var/tmp/tmc-megablock-final'
 base=buildroot+'/system'
 common=['env','TMC_QA_RESULTS='+str(args.output.resolve().parent),'TMC_PRODUCTION_BUILD='+base]
 # Lifecycle builder always rebuilds the base from its pre-build hash manifest.
 cases += [('guest-lifecycle-probe-build',common+['TMC_LIFECYCLE_SYSTEM_BUILD='+base,'TMC_PROBE_BUILD='+buildroot+'/lifecycle','bash',str(root/'tools/build-guest-lifecycle-probe.sh')])]
 for kind in (('events','heap','cpp','pthread') if args.events_megablock else ('heap','cpp','pthread')):
  cases += [('guest-'+kind+'-probe-build',common+['TMC_PROBE_BASE='+base,'TMC_PROBE_KIND='+kind,'TMC_PROBE_BUILD='+buildroot+'/'+kind,'bash',str(root/'tools/build-guest-network-probe.sh')])]
 for kind in (('events','heap','cpp','lifecycle','pthread') if args.events_megablock else ('heap','cpp','lifecycle','pthread')):
  cases += [('guest-'+kind+'-smp'+str(n),common+['TMC_SMP='+str(n),'TMC_PROBE_BUILD='+buildroot+'/'+kind,'python3',str(root/('tools/test-guest-'+kind+'.py'))]) for n in (1,4,8)]
results=[]
for name,cmd in cases:
 try:
  r=subprocess.run(cmd,text=True,capture_output=True,timeout=150 if name.startswith('guest-') else 90)
  results.append({'name':name,'exit_code':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
 except subprocess.TimeoutExpired:
  results.append({'name':name,'exit_code':124,'stdout':'','stderr':'Test exceeded its deadline; this is NOT a pass.'})
 print(('PASS' if results[-1]['exit_code']==0 else 'FAIL')+': '+name,flush=True)
 # Never boot stale probe artifacts if the build failed.
 if name.endswith('probe-build') and results[-1]['exit_code']!=0:break
# Save complete original frames externally, then retain lossless terminal crops
# in the small shared workspace. Does not affect a guest assertion or its result.
if args.guest:subprocess.run(['python3',str(root/'tools/compact-qa-evidence.py')],check=True)
args.output.write_text(json.dumps({
 'scope':'backend/runtime contracts, target objects'+(' and real guest TCP/read-only-file, VM/MMU, private synchronization, per-thread errno, managed thread/TSD and freestanding C++ consumer probes' if args.guest else '')+(' and real native C++ lifecycle .init_array/DSO finalization on 1/4/8 CPUs' if args.lifecycle_guest else '')+(' and fresh native heap/cache/unanchored-C++/lifecycle/pthread matrix on 1/4/8 CPUs' if args.runtime_megablock else '')+(' and native MONOTONIC clock/condition/task-timer queue plus prior runtime 1/4/8 CPU matrix' if args.events_megablock else '')+'; NOT JavaScriptCore/WebCore/HTTPS/browser acceptance',
 'events_megablock_requested':args.events_megablock,'runtime_megablock_requested':args.runtime_megablock,'guest_requested':args.guest,'lifecycle_guest_requested':args.lifecycle_guest,'passed':all(r['exit_code']==0 for r in results),'cases':results},indent=2))
sys.exit(0 if all(r['exit_code']==0 for r in results) else 1)
