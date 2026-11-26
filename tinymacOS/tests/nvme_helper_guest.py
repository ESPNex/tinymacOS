"""BIOS NVMe bootstrap acceptance gate with NVMe disabled in SeaBIOS source.
Requires the disposable installed NVMe fixture from nvme_guest.py, a helper
image, and the independently built/configured QA firmware. No physical media.
"""
from pathlib import Path
import hashlib,json,os,struct,time,sys,contextlib,io
from retail_guest import Retail,fold
from boot_recovery import RecoveryKey
import pixels,retail_guest
root=Path(__file__).resolve().parents[1];base=Path(os.environ['TMC_QA_DIR'])
encrypted='--fs2-encrypted' in sys.argv
reset='--fs2-reset' in sys.argv
fs2=encrypted or reset or '--fs2-installer' in sys.argv
old=json.loads((base/('vault2-guest-result.json' if encrypted else 'fs2-reset-guest-result.json' if reset else 'fs2-install-guest-result.json' if fs2 else 'nvme-guest-result.json')).read_text());w=Path(old['work']);ev=w/'evidence'
assert str(w).startswith('/var/tmp/tmc-fs2-install-' if fs2 else '/var/tmp/tmc-nvme-guest-') and (w/'target/system.raw').is_file()
if fs2:assert old['passed'] and (old['host_prepared_encrypted_fixture'] if encrypted else not old['host_formatted_target'])
lock=json.loads((base/'seabios-no-nvme/source-lock.json').read_text());src=Path(lock['source']);fw=src/'out/bios.bin';config=(src/'.config').read_text();assert '# CONFIG_NVME is not set' in config and 'CONFIG_NVME=y' not in config
helper=Path(os.environ.get('TMC_BOOT_HELPER',base/'nvme-boot-helper-v2.img'));meta=json.loads(helper.with_suffix('.json').read_text());before=hashlib.file_digest(helper.open('rb'),'sha256').hexdigest();assert before==meta['sha256']
font=Path(os.environ['TMC_FONT']).read_bytes();pixels.b=font;pixels.nf,pixels.ng=struct.unpack_from('<II',font,4);pixels.codes={c:j for j,c in enumerate(struct.unpack_from(f'<{pixels.ng}I',font,12))};pixels.start=12+pixels.ng*4;pixels.pix=pixels.start+pixels.nf*(pixels.ng*8+4);pixels.word.cache_clear()
locate=retail_guest.locate;retail_guest.locate=lambda im,text,faces=(0,1,2,3,5,6,8),threshold=.88:locate(im,text,faces,threshold)
os.environ.update(TMC_STORAGE='nvme',TMC_MEM='512',TMC_SMP='2',TMC_POINTER='tablet',TMC_BIOS_CODE=str(fw),OCR_ROOT=os.environ['TOOLROOT'])
r={'passed':False,'scope':'helper BIOS bootstrap, not complete 10.2.5 release acceptance','firmware_source':lock,'firmware_sha256':hashlib.file_digest(fw.open('rb'),'sha256').hexdigest(),'helper_sha256':before,'physical_hardware_tested':False,'negative_without_helper':False,'system_with_helper':False,'recovery_with_helper':False,'work':str(w)}
r['actual_fs2_installer_fixture']=fs2
if fs2:r['installed_kernel_sha256']=old['kernel_sha256']
launcher=root/'tools/run-bios-installed.sh'
r['after_logical_factory_reset']=reset
r['host_prepared_encrypted_fixture']=encrypted
class Private(Retail):
 def type(self,text):
  if not encrypted:return super().type(text)
  with contextlib.redirect_stdout(io.StringIO()):return super().type(text)
 def unlock(self,password='local-test-285'):
  if not encrypted:return super().unlock(password)
  self.type(password);self.key('ret');self.visible('Preferiti',300);time.sleep(.65)
class Installed(Private):initial='Sblocca Macintosh HD' if encrypted else 'Fs2Reset' if reset else 'Fs2Install' if fs2 else 'NvmeDisk'
class Recovery(RecoveryKey,Private):recovery_text='Ripristina da una copia locale'
class NoHelper(Retail):
 def wait(self,*args,**kwargs):
  deadline=time.monotonic()+30
  while time.monotonic()<deadline:
   if 'nobootabledevice' in fold(self.ocr(psm=6)):self.snap().save(self.ev/'bios-NVME-disabled-without-helper-no-boot.png');return
   if self.p.poll() is not None:raise AssertionError('firmware exited before negative boot proof')
   time.sleep(.3)
  raise AssertionError('No independent negative proof that this firmware cannot boot the NVMe target')
