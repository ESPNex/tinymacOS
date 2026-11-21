#!/usr/bin/env python3
"""Run and record every host suite's actual exit code; never infer PASS from log text."""
from pathlib import Path
import subprocess,hashlib,json
q=Path(__file__).resolve().parent;home=q.parents[1]
cases=[('host-input',['bash','tinymacOS/tests/test-input-maintenance.sh']),('host-legacy',['bash','tinymacOS/tests/test-legacy-input.sh']),('pointer-contract',['python3',str(q/'pointer_contract.py')]),('regression-negative-controls',['python3',str(q/'regression-negative-controls.py')]),('control-contract',['python3',str(q/'control_contract.py')])]
results=[]
for name,cmd in cases:
 r=subprocess.run(cmd,cwd=home,capture_output=True,text=True);(q/(name+'.log')).write_text(r.stdout+r.stderr)
 results.append({'name':name,'exit_code':r.returncode});print(('PASS' if r.returncode==0 else 'FAIL'),name,flush=True)
 if r.returncode:break
ok=len(results)==len(cases) and all(r['exit_code']==0 for r in results)
(q/'host-suite-result.json').write_text(json.dumps({'passed':ok,'cases':results,'source_manifest_sha256':hashlib.sha256((q/'source-hashes.json').read_bytes()).hexdigest()},indent=2)+'\n')
raise SystemExit(0 if ok else 1)
