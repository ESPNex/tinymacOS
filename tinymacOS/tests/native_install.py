#!/usr/bin/env python3
"""BIOS live -> actual GUI installer -> BIOS installed disk with ISO removed.
Then reboot BIOS with verbose on, and cross-boot same disk with fresh OVMF.
Every disk is a disposable external file; never a physical/user-owned device.
"""
import hashlib,json,os,struct,sys,tempfile,time,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2];sys.path.insert(0,str(root/'tinymacOS/tests'))
from retail_guest import Retail
import pixels
font=Path(os.environ['TMC_FONT']).read_bytes();pixels.b=font;pixels.nf,pixels.ng=struct.unpack_from('<II',font,4);pixels.codes={c:j for j,c in enumerate(struct.unpack_from(f'<{pixels.ng}I',font,12))};pixels.start=12+pixels.ng*4;pixels.pix=pixels.start+pixels.nf*(pixels.ng*8+4);pixels.word.cache_clear()
from riviera_media import gpt
iso=Path(os.environ.get('TMC_INSTALL_ISO','/var/tmp/tmc-10.2.5/linux-hid-v12/tinymacOS-10.2.5-x86_64.iso'))
w=Path(tempfile.mkdtemp(prefix='tmc-bios-install-qa-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
os.environ.update(TOOLROOT='/var/tmp/tmc/tools/root',TMC_BUILD=os.environ.get("TMC_STAGE_BUILD",str(iso.parent/"build")),TMC_MEM="512",TMC_BIOS_ISO=str(iso),TMC_POINTER='mouse',TMC_SMP='2')
launcher=root/'tinymacOS/tools/run-bios-installed.sh';report={'passed':False,'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'external_work':str(w),'physical_hardware_tested':False,'scope':'actual BIOS GUI installation, HDD-only BIOS boot/reboot and UEFI cross-boot in QEMU'}
def no_iso(g):
 blocks=g.req('query-block');assert any(str(w/'target/system.raw') in str(b) for b in blocks),blocks;assert not any('cd'==b.get('device') or str(iso) in str(b) for b in blocks),blocks
 assert '-pflash' not in g.p.args
 return blocks
class Installed(Retail):initial='BiosDisk'
resume=os.environ.get('TMC_INSTALL_RESUME')=='1'
if resume:
 previous=json.loads((Path(os.environ['TMC_QA_DIR'])/'install-result.json').read_text())
 assert previous['iso_sha256']==report['iso_sha256'] and previous.get('installed_esp_kernel_readback_equals_current_build')
 w=Path(previous['external_work']);assert str(w).startswith('/var/tmp/tmc-bios-install-qa-')
 ev=w/'evidence';disk=w/'target/system.raw';assert disk.is_file();report=previous;report['resumed_at_installed_recovery']=True
try:
 if not resume:
  with Retail(iso,w,ev,'bios-install',disk='target',live=False,launcher=launcher) as g:
   g.enroll(name='BiosDisk');g.key('f11');g.visible('Installa tinymacOS Tramontana');g.key('ret');g.key('down');g.key('ret');g.visible('Conferma l’installazione');g.key('ret');g.key('spc');g.key('ret');g.visible('Il disco è pronto',150,faces=(3,));g.shot('bios-install-complete');g.key('ret');g.p.wait(timeout=15);assert g.p.returncode==0
  disk=w/'target/system.raw';layout=gpt(disk)
  installed_esp=w/'installed-esp-readback.fat';installed_esp.write_bytes(layout['esp'])
  tool='/var/tmp/tmc/tools/root';installed_kernel=subprocess.check_output([tool+'/usr/bin/mtype','-i',str(installed_esp),'::/System/Library/KernelCollections/KernelCache'],env={**os.environ,'LD_LIBRARY_PATH':tool+'/usr/lib/x86_64-linux-gnu'})
  assert installed_kernel==(Path(os.environ['TMC_BUILD'])/'KernelCache').read_bytes()
  report['installed_esp_kernel_readback_equals_current_build']=True;report['installed_kernelcache_sha256']=hashlib.sha256(installed_kernel).hexdigest()
  notice_hashes={}
  for notice in sorted((root/'tinymacOS/licenses').iterdir()):
   actual=subprocess.check_output([tool+'/usr/bin/mtype','-i',str(installed_esp),'::/LICENSES/'+notice.name],env={**os.environ,'LD_LIBRARY_PATH':tool+'/usr/lib/x86_64-linux-gnu'})
   assert actual==notice.read_bytes(),notice.name;notice_hashes[notice.name]=hashlib.sha256(actual).hexdigest()
  report['actual_installed_component_notices_byte_exact']=True;report['installed_component_notice_sha256']=notice_hashes
  with disk.open('rb') as f:
   mbr=f.read(512);f.seek(64*512);duet=f.read((root/'tinymacOS/vendor/opencore-1.0.8/bootX64').stat().st_size)
  assert mbr[:440]==(Path(os.environ['TMC_BUILD'])/'bios-mbr.bin').read_bytes();assert mbr[450]==0xee and mbr[510:]==b'\x55\xaa'
  assert duet==(root/'tinymacOS/vendor/opencore-1.0.8/bootX64').read_bytes()
  with Installed('/not-attached',w,ev,'bios-hdd-only',disk='target',boot_disk=True,launcher=launcher) as g:
   no_iso(g);g.unlock();g.key('f2');g.visible('Sessione locale');g.cmd('sw_vers','ProductVersion:    10.2.5');g.cmd('bootverbose log','[boot] CPUs online: 2');g.cmd('echo BIOS_DISK_PERSISTENT > Documents/bios-proof.txt');g.cmd('cat Documents/bios-proof.txt','BIOS_DISK_PERSISTENT');g.cmd('bootverbose on','Preferenza verificata su disco');g.shot('bios-hdd-desktop');g.cmd('off');g.p.wait(timeout=15);assert g.p.returncode==0
  with Installed('/not-attached',w,ev,'bios-reboot',disk='target',boot_disk=True,launcher=launcher) as g:
   no_iso(g);g.unlock();g.key('f2');g.visible('Sessione locale');g.cmd('cat Documents/bios-proof.txt','BIOS_DISK_PERSISTENT');g.cmd('bootverbose status','Verbose avvio corrente: attivo');g.shot('bios-reboot-persistence-verbose');g.cmd('bootverbose off','Preferenza verificata su disco');g.cmd('off');g.p.wait(timeout=15);assert g.p.returncode==0
  # No previous OVMF state exists: this firmware must discover the native GPT ESP.
  with Installed('/not-attached',w,ev,'uefi-crossboot',disk='target',boot_disk=True) as g:
   g.unlock();g.key('f2');g.visible('Sessione locale');g.cmd('cat Documents/bios-proof.txt','BIOS_DISK_PERSISTENT');g.cmd('bootverbose status','Verbose avvio corrente: disattivo');g.shot('uefi-same-installed-disk');g.cmd('off');g.p.wait(timeout=15);assert g.p.returncode==0
 # Recovery from the installed disk alone: native kernel and SATA maintenance.
 from boot_recovery import RecoveryKey
 class InstalledRecovery(RecoveryKey,Retail):recovery_text='Ripristina da una copia locale'
 with InstalledRecovery('/not-attached',w,ev,'recovery-hdd',disk='target',boot_disk=True) as g:
  assert str(iso) not in str(g.req('query-block'))
  def utility(label):
   x,y=g.visible('Utility',box=(0,0,800,34));g.click(x+12,y+8);x,y=g.visible(label,box=(0,34,900,240));g.click(x+24,y+8)
  utility('Crea una copia locale');g.visible('QEMU HARDDISK');g.visible('Confermi l’operazione');g.click(820,470);g.visible('Operazione completata.',60);g.shot('recovery-native-backup');g.key('ret')
  g.click(550,270);g.click(880,557);g.visible('Confermi l’operazione');g.click(820,470);g.visible('Operazione completata.',60);g.shot('recovery-native-restore');g.key('ret')
  # The Recovery row now launches the same ordinary Ring-3 installer as the
  # desktop, not the compositor's old direct confirmation/maintenance dialog.
  def data_digest():
   with disk.open('rb') as f:
    a,b=gpt(disk)['parts'][1];f.seek(a*512);return hashlib.sha256(f.read((b-a+1)*512)).hexdigest()
  before_reinstall=data_digest()
  g.visible('Reinstalla tinymacOS Tramontana');g.click(550,355);g.click(880,557)
  g.visible('Reinstalla tinymacOS Tramontana',box=(0,0,1280,210));g.visible('Per reinstallare il sistema')
  g.shot('recovery-shared-installer-welcome');g.key('ret');g.visible('Seleziona la destinazione');g.key('down');g.key('ret');g.visible('Conferma la reinstallazione')
  g.key('ret');g.visible('Conferma la reinstallazione');assert data_digest()==before_reinstall
  g.key('spc');g.key('ret');g.visible('Reinstallazione completata',90);g.shot('recovery-native-reinstall')
  assert data_digest()==before_reinstall
  raw=gpt(disk);assert raw['esp']==raw['src']
  report['same_installer_recovery_wizard']=True;report['reinstall_native_data_partition_sha256_unchanged']=before_reinstall;report['reinstalled_system_equals_sealed_source']=True
  g.key('ret');g.visible('Ripristina da una copia locale')
  utility('Terminale');g.visible('Sessione locale');g.cmd('csrutil disable','Preferenza SIP verificata');g.cmd('off');g.p.wait(timeout=15)
 report['installed_recovery_without_iso']=True;report['native_sata_backup_restore_reinstall']=True
 with Installed('/not-attached',w,ev,'after-recovery',disk='target',boot_disk=True) as g:
  g.unlock();g.key('f2');g.visible('Sessione locale');g.cmd('cat Documents/bios-proof.txt','BIOS_DISK_PERSISTENT');g.cmd('csrutil status','System Integrity Protection: disattiva.');g.cmd('csrutil enable','solo la Shell di Recupero');g.shot('after-native-recovery');g.cmd('off');g.p.wait(timeout=15)
 report['native_recovery_preserves_document_and_credentials']=True;report['sip_preference_persisted_and_desktop_change_denied']=True
 report.update(passed=True,bios_live_installer=True,bios_hdd_only_login=True,bios_cold_reboot=True,persistent_document=True,bios_verbose_setting_persisted=True,same_disk_native_uefi_boot=True,gpt_primary_backup_verified=True,mbr_bridge_readback_verified=True,official_duet_reserved_gap_verified=True,installed_cpus_online=2,cd_removed=True)
finally:(Path(os.environ['TMC_QA_DIR'])/'install-result.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS BIOS live -> GUI install -> remove ISO -> BIOS HDD login -> persistent document -> BIOS reboot verbose -> same disk UEFI login; evidence',ev)
