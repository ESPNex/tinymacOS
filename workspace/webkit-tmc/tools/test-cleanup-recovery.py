#!/usr/bin/env python3
"""Actual native Recovery after EFI-runtime retirement. Disposable UEFI boots.
NOT installed 4Kn/BIOS re-certification, not the obsolete EFI Recovery mocks.
"""
from pathlib import Path
import os,sys,json,time,tempfile,hashlib
r=Path(__file__).resolve().parents[1];sys.path.insert(0,str(r.parent.parent/'tinymacOS/tests'))
from retail_guest import Retail
from boot_recovery import RecoveryKey
base=Path(os.environ['TMC_PRODUCTION_BUILD']);iso=base/'tinymacOS-10.2.5-x86_64.iso';cpu=int(os.environ['TMC_SMP']);assert cpu in (1,4)
results=Path(os.environ['TMC_QA_RESULTS']);source=json.loads((results/'vm-system-source-hashes.json').read_text())
for name,sha in source.items():assert hashlib.sha256((r.parent.parent/'tinymacOS'/name).read_bytes()).hexdigest()==sha,name
assert (base/'RecoveryKernel.elf').is_file()
os.environ.update(TMC_BUILD=str(base),TMC_POINTER='mouse',OCR_ROOT=os.environ['TOOLROOT'])
w=Path(tempfile.mkdtemp(prefix='tmc-cleanup-recovery-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
record={'passed':False,'requested_cpus':cpu,'iso_sha256':hashlib.sha256(iso.read_bytes()).hexdigest(),'external_evidence':str(ev),'scope':'fresh base ordinary R-key native Recovery, panel and Terminal; UEFI disposable optical only','physical_hardware_tested':False,'old_EFI_Recovery_runtime_removed':True}
class Recovery(RecoveryKey,Retail):recovery_text='Ripristina da una copia locale'
try:
 with Recovery(iso,w,ev,'native-recovery',live=True) as g:
  g.visible('Ripristina da una copia locale');g.visible('Utility',box=(0,0,900,34));g.shot('native-recovery-home')
  im=g.snap();assert all(x==(45,45) for x in im.crop((544,489,736,495)).getextrema()),'Native Recovery panel contains old progress bar pixels'
  g.key('f2');g.visible('Sessione locale');g.shot('native-recovery-terminal')
  record.update(passed=True,ordinary_R_key_boot=True,native_Recovery_panel=True,no_boot_progress_residue=True,native_Terminal_opened=True)
finally:(results/f'cleanup-recovery-smp{cpu}.json').write_text(json.dumps(record,indent=2)+'\n')
print('PASS native Recovery after cleanup on',cpu,'CPUs: R-key boot, panel, no residue and Terminal; no EFI Recovery runtime')
