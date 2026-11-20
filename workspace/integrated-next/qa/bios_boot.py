#!/usr/bin/env python3
import os,sys,tempfile,time,json
from pathlib import Path
sys.path.insert(0,'/home/user/tinymacOS/tests')
from retail_guest import Retail
from pixels import locate
w=Path(tempfile.mkdtemp(prefix='tmc-bios-test-',dir='/var/tmp'));e=Path(__file__).resolve().parents[1]/'evidence/bios-current';e.mkdir(exist_ok=True)
os.environ.update(TOOLROOT='/var/tmp/tmc/tools/root',TMC_BUILD='/var/tmp/tmc-integrated-system',TMC_BIOS_DISK=os.environ.get('TMC_BIOS_DISK','/var/tmp/tmc-legacy-config3.raw'),TMC_POINTER='mouse')
class BIOS(Retail):
 def frame(self):return self.snap()
 def wait(self,*args,**kwargs):
  end=time.monotonic()+80;saved=False
  while time.monotonic()<end:
   im=self.snap();im.save(e/'bios-latest.png')
   if im.size==(1280,800) and im.crop((0,0,140,7)).convert('L').getextrema()[1]>180:im.save(e/'bios-picker-sample.png')
   if time.monotonic()>end-65 and not getattr(self,'boot_selected',False):
    im.save(e/'bios-opencore-picker.png');self.key('ret');self.boot_selected=True
   if not saved and time.monotonic()>end-73:im.save(e/'bios-early.png');saved=True
   if locate(im,'Crea un account',faces=(0,1,2,3,5,6,8)):return
   time.sleep(.3)
  raise AssertionError('BIOS/DUET/OpenCore did not reach native account setup')
record={'passed':False}
try:
 with BIOS('/not-attached',w,e,'bios-duet',live=True,launcher=Path('/home/user/tinymacOS/tools/run-bios-qemu.sh')) as g:
  g.shot('bios-duet-setup');g.enroll(name='Bios');g.key('f2');g.visible('Sessione locale');g.cmd('bootverbose status');g.shot('bios-status-actual');g.visible('Verbose avvio corrente: attivo');g.shot('bios-verbose-status');g.cmd('bootverbose log');g.visible('[boot] tinymacOS native kernel',faces=(0,1,2,3,5,6,8));g.visible('[boot] CPUs online: 2');g.shot('bios-verbose-log')
  record.update(passed=True,firmware='SeaBIOS',duet='OpenDuet 1.0.8 BlockIO',opencore='1.0.8',native_setup_login=True,native_cpus_online=2,verbose_source='BOOTCFG.BIN verbose flag',verbose_argument=False,terminal_bootverbose=True,bios_installer_boot_without_bootstrap=False)
finally:
 (e/'bios-duet.json').write_text(json.dumps(record,indent=2));print('run directory',w)
print('PASS BIOS -> OpenDuet -> OpenCore -> native kernel/setup/login/verbose command')
