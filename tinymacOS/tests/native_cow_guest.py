"""CoW through actual kernel syscalls, BIOS/UEFI boots and native Recovery.
Only the disposable current-ISO native_install fixture is used, never /dev.
"""
from pathlib import Path
import os,json,struct,hashlib,subprocess,zlib,time
from retail_guest import Retail
from boot_recovery import RecoveryKey
import pixels,retail_guest
root=Path(__file__).resolve().parents[1];base=Path(os.environ['TMC_QA_DIR']);iso=Path(os.environ['TMC_STAGE_ISO'])
w=Path(json.loads((base/'install-result.json').read_text())['external_work']);assert str(w).startswith('/var/tmp/')
ev=w/'cow-evidence';ev.mkdir(exist_ok=True);disk=w/'target/system.raw'
font=Path(os.environ['TMC_FONT']).read_bytes();pixels.b=font;pixels.nf,pixels.ng=struct.unpack_from('<II',font,4);pixels.codes={c:j for j,c in enumerate(struct.unpack_from(f'<{pixels.ng}I',font,12))};pixels.start=12+pixels.ng*4;pixels.pix=pixels.start+pixels.nf*(pixels.ng*8+4);pixels.word.cache_clear()
old=retail_guest.locate;retail_guest.locate=lambda im,text,faces=(0,1,2,3,5,6,8),threshold=.88:old(im,text,faces,threshold)
os.environ.update(TMC_MEM='512',TMC_POINTER='tablet',TMC_SMP='2',OCR_ROOT=os.environ['TOOLROOT'])
class Installed(Retail):
 initial='BiosDisk'
 def cmd(self,s,expect=None):
  # Snapshot verification/creation can still be returning after its first
  # status line is composed. Do not lose the next command's initial key, or
  # satisfy a new assertion with an old visible command/output line.
  time.sleep(.8);self.type('clear');self.key('ret',.4);time.sleep(.3)
  self.type(s);self.key('ret',.4)
  if expect:self.visible(expect)
  time.sleep(.7)
class Recovery(RecoveryKey,Retail):recovery_text='Ripristina da una copia locale'
r={'passed':False,'work':str(w),'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'checks':[],'physical_hardware_tested':False,'windows_boot_tested':False}
def ok(s):r['checks'].append(s);print('PASS',s,flush=True)
def shell(g):g.unlock();g.key('f2');g.visible('Sessione locale')
def verify(g):
 g.cmd('cat /.snapshots/cow-one/Users/User/Documents/cow-check.txt','COW_VERSION_A')
 g.cmd('cat /.snapshots/cow-two/Users/User/Documents/cow-check.txt','COW_VERSION_B')
 g.cmd('diskutil apfs snapshot verify cow-one','Snapshot valido')
 g.cmd('diskutil apfs snapshot verify cow-two','Snapshot valido')
def off(g):g.cmd('off');g.p.wait(timeout=20)
def recovery_op(label,backup):
 with Recovery('/not-attached',w,ev,label,disk='target',boot_disk=True) as g:
  if backup:
   x,y=g.visible('Utility',box=(0,0,800,34));g.click(x+12,y+8);x,y=g.visible('Crea una copia locale',box=(0,34,900,240));g.click(x+24,y+8);g.visible('QEMU HARDDISK')
  else:g.click(550,270);g.click(880,557)
  g.visible('Confermi l’operazione');g.click(820,470);g.visible('Operazione completata.',90);g.shot(label);g.key('ret')
  x,y=g.visible('Utility',box=(0,0,800,34));g.click(x+12,y+8);x,y=g.visible('Terminale',box=(0,34,900,240));g.click(x+24,y+8);g.visible('Sessione locale');off(g)
try:
 with Installed('/not-attached',w,ev,'cow-create',disk='target',boot_disk=True) as g:
  shell(g);g.cmd('echo COW_VERSION_A > Documents/cow-check.txt');g.cmd('diskutil apfs snapshot create cow-one','Snapshot creato.')
  g.cmd('echo COW_VERSION_B > Documents/cow-check.txt');g.cmd('diskutil apfs snapshot create cow-two','Snapshot creato.')
  g.cmd('echo COW_VERSION_C > Documents/cow-check.txt');verify(g);g.cmd('cat Documents/cow-check.txt','COW_VERSION_C');g.shot('cow-distinct-generations');off(g)
 # Independent read of the actual on-disk maps: tiny shared block set, not two
 # full-volume clones. File contents above were resolved by the actual guest.
 with disk.open('rb') as f:
  f.seek((51200+137)*512);superblock=f.read(512);assert superblock[:8]==b'TMCOW03\0'
  version,frame,mapstart,stride,store,count,limit,crc=struct.unpack_from('<8I',superblock,8);raw=bytearray(superblock);struct.pack_into('<I',raw,36,0);assert version==1 and zlib.crc32(raw)==crc
  f.seek((51200+136)*512);cat=f.read(512);ids=set()
  for k in range(8):
   if cat[32+k*48:60+k*48].split(b'\0')[0] not in (b'cow-one',b'cow-two'):continue
   f.seek((51200+mapstart+k*stride)*512);m=f.read(frame*8);expected=struct.unpack_from('<I',cat,32+k*48+40)[0];assert zlib.crc32(m)==expected
   ids.update(struct.unpack_from('<I',m,j*8)[0] for j in range(frame))
  ids.discard(0);assert 0<len(ids)<256;r['independent_map_check']={'frame_sectors':frame,'unique_preserved_data_sectors':len(ids),'full_clone_sectors_avoided':2*frame}
 ok('Actual UEFI kernel creates two CoW snapshots, preserves A/B while live is C; independent on-disk maps prove fewer than 256 old-data sectors instead of two full-volume copies')
 with Installed('/not-attached',w,ev,'cow-bios',disk='target',boot_disk=True,launcher=root/'tools/run-bios-installed.sh') as g:
  shell(g);verify(g);g.cmd('mv Documents/cow-check.txt Documents/cow-renamed.txt');g.cmd('rm Documents/cow-renamed.txt');verify(g)
  g.cmd('diskutil apfs snapshot restore cow-two','Volume ripristinato.');g.cmd('cat Documents/cow-check.txt','COW_VERSION_B');verify(g);off(g)
 ok('BIOS cold boot reloads CoW maps; rename/delete do not alter snapshots, snapshot restore preserves the other historical view')
 recovery_op('cow-recovery-backup',True)
 with Installed('/not-attached',w,ev,'cow-change-after-backup',disk='target',boot_disk=True) as g:
  shell(g);g.cmd('echo COW_VERSION_D > Documents/cow-check.txt');verify(g);off(g)
 recovery_op('cow-recovery-restore',False)
 with Installed('/not-attached',w,ev,'cow-after-recovery',disk='target',boot_disk=True) as g:
  shell(g);g.cmd('cat Documents/cow-check.txt','COW_VERSION_B');verify(g)
  g.cmd('diskutil apfs snapshot delete cow-one','Snapshot eliminato.');g.cmd('cat /.snapshots/cow-two/Users/User/Documents/cow-check.txt','COW_VERSION_B');g.cmd('diskutil apfs snapshot delete cow-two','Snapshot eliminato.');g.shot('cow-after-restore-and-delete');off(g)
 ok('Real native Recovery backup/restore keeps CoW historical views and restores the backed-up live file; snapshots can then be independently deleted')
 subprocess.run(['fallocate','--dig-holes',disk],check=True)
 r['passed']=True
finally:(base/'cow-guest-result.json').write_text(json.dumps(r,indent=2)+'\n')
