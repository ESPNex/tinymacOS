#!/usr/bin/env python3
"""Recovery firmware-only Italian keyboard regression: isolated PS/2 or USB."""
import hashlib,json,os,sys,tempfile,time,shutil
from pathlib import Path
root=Path(__file__).resolve().parents[2];sys.path.insert(0,str(root/'tinymacOS/tests'))
from retail_guest import Retail
from boot_recovery import RecoveryKey
profile=os.environ.get('TMC_TEST_INPUT','ps2');firmware=os.environ.get('TMC_TEST_FIRMWARE','uefi')
iso=Path(os.environ['TMC_TEST_ISO']);w=Path(tempfile.mkdtemp(prefix='tmc10-rec-kbd-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
os.environ.update(TOOLROOT='/var/tmp/tmc/tools/root',TMC_POINTER='mouse',TMC_MEM='512')
class OpticalRec(RecoveryKey,Retail):
 recovery_text='Ripristina da una copia locale'
 def key(self,k,dt=.25,hold=None):
  if hold is None:hold=.30 if firmware=='bios' else .06
  if '-' in k:
   parts=k.split('-');mods=parts[:-1];key=parts[-1]
   def edge(keys,down):self.req('input-send-event',events=[{'type':'key','data':{'down':down,'key':{'type':'qcode','data':p}}} for p in keys])
   edge(mods,True);time.sleep(.20);edge([key],True);time.sleep(hold);edge([key],False);time.sleep(.12);edge(list(reversed(mods)),False);time.sleep(dt);return
  return super().key(k,dt,hold)
kwargs={'live':True};Rec=OpticalRec
record={'passed':False,'profile':profile,'firmware':firmware,'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'work':str(w)}
try:
 with Rec(iso,w,ev,'recovery',**kwargs,launcher=root/'tinymacOS/tools/run-input-qa.sh') as g:
  record['local_installed_recovery']=False
  record['keyboard_still_native']=False
  record['keyboard_transport']='OpenUsbKbDxe Apple keymap / declared derived Ps2KeyboardDxe raw protocol'
  record['physical_hardware_tested']=False
  qtree=g.hmp('info qtree');record['i8042_present']='i8042' in qtree
  if profile.endswith('-kbd'):assert not record['i8042_present']
  g.visible('tinymacOS Recovery');g.key('down');g.key('down');g.key('down');g.key('ret');g.visible('recovery %',12)
  g.cmd('sw_vers','tinymacOS Recovery 10.1.7 (30B8)')
  # AltGr must produce @, not a Ctrl shortcut; ISO punctuation must not be US.
  text="àèéìòù€ @#[]{} \\|<> /?;:_=+*£"
  g.type(text);g.shot('italian-physical-keys');g.visible(text,10,faces=(1,));g.visible('€',faces=(1,));g.visible('@#[]{}',10);g.visible('/?;:_=+*',10)
  # Clear with scalar-aware backspace; the next command must be recognized.
  for _ in text:g.key('backspace',.25)
  g.cmd('csrutil status','System Integrity Protection');g.shot('backspace-command')
  record.update(passed=True,keyboard_only_menu=True,italian_punctuation=True,altgr=True,utf8_backspace=True)
finally:(Path(__file__).parent/f'recovery-keyboard-{firmware}-{profile}.json').write_text(json.dumps(record,indent=2,ensure_ascii=False)+'\n')
print('PASS Recovery keyboard',firmware,profile,w)
