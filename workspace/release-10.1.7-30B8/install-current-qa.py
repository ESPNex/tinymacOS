#!/usr/bin/env python3
"""BIOS live -> actual GUI installer -> BIOS installed disk with ISO removed.
Then reboot BIOS with verbose on, and cross-boot same disk with fresh OVMF.
Every disk is a disposable external file; never a physical/user-owned device.
"""
import hashlib,json,os,struct,sys,tempfile,time,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2];sys.path.insert(0,str(root/'tinymacOS/tests'))
from retail_guest import Retail
from riviera_media import gpt
iso=Path(os.environ.get('TMC_INSTALL_ISO','/var/tmp/tmc-10.1.7/linux-hid-v12/tinymacOS-10.1.7-x86_64.iso'))
w=Path(tempfile.mkdtemp(prefix='tmc-bios-install-qa-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
os.environ.update(TOOLROOT='/var/tmp/tmc/tools/root',TMC_BUILD=os.environ.get("TMC_STAGE_BUILD",str(iso.parent/"build")),TMC_MEM="512",TMC_BIOS_ISO=str(iso),TMC_POINTER='mouse',TMC_SMP='2')
launcher=root/'tinymacOS/tools/run-bios-installed.sh';report={'passed':False,'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'external_work':str(w),'physical_hardware_tested':False,'full_linux_input_port':False,'scope':'actual BIOS GUI installation, HDD-only BIOS boot/reboot and UEFI cross-boot in QEMU'}
def no_iso(g):
 blocks=g.req('query-block');assert any(str(w/'target/system.raw') in str(b) for b in blocks),blocks;assert not any('cd'==b.get('device') or str(iso) in str(b) for b in blocks),blocks
 assert '-pflash' not in g.p.args
 return blocks
class Installed(Retail):initial='BiosDisk'
try:
 with Retail(iso,w,ev,'bios-install',disk='target',live=False,launcher=launcher) as g:
  g.enroll(name='BiosDisk');g.key('f11');g.visible('Introduzione');g.key('ret');g.key('down');g.key('ret');g.visible('Conferma l’installazione');g.key('ret');g.key('spc');g.key('ret');g.visible('Il disco è pronto',150,faces=(3,));g.shot('bios-install-complete');g.key('ret');g.p.wait(timeout=15);assert g.p.returncode==0
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
  no_iso(g);g.unlock();g.key('f2');g.visible('Sessione locale');g.cmd('sw_vers','ProductVersion:    10.1.7');g.cmd('bootverbose log','[boot] CPUs online: 2');g.cmd('echo BIOS_DISK_PERSISTENT > Documents/bios-proof.txt');g.cmd('cat Documents/bios-proof.txt','BIOS_DISK_PERSISTENT');g.cmd('bootverbose on','Preferenza verificata su disco');g.shot('bios-hdd-desktop');g.cmd('off');g.p.wait(timeout=15);assert g.p.returncode==0
 with Installed('/not-attached',w,ev,'bios-reboot',disk='target',boot_disk=True,launcher=launcher) as g:
  no_iso(g);g.unlock();g.key('f2');g.visible('Sessione locale');g.cmd('cat Documents/bios-proof.txt','BIOS_DISK_PERSISTENT');g.cmd('bootverbose status','Verbose avvio corrente: attivo');g.shot('bios-reboot-persistence-verbose');g.cmd('bootverbose off','Preferenza verificata su disco');g.cmd('off');g.p.wait(timeout=15);assert g.p.returncode==0
 # No previous OVMF state exists: this firmware must discover the native GPT ESP.
 with Installed('/not-attached',w,ev,'uefi-crossboot',disk='target',boot_disk=True) as g:
  g.unlock();g.key('f2');g.visible('Sessione locale');g.cmd('cat Documents/bios-proof.txt','BIOS_DISK_PERSISTENT');g.cmd('bootverbose status','Verbose avvio corrente: disattivo');g.shot('uefi-same-installed-disk');g.cmd('off');g.p.wait(timeout=15);assert g.p.returncode==0
 report.update(passed=True,bios_live_installer=True,bios_hdd_only_login=True,bios_cold_reboot=True,persistent_document=True,bios_verbose_setting_persisted=True,same_disk_native_uefi_boot=True,gpt_primary_backup_verified=True,mbr_bridge_readback_verified=True,official_duet_reserved_gap_verified=True,installed_cpus_online=2,cd_removed=True)
finally:(Path(__file__).parent/'install-current-result.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS BIOS live -> GUI install -> remove ISO -> BIOS HDD login -> persistent document -> BIOS reboot verbose -> same disk UEFI login; evidence',ev)
