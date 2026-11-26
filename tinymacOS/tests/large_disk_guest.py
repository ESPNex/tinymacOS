"""Actual AHCI/kernel install near the end of a sparse 1 TiB GPT disk.
No physical devices, no terabyte allocation, no whole-image read/copy.
"""
from pathlib import Path
import hashlib,json,os,struct,tempfile,time,uuid,zlib
from retail_guest import Retail
from boot_recovery import RecoveryKey
import pixels,retail_guest
root=Path(__file__).resolve().parents[1];base=Path(os.environ['TMC_QA_DIR']);iso=Path(os.environ['TMC_STAGE_ISO'])
w=Path(tempfile.mkdtemp(prefix='tmc-large-disk-guest-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir();(w/'target').mkdir();disk=w/'target/system.raw'
z=2147483648;high=(z-524288)&~2047;canaries={a:hashlib.sha256(str(a).encode()).digest()*16 for a in [2048,123456789,high-1]}
table=bytearray(16384);table[:16]=uuid.UUID('0fc63daf-8483-4772-8e79-3d69d8477de4').bytes_le;table[16:32]=uuid.UUID('cda578a1-8df6-43b0-a0da-cd471bd8c55b').bytes_le;struct.pack_into('<QQ',table,32,2048,high-1);name='Preserved foreign partition'.encode('utf-16le');table[56:56+len(name)]=name
def header(at,other,entries):
 h=bytearray(512);h[:8]=b'EFI PART';struct.pack_into('<IIIIQQQQ',h,8,65536,92,0,0,at,other,34,z-34);h[56:72]=uuid.UUID('a21fb4a9-81d4-45ed-bccc-0712ada938a7').bytes_le;struct.pack_into('<QIII',h,72,entries,128,128,zlib.crc32(table));struct.pack_into('<I',h,16,zlib.crc32(h[:92]));return h
with disk.open('xb') as f:
 f.truncate(z*512);mbr=bytearray(512);mbr[450]=0xee;struct.pack_into('<II',mbr,454,1,z-1);mbr[510:]=b'\x55\xaa';f.write(mbr)
 for at,data in [(1,header(1,z-1,2)),(2,table),(z-33,table),(z-1,header(z-1,1,z-33)),*canaries.items()]:f.seek(at*512);f.write(data)
font=Path(os.environ['TMC_FONT']).read_bytes();pixels.b=font;pixels.nf,pixels.ng=struct.unpack_from('<II',font,4);pixels.codes={c:j for j,c in enumerate(struct.unpack_from(f'<{pixels.ng}I',font,12))};pixels.start=12+pixels.ng*4;pixels.pix=pixels.start+pixels.nf*(pixels.ng*8+4);pixels.word.cache_clear()
old=retail_guest.locate;retail_guest.locate=lambda im,text,faces=(0,1,2,3,5,6,8),threshold=.88:old(im,text,faces,threshold)
os.environ.update(OCR_ROOT=os.environ['TOOLROOT'],TMC_MEM='512',TMC_SMP='2',TMC_POINTER='tablet')
r={'passed':False,'work':str(w),'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'logical_sector_bytes':512,'disk_bytes':z*512,'native_partition_lba':high,'physical_hardware_tested':False,'checks':[]}
def ok(s):r['checks'].append(s);print('PASS',s,flush=True)
def verify():
 with disk.open('rb') as f:
  for at,data in canaries.items():f.seek(at*512);assert f.read(512)==data
  f.seek(1024);p=f.read(16384);assert p[:128]==table[:128]
  for at in [1,z-1]:
   f.seek(at*512);h=bytearray(f.read(512));c=struct.unpack_from('<I',h,16)[0];struct.pack_into('<I',h,16,0);assert zlib.crc32(h[:92])==c;assert struct.unpack_from('<I',h,88)[0]==zlib.crc32(p)
  f.seek((z-33)*512);assert f.read(16384)==p
  assert struct.unpack_from('<Q',p,128+32)[0]==high
class Installed(Retail):
 initial='LargeDisk'
 def wait(self,*a,**k):
  if 'run-bios-installed.sh' in str(self.p.args):
   until=time.monotonic()+30
   while time.monotonic()<until:
    text=self.ocr(psm=6)
    if 'Previous' in text or 'previous' in text:self.key('2');break
    time.sleep(.2)
   else:raise AssertionError('Native BIOS coexistence menu missing: '+text)
  return super().wait(*a,**k)
class Recovery(RecoveryKey,Retail):recovery_text='Ripristina da una copia locale'
try:
 with Retail(iso,w,ev,'large-install',disk='target',live=False) as g:
  g.enroll(name='LargeDisk');g.key('f2');g.visible('Sessione locale');g.cmd('diskutil info disk0','1048576 MiB');g.visible('GPT');g.shot('recognized-1TiB-GPT')
  g.cmd(f'diskutil addPartition disk0 {high} 32 Probe','Solo anteprima');g.shot('high-LBA-preview')
  g.key('f11');g.visible('tinymacOS Tramontana');g.key('ret');g.visible('Seleziona la destinazione');g.visible('GPT');g.shot('large-eligible-destination');g.key('down');g.key('ret');g.visible('Conferma l’installazione');g.key('spc');g.key('ret');g.visible('Il disco è pronto',180);g.shot('large-install-complete');g.key('ret');g.p.wait(timeout=20)
 verify();ok('Real AHCI identifies 1TiB, CLI accepts LBA beyond 128GiB, actual GUI installer writes a 96MiB native extent near disk end; primary/backup GPT CRCs and foreign entry/canaries preserved')
 with Installed('/not-attached',w,ev,'large-installed',disk='target',boot_disk=True) as g:
  g.unlock();g.key('f2');g.visible('Sessione locale');g.cmd('sw_vers -productVersion','10.2.5');g.cmd('echo HIGH_LBA_PERSISTENCE > Documents/large-proof.txt');time.sleep(.8);g.cmd('cat Documents/large-proof.txt','HIGH_LBA_PERSISTENCE');g.shot('large-UEFI-desktop');g.cmd('off');g.p.wait(timeout=20)
 with Installed('/not-attached',w,ev,'large-bios-installed',disk='target',boot_disk=True,launcher=root/'tools/run-bios-installed.sh') as g:
  g.unlock();g.key('f2');g.visible('Sessione locale');g.cmd('cat Documents/large-proof.txt','HIGH_LBA_PERSISTENCE');g.shot('large-BIOS-desktop');g.cmd('off');g.p.wait(timeout=20)
 ok('BIOS coexistence menu and OpenDuet load the high-LBA installation from the 1TiB disk without ISO; document persists across UEFI/BIOS')
 with Recovery('/not-attached',w,ev,'large-recovery' ,disk='target',boot_disk=True) as g:
  a=g.snap();assert all(a.getpixel((x,y))==(45,45,45) for x in range(544,736) for y in range(489,495));g.shot('large-recovery-clean')
  x,y=g.visible('Utility',box=(0,0,800,34));g.click(x+12,y+8);x,y=g.visible('Terminale',box=(0,34,900,250));g.click(x+20,y+8);g.visible('Sessione locale');g.cmd('diskutil verifyVolume disk0','Il volume risulta valido.');g.shot('large-recovery-firstaid');g.cmd('off');g.p.wait(timeout=20)
 verify();ok('UEFI cold boot without ISO mounts the high-LBA home; installed Recovery boots with no progress residue and verifies the native volume at the 1TiB disk tail; foreign canaries/GPT copies still valid')
 r['passed']=True
finally:
 r['allocated_fixture_bytes']=disk.stat().st_blocks*512
 (base/'large-disk-guest-result.json').write_text(json.dumps(r,indent=2)+'\n')
 if r['passed']:disk.unlink()
