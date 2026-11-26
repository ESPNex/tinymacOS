"""Actual optical native Recovery on a Windows-like GPT without tinymacOS.
Real USB relative mouse movement/clicks; read-only metadata/boundary proofs.
"""
from pathlib import Path
import os,ast,time,struct,json,tempfile,hashlib,zlib,uuid
import numpy as np
from retail_guest import Retail
from boot_recovery import RecoveryKey
base=Path(os.environ['TMC_QA_DIR']);iso=Path(os.environ['TMC_STAGE_ISO']);root=Path(__file__).resolve().parents[1]
w=Path(tempfile.mkdtemp(prefix='tmc-recovery-no-install-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
total=32*1024*2048;rec=total-1048576-2048;gap=rec-10*1024*2048
module=ast.parse(Path(__file__).with_name('hotfix_windows_guest.py').read_text())
exec(compile(ast.Module(body=[n for n in module.body if isinstance(n,ast.FunctionDef) and n.name in ('fixture','fingerprints')],type_ignores=[]),'fixture','exec'))
module=ast.parse(Path(__file__).with_name('levante_pointer.py').read_text())
exec(compile(ast.Module(body=[n for n in module.body if isinstance(n,ast.FunctionDef) and n.name=='cursor'],type_ignores=[]),'cursor','exec'))
sprite=np.frombuffer((root/'assets/symbols.rgba').read_bytes(),dtype=np.uint8).reshape(-1,64,64,4)[15]
os.environ.update(TMC_STORAGE='nvme',TMC_POINTER='mouse',TMC_SMP='2',TMC_MEM='512',OCR_ROOT=os.environ['TOOLROOT'])
r={'passed':False,'work':str(w),'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'physical_hardware_tested':False,'checks':[]}
disk=w/'target/system.raw';ranges=fixture(disk,False);before=fingerprints(disk,ranges)
class Recovery(RecoveryKey,Retail):recovery_text='Ripristina da una copia locale'
try:
 with Recovery(iso,w,ev,'no-native-recovery',disk='target',live=False) as g:
  cursor(g,180,210);cursor(g,1100,660);r['checks'].append('Native optical Recovery: real relative USB mouse moves rendered cursor exactly in two positions')
  g.click(550,355);g.click(880,557);g.visible('Per reinstallare il sistema');g.visible('Per una nuova installazione avvia il desktop live della ISO.');g.shot('recovery-live-install-guidance')
  g.key('ret');g.visible('Seleziona la destinazione');g.visible('Nessun tinymacOS installato: usa il desktop live.');g.key('down');g.key('ret');g.visible('Seleziona la destinazione');g.shot('recovery-no-native-not-corrupt-table');g.key('esc');g.key('esc')
 assert fingerprints(disk,ranges)==before
 r['checks'].append('No native install: truthful live-desktop guidance, destination remains disabled without claiming corrupt Windows GPT; metadata and occupied boundaries unchanged')
 r['passed']=True
except Exception as e:r['failure']=str(e);raise
finally:(base/'recovery-no-install-guest-result.json').write_text(json.dumps(r,indent=2)+'\n')
print('PASS native Recovery without tinymacOS: relative mouse, explicit live-installer guidance, zero disk modifications')
