"""Native NVMe development gate on a disposable QEMU namespace.
Still uses legacy TMCBFS1: this is NOT acceptance of the new filesystem.
"""
from pathlib import Path
import hashlib,json,os,struct,tempfile,time
from retail_guest import Retail
from boot_recovery import RecoveryKey
import pixels,retail_guest
root=Path(__file__).resolve().parents[1];base=Path(os.environ['TMC_QA_DIR']);iso=Path(os.environ['TMC_STAGE_ISO'])
w=Path(tempfile.mkdtemp(prefix='tmc-nvme-guest-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
font=Path(os.environ['TMC_FONT']).read_bytes();pixels.b=font;pixels.nf,pixels.ng=struct.unpack_from('<II',font,4);pixels.codes={c:j for j,c in enumerate(struct.unpack_from(f'<{pixels.ng}I',font,12))};pixels.start=12+pixels.ng*4;pixels.pix=pixels.start+pixels.nf*(pixels.ng*8+4);pixels.word.cache_clear()
old=retail_guest.locate;retail_guest.locate=lambda im,text,faces=(0,1,2,3,5,6,8),threshold=.88:old(im,text,faces,threshold)
os.environ.update(TMC_STORAGE='nvme',TMC_MEM='512',TMC_SMP='2',TMC_POINTER='tablet',TMC_BIOS_ISO=str(iso),OCR_ROOT=os.environ['TOOLROOT'])
r={'passed':False,'release_ready':False,'scope':'native NVMe transport development gate; not complete 10.2.5 acceptance','work':str(w),'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'physical_hardware_tested':False,'new_filesystem_integrated':False,'native_nvme_io_and_uefi':False,'bios_live_native_nvme':False,'bios_installed_nvme':False,'bios_firmware_nvme_absence_verified':False,'checks':[]}
def ok(s):r['checks'].append(s);print('PASS',s,flush=True)
class Installed(Retail):initial='NvmeDisk'
class Recovery(RecoveryKey,Retail):recovery_text='Ripristina da una copia locale'
resume=os.environ.get('TMC_NVME_RESUME')=='1'
if resume:
 previous=json.loads((base/'nvme-guest-result.json').read_text());assert previous['iso_sha256']==r['iso_sha256'] and previous['native_nvme_io_and_uefi']
 w=Path(previous['work']);assert str(w).startswith('/var/tmp/tmc-nvme-guest-') and (w/'target/system.raw').is_file();ev=w/'evidence';r=previous;r['resumed_at_bios']=True;r['bios_firmware_nvme_absence_verified']=False
try:
 if not resume:
  with Retail(iso,w,ev,'nvme-install',disk='target',live=False) as g:
   assert 'nvme,drive=hd' in str(g.p.args) or any('nvme' in str(d) for d in g.req('query-block'))
   g.enroll(name='NvmeDisk');g.key('f2');g.visible('Sessione locale');g.cmd('diskutil info disk0','QEMU NVMe');g.shot('nvme-native-detected')
   g.cmd('bootverbose log','[nvme] disk 0');g.key('f11');g.visible('Installa tinymacOS Tramontana');g.key('ret');g.visible('Seleziona la destinazione');g.key('down');g.key('ret');g.visible('Conferma l’installazione');g.key('ret');g.key('spc');g.key('ret');g.visible('Il disco è pronto',180);g.shot('nvme-install-complete');g.key('ret');g.p.wait(timeout=20)
  ok('Actual PCIe NVMe controller/namespace discovered after ExitBootServices; native queue read/write/flush executes the real GUI installer (legacy filesystem payload)')
  with Installed('/not-attached',w,ev,'nvme-uefi',disk='target',boot_disk=True) as g:
   assert str(iso) not in str(g.req('query-block'));g.unlock();g.key('f2');g.visible('Sessione locale');g.cmd('echo NATIVE_NVME_PERSISTENCE > Documents/nvme.txt');g.cmd('cat Documents/nvme.txt','NATIVE_NVME_PERSISTENCE');g.shot('nvme-uefi-home');g.cmd('off');g.p.wait(timeout=20)
  with Recovery('/not-attached',w,ev,'nvme-recovery',disk='target',boot_disk=True) as g:
   x,y=g.visible('Utility',box=(0,0,800,34));g.click(x+12,y+8);x,y=g.visible('Terminale',box=(0,34,900,250));g.click(x+20,y+8);g.visible('Sessione locale');g.cmd('diskutil verifyVolume disk0','Il volume risulta valido.');g.shot('nvme-native-recovery');g.cmd('off');g.p.wait(timeout=20)
  r['native_nvme_io_and_uefi']=True;ok('UEFI NVMe-only installed boot and internal Recovery without ISO, persistent document and native firstaid readback')
 with Retail(iso,w,ev,'nvme-bios-live',disk='biosprobe',live=False,launcher=root/'tools/run-bios-installed.sh') as g:
  g.enroll(name='BiosNvme');g.key('f2');g.visible('Sessione locale');g.cmd('diskutil info disk0','QEMU NVMe');g.shot('nvme-native-from-bios-live');g.cmd('off');g.p.wait(timeout=20)
 r['bios_live_native_nvme']=True;ok('BIOS/OpenDuet optical live boot hands off to the same native NVMe driver; disposable namespace is independently inventoried')
 with Installed('/not-attached',w,ev,'nvme-bios-installed',disk='target',boot_disk=True,launcher=root/'tools/run-bios-installed.sh') as g:
  assert str(iso) not in str(g.req('query-block'));g.unlock();g.key('f2');g.visible('Sessione locale');g.cmd('cat Documents/nvme.txt','NATIVE_NVME_PERSISTENCE');g.shot('nvme-bios-installed-home');g.cmd('off');g.p.wait(timeout=20)
 r['bios_installed_nvme']=True;ok('BIOS/OpenDuet installed NVMe-only boot without optical media and cross-firmware document persistence')
 r['passed']=True
except Exception as error:
 r['failure']=str(error);raise
finally:(base/'nvme-guest-result.json').write_text(json.dumps(r,indent=2)+'\n')
