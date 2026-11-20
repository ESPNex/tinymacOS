#!/usr/bin/env python3
"""Boot one ISO, no HDD/bootstrap disk attached, in BIOS and direct UEFI."""
import hashlib,json,os,sys,tempfile,time
from pathlib import Path
root=Path(__file__).resolve().parents[2];sys.path.insert(0,str(root/'tinymacOS/tests'))
from retail_guest import Retail
from pixels import locate
iso=Path(os.environ.get('TMC_DUAL_ISO','/var/tmp/tmc-oc1-release/tinymacOS-2.4.4-OpenCore-x86_64.iso'));work=Path(tempfile.mkdtemp(prefix='tmc-oc1-qa-',dir='/var/tmp'));ev=work/'evidence';ev.mkdir()
os.environ.update(TOOLROOT='/var/tmp/tmc/tools/root',TMC_BUILD='/var/tmp/tmc-integrated-system',TMC_BIOS_ISO=str(iso),TMC_POINTER='mouse',TMC_SMP='2')
class OpticalBIOS(Retail):
 def wait(self,*a,**kw):
  end=time.monotonic()+95
  while time.monotonic()<end:
   im=self.snap();im.save(ev/'bios-last.png')
   if locate(im,'Crea un account'):return
   time.sleep(.15)
  raise AssertionError('BIOS optical ISO did not reach native setup')
report={'passed':False,'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'iso_bytes':iso.stat().st_size,'external_evidence':str(ev),'bios_installer':False}
try:
 for name,klass,launcher in [('bios',OpticalBIOS,root/'tinymacOS/tools/run-bios-iso.sh'),('uefi',Retail,None)]:
  with klass(iso,work,ev,name,live=True,launcher=launcher) as g:
   blocks=g.req('query-block');assert any(str(iso) in str(b) for b in blocks);assert not any('bootdisk' in str(b) or 'system.raw' in str(b) for b in blocks),blocks
   assert len(g.req('query-cpus-fast'))==2;g.shot(name+'-setup');g.enroll(name='DualBoot');g.key('f2');g.visible('Sessione locale');g.cmd('sw_vers','ProductVersion:    2.4.4');g.visible('BuildVersion:      21B5');g.cmd('bootverbose status','Verbose avvio corrente: '+('attivo' if name=='bios' else 'disattivo'));g.cmd('bootverbose log','[boot] CPUs online: 2');g.shot(name+'-desktop')
   report[name]={'passed':True,'only_optical_iso':True,'native_setup_and_login':True,'native_cpus_online':2,'terminal_version':'2.4.4 (21B5)','verbose':name=='bios'}
 report['passed']=True
finally:(Path(__file__).parent/'dualboot-result.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS one ISO: SeaBIOS optical OpenDuet/OpenCore and OVMF direct UEFI -> native desktop, two CPUs, no HDD; evidence',ev)
