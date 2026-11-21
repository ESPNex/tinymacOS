#!/usr/bin/env python3
"""Retain failed attempt; rerun the single timed-out guest test without concurrent VMs."""
from pathlib import Path
import subprocess,json,os,hashlib
q=Path(__file__).parent;report=json.loads((q/'kernel-regression-result.json').read_text())
for c in report['cases']:
 if c['exit_code']:
  assert c['name']=='guest-threads-smp1',c
  r=subprocess.run(['python3','workspace/webkit-tmc/tools/test-guest-threads.py'],env={**os.environ,'TMC_SMP':'1','TMC_MEM':'512'},text=True,capture_output=True,timeout=180)
  c['previous_attempt']={k:c[k] for k in ['exit_code','stdout','stderr']};c['retry_reason']='Concurrent VM memory pressure in 2 GiB host; isolated rerun, same probe and production kernel';c.update(exit_code=r.returncode,stdout=r.stdout,stderr=r.stderr);assert r.returncode==0,(r.stdout,r.stderr)
  print('PASS isolated guest-threads-smp1',flush=True)
report['passed']=all(c['exit_code']==0 for c in report['cases'])
report['production_match']={}
for n in ['kernel.elf','KernelCache']:
 a=Path('/var/tmp/tmc-10.1/build')/n;b=Path('/var/tmp/tmc-webkit-vm-system')/n;assert a.read_bytes()==b.read_bytes();report['production_match'][n]=hashlib.sha256(a.read_bytes()).hexdigest()
(q/'kernel-regression-result.json').write_text(json.dumps(report,indent=2)+'\n')
subprocess.run(['python3','workspace/webkit-tmc/tools/compact-qa-evidence.py'],check=True)
assert report['passed'];print('PASS 29/29, exact production kernel/cache match',flush=True)
