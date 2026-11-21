#!/usr/bin/env python3
"""Physical-key Recovery typing, no firmware Unicode/layout fallback."""
import hashlib,json,os,sys,tempfile,time,shutil
from pathlib import Path
root=Path(__file__).resolve().parents[2];sys.path.insert(0,str(root/'tinymacOS/tests'))
from retail_guest import Retail
from boot_recovery import RecoveryKey
profile=os.environ.get('TMC_TEST_INPUT','ps2');firmware=os.environ.get('TMC_TEST_FIRMWARE','uefi')
iso=Path(os.environ['TMC_TEST_ISO']);w=Path(tempfile.mkdtemp(prefix='tmc10-rec-kbd-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
os.environ.update(TOOLROOT='/var/tmp/tmc/tools/root',TMC_POINTER='mouse',TMC_MEM='512')
class OpticalRec(RecoveryKey,Retail):pass
class InstalledRec(Retail):initial='Scegli come proseguire.'
kwargs={'live':True};Rec=OpticalRec
if firmware=='bios':
 from riviera_media import gpt
 receipt=json.loads((Path(__file__).parent/'bios-installer-result.json').read_text())
 assert receipt['passed'] and receipt['iso_sha256']==hashlib.file_digest(iso.open('rb'),'sha256').hexdigest()
 src=Path(receipt['external_work'])/'target/system.raw';(w/'target').mkdir();disk=w/'target/system.raw';shutil.copyfile(src,disk)
 cache=Path(os.environ['TMC_BUILD'])/'KernelCache';data=gpt(disk);off=data['esp'].index(cache.read_bytes())+128
 with disk.open('r+b') as f:f.seek(2048*512+off);b=f.read(1);f.seek(-1,1);f.write(bytes([b[0]^8]))
 kwargs={'live':False,'disk':'target','boot_disk':True};Rec=InstalledRec
record={'passed':False,'profile':profile,'firmware':firmware,'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'work':str(w)}
try:
 with Rec(iso,w,ev,'recovery',**kwargs,launcher=root/'tinymacOS/tools/run-input-qa.sh') as g:
  if firmware=='bios':assert '.iso' not in str(g.req('query-block'))
  record['local_installed_recovery']=firmware=='bios'
  qtree=g.hmp('info qtree');record['i8042_present']='i8042' in qtree
  if profile.endswith('-kbd'):assert not record['i8042_present']
  g.visible('Tastiera ITA');g.key('down');g.key('down');g.key('down');g.key('ret');g.visible('Shell di recupero locale',12)
  g.cmd('sw_vers','tinymacOS Recovery 10.0 (30A1)')
  # AltGr must produce @, not a Ctrl shortcut; ISO punctuation must not be US.
  text="àèéìòù€ @#[]{} \\|<> /?;:_=+*£"
  g.type(text);g.shot('italian-physical-keys');g.visible('€',faces=(1,));g.visible('@#[]{}',10);g.visible('/?;:_=+*',10)
  # Clear with scalar-aware backspace; the next command must be recognized.
  for _ in text:g.key('backspace',.04)
  g.cmd('csrutil status','System Integrity Protection');g.shot('backspace-command')
  record.update(passed=True,keyboard_only_menu=True,italian_punctuation=True,altgr=True,utf8_backspace=True)
finally:(Path(__file__).parent/f'recovery-keyboard-{firmware}-{profile}.json').write_text(json.dumps(record,indent=2,ensure_ascii=False)+'\n')
print('PASS Recovery keyboard',firmware,profile,w)
