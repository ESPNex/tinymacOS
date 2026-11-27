"""Real no-echo unlock of GUI-created FV003, including bad/good passwords.
Only disposable QA media; encrypted DATA never prepared/modified by the host.
"""
from pathlib import Path
import ast,contextlib,hashlib,io,json,os,struct,tempfile,time
import cv2,numpy as np
import pixels,retail_guest
from retail_guest import Retail
from boot_recovery import RecoveryKey
# Import pure drivers only, not the unrelated destructive reset suite.
source=Path(__file__).with_name('encrypted_install_reset_guest.py')
names={'Private','Locked','Recovery','utility','terminal','read_rendered_key','prepare_encryption'}
module=ast.parse(source.read_text());exec(compile(ast.Module(body=[n for n in module.body if isinstance(n,(ast.ClassDef,ast.FunctionDef)) and n.name in names],type_ignores=[]),str(source),'exec'))
iso=Path(os.environ['TMC_STAGE_ISO']);qa=Path(os.environ['TMC_QA_DIR']);qa.mkdir(parents=True,exist_ok=True)
w=Path(tempfile.mkdtemp(prefix='tmc-m1-vault-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir();disk=w/'target/system.raw'
os.environ.update(TMC_MEM='512',TMC_SMP='2',TMC_POINTER='tablet',TMC_STORAGE='nvme')
r={'passed':False,'release_ready':False,'physical_hardware_tested':False,'host_prepared_encrypted_data':False,'work':str(w),'checks':[],'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest()}
try:
    with Private(iso,w,ev,'m1-noecho-encrypted-install',disk='target',live=False) as g:
        g.enroll(name='M1Vault');g.key('f11');g.visible('Installa tinymacOS Tramontana');g.key('ret');g.visible('Seleziona la destinazione');g.key('down');g.key('ret');g.visible('Conferma l’installazione')
        key=prepare_encryption(g,'m1-volume-285');key=None
        g.key('spc');g.key('ret');g.visible('Il disco è pronto',300);g.key('ret');g.p.wait(timeout=20)
    with disk.open('rb') as f:
        f.seek(1024+128+32);a,b=struct.unpack('<QQ',f.read(16));f.seek(a*512);assert f.read(8)==b'TMCFV003'
    r['checks'].append('Actual GUI installer created FV003 on a fresh target; host only read the resulting volume signature')
    with Recovery('/not-attached',w,ev,'m1-noecho-native-recovery',disk='target',boot_disk=True) as g:
        utility(g,'Terminale');g.move(60,700);g.visible('Sessione locale')
        g.cmd('apfs_unlockfv');g.visible('Senza eco');g.type('wrong-volume-password');g.key('ret');g.visible('Sblocco non riuscito')
        g.cmd('diskutil apfs unlockVolume');g.visible('Senza eco');g.type('m1-volume-285');g.key('ret');g.visible('Il volume è stato sbloccato.')
        g.cmd('history');g.cmd('fdesetup manage');g.visible('FileVault');g.visible('Attivo');g.shot('noecho-actual-FV003-unlocked')
        r['checks'].append('ISO-detached native Recovery: Terminal no-echo alias rejects bad password, second alias unlocks actual FV003 with correct password; independent FileVault GUI reports Attivo')
        g.key('ctrl-w');g.move(60,700);g.visible('Sessione locale');g.cmd('off');g.p.wait(timeout=20)
    r['passed']=True
except Exception as e:r['failure']=str(e);raise
finally:
    (qa/'m1-terminal-vault-result.json').write_text(json.dumps(r,indent=2)+'\n')
    for p in w.glob('*.ppm'):p.unlink()
print('PASS native Terminal no-echo -> actual GUI-created FV003; wrong password rejected, correct password unlocked, independent FileVault status verified',flush=True)
