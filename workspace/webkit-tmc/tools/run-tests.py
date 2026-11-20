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
args=ap.parse_args()
cases=[
 ('workspace-budget',['python3',str(root.parent/'check-snapshot-budget.py')]),
 ('source-sync',['python3',str(root/'tools/sync-system-sources.py'),'--check']),
 ('backend-contract',['sh',str(root/'tools/test-backend.sh')]),
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
 ]
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
(root/'test-results/milestone-tests.json').write_text(json.dumps({
 'scope':'backend/runtime contracts, target objects'+(' and real guest TCP/read-only-file, VM/MMU, private synchronization and per-thread errno probes' if args.guest else '')+'; NOT JavaScriptCore/WebCore/HTTPS/browser acceptance',
 'guest_requested':args.guest,'passed':all(r['exit_code']==0 for r in results),'cases':results},indent=2))
sys.exit(0 if all(r['exit_code']==0 for r in results) else 1)