try:
 os.environ.pop('TMC_BOOT_HELPER',None)
 with NoHelper('/not-attached',w,ev,'helper-negative',disk='target',boot_disk=True,launcher=launcher) as g:
  blocks=str(g.req('query-block'));assert 'helper' not in blocks and 'cdrom' not in blocks
 r['negative_without_helper']=True
 os.environ['TMC_BOOT_HELPER']=str(helper)
 with Installed('/not-attached',w,ev,'helper-system',disk='target',boot_disk=True,launcher=launcher) as g:
  g.unlock(password='reset-test-285' if reset else 'local-test-285');g.key('f2');g.visible('Sessione locale');g.cmd('cat Documents/encrypted-proof.txt' if encrypted else 'cat Documents/reset-proof.txt' if reset else 'cat Documents/persistent.txt' if fs2 else 'cat Documents/nvme.txt','FV3_GUEST_ONLY_SECRET_30B16' if encrypted else 'AFTER_FACTORY_RESET' if reset else 'FS2_INSTALL_PERSISTENT' if fs2 else 'NATIVE_NVME_PERSISTENCE');g.cmd('diskutil info disk1','QEMU NVMe');g.shot('bios-helper-NVME-disabled-system');g.cmd('off');g.p.wait(timeout=20)
 r['system_with_helper']=True
 with Recovery('/not-attached',w,ev,'helper-recovery',disk='target',boot_disk=True,launcher=launcher) as g:
  def terminal():
   x,y=g.visible('Utility',box=(0,0,800,34));g.click(x+12,y+8);x,y=g.visible('Terminale',box=(0,34,900,250));g.click(x+20,y+8);g.visible('Sessione locale')
  terminal()
  if encrypted:
   g.cmd('fdesetup manage');x,y=g.visible('FileVault',faces=(3,));X,Y=x-28,y-22;g.click(X+200,Y+95);g.visible('Bloccato');g.click(X+140,Y+260);g.visible('Sblocca il volume');g.type('local-test-285');g.key('ret');g.visible('Attivo',300);g.key('ctrl-w');time.sleep(.5);g.key('ctrl-w');time.sleep(.5)
   # The same ordinary Recovery installer must preserve ALL ciphertext bytes.
   def data_hash():
    h=hashlib.sha256()
    with (w/'target/system.raw').open('rb') as f:
     f.seek(1024+128+32);a,b=struct.unpack('<QQ',f.read(16));f.seek(a*512);left=(b-a+1)*512
     while left:
      chunk=f.read(min(left,1<<20));assert chunk;h.update(chunk);left-=len(chunk)
    return h.hexdigest()
   before_data=data_hash();g.click(550,355);g.click(880,557);g.visible('Per reinstallare il sistema');g.key('ret');g.visible('Seleziona la destinazione');g.key('down');g.key('down');g.key('ret');g.visible('Conferma la reinstallazione');g.key('ret');g.visible('Conferma la reinstallazione');assert data_hash()==before_data;g.key('spc');g.key('ret');g.visible('Reinstallazione completata',240);assert data_hash()==before_data;g.shot('bios-vault2-reinstall-preserves-ciphertext');g.key('ret');g.visible('Ripristina da una copia locale');r['encrypted_reinstall_entire_data_partition_sha256_unchanged']=before_data
   terminal()
  g.cmd('diskutil verifyVolume disk1','Il volume risulta valido.',timeout=180);g.shot('bios-helper-NVME-disabled-recovery');g.cmd('off');g.p.wait(timeout=20)
 r['recovery_with_helper']=True
 assert hashlib.file_digest(helper.open('rb'),'sha256').hexdigest()==before;r['helper_source_unchanged']=True;r['passed']=True
except Exception as error:r['failure']=str(error);raise
finally:(base/('vault2-nvme-helper-result.json' if encrypted else 'fs2-reset-nvme-helper-result.json' if reset else 'fs2-nvme-helper-result.json' if fs2 else 'nvme-helper-complete-result.json')).write_text(json.dumps(r,indent=2)+'\n')
print('PASS BIOS without firmware NVMe: no-helper negative control, helper system boot, Recovery, immutable helper source')
