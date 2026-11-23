#!/usr/bin/env python3
"""Rebuild disposable probes around the current production kernel; never deploy.
Full screenshots stay external so tests cannot overflow the 128 MB workspace.
"""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import hashlib,json,os,subprocess,tempfile
q=Path(__file__).resolve().parent;home=q.parents[1];root=home/'tinymacOS';tools=home/'workspace/webkit-tmc/tools'
base=Path('/var/tmp/tmc-10.1.7/linux-config-v10/build');evidence=Path(tempfile.mkdtemp(prefix='tmc-config-v10-kernel-',dir='/var/tmp'))
def sha(p):return hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
def save(name,data):(q/name).write_text(json.dumps(data,indent=2)+'\n')
source={str(p.relative_to(root)):sha(p) for p in sorted(root.rglob('*')) if p.is_file() and p.suffix in ('.c','.h','.S','.sh','.py','.ld') and '__pycache__' not in p.parts}
(evidence/'vm-system-source-hashes.json').write_text(json.dumps(source,indent=2)+'\n')
env={**os.environ,'TOOLROOT':'/var/tmp/tmc/tools/root','OCR_ROOT':'/var/tmp/tmc/tools/root','TMC_MEM':'512','TMC_PRODUCTION_BUILD':str(base),'TMC_PROBE_BASE':str(base),'TMC_QA_RESULTS':str(evidence)}
scope='Current production kernel: host/runtime/target contracts and actual guest TCP, MMU faults, synchronization, TLS/errno and managed-thread/C++ probes; NOT browser or Linux driver-port completion'
host=json.loads((tools.parent/'test-results/milestone-tests.json').read_text());assert host['passed'] and not host['guest_requested']
cases=[{'name':r['name'],'exit_code':r['exit_code']} for r in host['cases']];media={};guests=[]
report={'passed':False,'scope':scope,'production_build':str(base),'kernelcache_sha256':sha(base/'KernelCache'),'kernel_sha256':sha(base/'kernel.elf'),'external_evidence':str(evidence),'full_linux_input_port':False,'physical_hardware_tested':False}
try:
 for kind,cpus,output in [('network',[1,2],'/var/tmp/tmc-webkit-network-guest'),('vm',[1,2],'/var/tmp/tmc-webkit-vm-guest'),('sync',[1,4,8],'/var/tmp/tmc-sync-guest'),('tls',[1,4,8],'/var/tmp/tmc-tls-guest'),('threads',[1,4,8],'/var/tmp/tmc-threads-guest')]:
  current={**env,'TMC_PROBE_KIND':kind,'TMC_PROBE_BUILD':output,'TMC_VM_PROBE_BUILD':output}
  log=evidence/(kind+'-build.log')
  with log.open('w') as f:result=subprocess.run(['bash',str(tools/'build-guest-network-probe.sh')],env=current,stdout=f,stderr=subprocess.STDOUT,timeout=150)
  cases.append({'name':kind+'-probe-build','exit_code':result.returncode,'external_log':str(log),'log_sha256':sha(log)})
  assert result.returncode==0,(kind,log)
  def run(cpu):
   log=evidence/(kind+'-smp'+str(cpu)+'.log')
   with log.open('w') as f:p=subprocess.run(['python3',str(tools/('test-guest-'+kind+'.py'))],env={**current,'TMC_SMP':str(cpu)},stdout=f,stderr=subprocess.STDOUT,timeout=220)
   if kind in ['network','vm']:receipt=evidence/f'guest-{kind}-smp{cpu}'/'result.json'
   else:receipt=evidence/f'guest-{kind}-smp{cpu}.json'
   r=json.loads(receipt.read_text()) if receipt.exists() else {'passed':False}
   save(f'kernel-guest-{kind}-smp{cpu}.json',r)
   assert p.returncode==0 and r.get('passed'),(kind,cpu,log)
   return {'name':f'{kind}-smp{cpu}','exit_code':0,'external_log':str(log),'log_sha256':sha(log)}
  # Network probe has one intentionally fixed loopback peer port.
  with ThreadPoolExecutor(max_workers=1 if kind=='network' else 2) as pool:batch=list(pool.map(run,cpus))
  cases+=batch;guests+=batch
  proof=json.loads((evidence/f'guest-{kind}-media.json').read_text());assert proof['passed'] and proof['actual_iso_kernel_equals_current_production'] and proof['kernelcache_sha256']==report['kernelcache_sha256']
  media[kind]=proof['source_manifest_sha256'];save(f'kernel-probe-{kind}.json',proof)
  print('PASS current production kernel',kind,cpus,flush=True)
 assert len(guests)==13 and len(cases)==29 and len(media)==5
 report.update(passed=True,suite_case_count=len(cases),guest_profiles_passed=len(guests),actual_iso_kernel_equals_current_production_all_5_probes=True,source_manifest_sha256_by_probe=media)
finally:
 suite={'passed':bool(report['passed']),'scope':scope,'guest_requested':True,'cases':cases};save('kernel-regression-result.json',suite)
 report['suite_sha256']=sha(q/'kernel-regression-result.json');save('kernel-current-source-result.json',report)
