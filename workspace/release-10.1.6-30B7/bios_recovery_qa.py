#!/usr/bin/env python3
"""Damage a sacrificial installed disk, boot BIOS-only, restore sealed OS."""
import hashlib,json,os,shutil,sys,tempfile,time
from pathlib import Path
root=Path(__file__).resolve().parents[2];sys.path.insert(0,str(root/'tinymacOS/tests'))
from retail_guest import Retail
from riviera_media import gpt
src=Path(json.loads((Path(__file__).parent/'bios-installer-result.json').read_text())['external_work'])/'target/system.raw'
w=Path(tempfile.mkdtemp(prefix='tmc-bios-recovery-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir();(w/'target').mkdir();disk=w/'target/system.raw';shutil.copyfile(src,disk);before=gpt(disk)
cache=Path('/var/tmp/tmc-10.1.6/build/KernelCache').read_bytes();offset=before['esp'].index(cache)+128
with disk.open('r+b') as f:f.seek(2048*512+offset);v=f.read(1);f.seek(-1,1);f.write(bytes([v[0]^8]))
pointer=os.environ.get('TMC_REC_POINTER','mouse');firmware=os.environ.get('TMC_REC_FIRMWARE','bios')
os.environ.update(TOOLROOT='/var/tmp/tmc/tools/root',TMC_POINTER=pointer,TMC_SMP='2')
class Rec(Retail):
 initial='Scegli come proseguire.'
 def reset_pointer(self):self.mx,self.my=1280-48,800-55
 def move(self,x,y):
  if pointer=='tablet':return super().move(x,y)
  self.hmp(f'mouse_move {x-self.mx} {y-self.my}');self.mx,self.my=x,y;time.sleep(.65)
 def mouse(self,down):self.hmp('mouse_button '+('1' if down else '0'));time.sleep(.45)
rec={'passed':False,'external_evidence':str(ev),'firmware':firmware,'pointer':pointer,'scope':'HDD-only tampered KernelCache rejection -> local Recovery -> sealed offline OS restore -> same BIOS desktop'}
try:
 with Rec('/not-attached',w,ev,'bios-recovery',disk='target',boot_disk=True,launcher=(root/'tinymacOS/tools/run-bios-installed.sh') if firmware=='bios' else None) as g:
  blocks=g.req('query-block');assert str(disk) in str(blocks) and '.iso' not in str(blocks)
  g.shot('bios-local-recovery')
  def clicktext(text):
   x,y=g.visible(text);g.click(x+10,y+6)
  clicktext('Reinstalla tinymacOS');clicktext('Continua');g.visible('Seleziona un disco');clicktext('Continua')
  g.visible('Conferma l’operazione');g.click(1060,648);g.visible('tinymacOS è stato reinstallato.',70);g.shot('bios-sealed-restore');clicktext('Fine');g.visible('Scegli come proseguire.')
  clicktext('Disco di avvio');clicktext('Continua');g.visible('Seleziona un disco');clicktext('Continua')
  g.visible('BiosDisk',60);g.unlock();g.key('f2');g.visible('Sessione locale');g.cmd('cat Documents/bios-proof.txt','BIOS_DISK_PERSISTENT');g.shot('bios-restored-desktop');g.cmd('off');g.p.wait(timeout=15);assert g.p.returncode==0
 after=gpt(disk);assert after['esp']==before['esp']==before['src'];assert after['data']==before['data']
 with disk.open('rb') as f:f.seek(64*512);duet=f.read((root/'tinymacOS/vendor/opencore-1.0.8/bootX64').stat().st_size)
 assert duet==(root/'tinymacOS/vendor/opencore-1.0.8/bootX64').read_bytes()
 rec.update(passed=True,local_recovery_without_iso=True,automatic_damaged_kernel_rejection=True,sealed_esp_restored_byte_identical=True,documents_unchanged=True,duet_gap_unchanged=True,desktop_reboot_after_restore=True)
finally:(Path(__file__).parent/('bios-recovery-result.json' if pointer=='mouse' and firmware=='bios' else firmware+'-recovery-'+pointer+'-result.json')).write_text(json.dumps(rec,indent=2)+'\n')
print('PASS BIOS-only local Recovery, sealed offline OS restore, preserved documents, BIOS desktop reboot')
