"""Actual ISO -> native installer -> mixed disk cold boots (BIOS and UEFI).
Existing FAT32/NTFS/RAW bytes and original partition entries are compared.
The previous OS boot is a synthetic MBR sentinel, not certified Windows boot.
"""
import contextlib,hashlib,json,os,struct,subprocess,tempfile,time
from pathlib import Path
from retail_guest import Retail
import pixels,retail_guest
from dualboot_fixtures import fixture,hashes,verify_files,PARTS,SECTORS
from boot_recovery import RecoveryKey
root=Path(__file__).resolve().parents[1];base=Path(os.environ['TMC_QA_DIR']);iso=Path(os.environ['TMC_STAGE_ISO']);scheme=os.sys.argv[1]
w=Path(tempfile.mkdtemp(prefix='tmc-dualboot-'+scheme+'-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir();disk=w/'target/system.raw';before=fixture(disk,scheme)
original=disk.read_bytes()[:(34*512 if scheme=='gpt' else 512)]
font=Path(os.environ['TMC_FONT']).read_bytes();pixels.b=font;pixels.nf,pixels.ng=struct.unpack_from('<II',font,4);pixels.codes={c:j for j,c in enumerate(struct.unpack_from(f'<{pixels.ng}I',font,12))};pixels.start=12+pixels.ng*4;pixels.pix=pixels.start+pixels.nf*(pixels.ng*8+4);pixels.word.cache_clear()
old=retail_guest.locate;retail_guest.locate=lambda im,text,faces=(0,1,2,3,5,6,8),threshold=.88:old(im,text,faces,threshold)
os.environ.update(OCR_ROOT=os.environ['TOOLROOT'],TMC_MEM='512',TMC_POINTER='tablet',TMC_BIOS_ISO=str(iso),TMC_SMP='2')
r={'passed':False,'work':str(w),'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'scheme':scheme,'checks':[],'physical_hardware_tested':False,'windows_boot_tested':False,'ntfs_fixture':'real mkntfs filesystem and independently read /PRESERVE.TXT; no Windows OS installed'}
def ok(s):r['checks'].append(s);print('PASS',s,flush=True)
class Installed(Retail):
 initial='DualBoot'
 def wait(self,*a,**k):
  # BIOS menu is drawn by int 10h, so use independent OCR, not native font matching.
  if '-pflash' not in self.p.args and 'run-bios-installed.sh' in str(self.p.args):
   until=time.monotonic()+30
   while time.monotonic()<until:
    text=self.ocr(psm=6)
    if 'Previous' in text or 'previous' in text:self.key('2');break
    time.sleep(.25)
   else:raise AssertionError('Native BIOS coexistence menu missing: '+text)
  return super().wait(*a,**k)
try:
 with Retail(iso,w,ev,'installer',disk='target',live=False) as g:
  g.enroll(name='DualBoot');g.key('f2');g.visible('Sessione locale');g.cmd('diskutil list','FAT32');g.visible('NTFS');g.visible('RAW');g.shot('terminal-filesystems')
  g.cmd('diskutil info disk0s3','RAW non significa spazio libero.');g.cmd('open -a "Disk Utility"');g.visible('Utility Disco');g.key('down');g.visible('FAT32',box=(450,200,1200,650));g.shot('utility-fat32');g.key('down');g.visible('NTFS',box=(450,200,1200,650));g.shot('utility-ntfs');g.key('down');g.visible('RAW / non riconosciuto',box=(450,200,1200,650));g.key('s');g.visible('Seleziona un disco con un volume tinymacOS.');g.shot('utility-raw-protected');g.key('f11');g.visible('tinymacOS Tramontana');g.shot('installer-welcome');g.key('ret');g.visible('Seleziona la destinazione');g.visible('FAT32, NTFS, RAW');g.shot('installer-destination');g.key('down');g.key('ret');g.visible('Conferma l’installazione');g.shot('installer-confirmation');g.key('spc');g.key('ret');g.visible('Il disco è pronto',180);g.shot('installer-complete');g.key('ret');g.p.wait(timeout=15)
 assert hashes(disk)==before
 with disk.open('rb') as f:
  f.seek(0);mbr=f.read(512);assert mbr[440:446]==original[440:446]
  if scheme=='mbr':assert mbr[446:494]==original[446:494];start,length=struct.unpack_from('<II',mbr,502)
  else:
   f.seek(1024);p=f.read(16384);assert p[:3*128]==original[1024:1024+3*128];start,end=struct.unpack_from('<QQ',p,3*128+32);length=end-start+1
  assert length==196608
  f.seek((start+10)*512);assert f.read(512)==original[:512]
  f.seek((start+9)*512);assert f.read(8)==b'TMCVOL23'
 ok('Installer copies real system/data/Recovery only into a new 96 MiB extent; every FAT32/NTFS/RAW payload hash and original partition entry is unchanged')
 with Installed('/not-attached',w,ev,'uefi-cold',disk='target',boot_disk=True) as g:
  assert str(iso) not in str(g.req('query-block'));g.unlock();g.key('f2');g.visible('Sessione locale');g.cmd('sw_vers -productVersion','10.2.5');g.cmd('echo MIXED_DISK_PERSISTENCE > Documents/dual.txt');g.cmd('bootverbose on','Preferenza verificata su disco');g.shot('uefi-installed');g.cmd('off');g.p.wait(timeout=15)
 ok('Cold UEFI boot of the mixed disk without ISO, native account login, data persistence and relocated BootConfig write')
 with Installed('/not-attached',w,ev,'bios-cold',disk='target',boot_disk=True,launcher=root/'tools/run-bios-installed.sh') as g:
  g.unlock();g.key('f2');g.visible('Sessione locale');g.cmd('cat Documents/dual.txt','MIXED_DISK_PERSISTENCE');g.cmd('bootverbose status','attivo');g.shot('bios-installed');g.cmd('bootverbose off','Preferenza verificata su disco');g.cmd('off');g.p.wait(timeout=15)
 ok('BIOS menu selects native OpenDuet boot from the new extent; same installed account and document survive cross-firmware boot')
 class Recovery(RecoveryKey,Retail):recovery_text='Ripristina da una copia locale'
 with Recovery('/not-attached',w,ev,'installed-recovery',disk='target',boot_disk=True) as g:
  assert str(iso) not in str(g.req('query-block'));g.shot('installed-recovery')
  x,y=g.visible('Utility',box=(0,0,800,34));g.click(x+12,y+8);x,y=g.visible('Terminale',box=(0,34,900,250));g.click(x+20,y+8);g.visible('Sessione locale')
  g.cmd('diskutil list','FAT32');g.visible('NTFS');g.visible('RAW');g.cmd('diskutil verifyVolume disk0','Il volume risulta valido.');g.shot('recovery-filesystems');g.cmd('off');g.p.wait(timeout=15)
 ok('Native installed Recovery boots without ISO from its internal FAT area; Terminal inventories existing filesystems and verifies relocated native data/system seals')
 class Previous(Retail):
  def wait(self,*a,**k):
   until=time.monotonic()+30
   while time.monotonic()<until:
    text=self.ocr(psm=6)
    if 'Previous' in text or 'previous' in text:self.key('1');break
    time.sleep(.2)
   else:raise AssertionError(text)
   until=time.monotonic()+10
   while time.monotonic()<until:
    text=self.ocr(psm=6)
    if 'PREVIOUSBOOTOK' in ''.join(c for c in text.upper() if c.isalnum()):return
    time.sleep(.2)
   raise AssertionError(text)
 with Previous('/not-attached',w,ev,'previous-boot',disk='target',boot_disk=True,launcher=root/'tools/run-bios-installed.sh') as g:g.snap().save(ev/'previous-boot-sentinel.png')
 ok('BIOS choice 1 executes the byte-preserved original MBR sentinel (not a Windows certification)')
 assert hashes(disk)==before;verify_files(disk,w);ok('Foreign partition hashes remain unchanged after all boots; independent mtype/ntfscat recover original FAT32 and NTFS files')
 r.update(passed=True,foreign_payload_sha256=before,allocated_start_lba=start,allocated_sectors=length)
finally:(base/('dualboot-'+scheme+'-result.json')).write_text(json.dumps(r,indent=2)+'\n')
