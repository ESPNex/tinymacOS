#!/usr/bin/env python3
"""Original published 30A2, identical final supplementary fixture; expected FAIL."""
from pathlib import Path
import hashlib,json,os,subprocess
r=Path(__file__).resolve().parent;home=r.parents[1]
iso=Path('/var/tmp/tmc30a2-final/tinymacOS-10-x86_64.iso')
with iso.open('rb') as f:digest=hashlib.file_digest(f,'sha256').hexdigest()
assert digest=='f6144153f7732af388310bab78e0947f89044fe11a64e209aa238ea0a611d87e'
log=r/'negative-original-30A2.log';out=Path('/var/tmp/tmc30a4-negative-original-final-fixture')
env={**os.environ,'TOOLROOT':'/var/tmp/tmc/tools/root','TMC_MEM':'512','TMC_OPTICAL_BIOS_LAUNCHER':str(r/'run-optical-recovery-input.sh'),'TMC_BUILD':'/var/tmp/tmc30a2-final','TMC_TEST_ISO':str(iso),'TMC_FIRMWARE_KBD_BUS':'xhci','TMC_REC_BOOT_KEY':'r','TMC_POINTER_RESULT':'/var/tmp/tmc30a4-negative-original-UNEXPECTED-PASS.json'}
with log.open('w') as f:res=subprocess.run(['python3',str(r/'recovery_pointer_qa.py'),str(out),'--firmware','bios','--profile','mouse'],cwd=home,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=160)
assert res.returncode!=0 and 'Native loader Recovery key did not reach menu' in log.read_text(),'Original ISO control unexpectedly changed outcome or failed at a different stage'
j={'passed':False,'selector_failure_reproduced':True,'same_final_harness':True,'iso_sha256':digest,'harness_sha256':hashlib.sha256((r/'recovery_pointer_qa.py').read_bytes()).hexdigest(),'log':log.name,'log_sha256':hashlib.sha256(log.read_bytes()).hexdigest(),'external_work':str(out),'returncode':res.returncode,'expected_failure':'Native loader Recovery key did not reach menu','physical_hardware_tested':False}
(r/'negative-original-30A2-result.json').write_text(json.dumps(j,indent=2)+'\n')
print('PASS negative-control verification: original published 30A2 reproduces native selector failure under the final fixture')
