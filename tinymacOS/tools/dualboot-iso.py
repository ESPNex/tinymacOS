#!/usr/bin/env python3
"""Add a real El Torito BIOS/OpenDuet/OpenCore path to a native UEFI ISO.
BIOS uses ISOLINUX/MEMDISK to expose a NEW MBR/FAT32 OpenDuet volume
as a reserved RAM disk, then enters OpenDuet/OpenCore. El Torito HDD
emulation alone is not sufficient on SeaBIOS (boot0af read failure). No physical devices.
Installed disks use the unchanged GPT/FAT16 layout plus a reserved-gap
OpenDuet MBR bridge, written and verified by the native installer.
"""
import argparse,hashlib,json,os,shutil,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('native_iso',type=Path);p.add_argument('output_iso',type=Path);a=p.parse_args()
src=a.native_iso.resolve();dst=a.output_iso.resolve()
if not src.is_file() or dst.exists() or not str(dst).startswith('/var/tmp/') or dst.is_relative_to('/home/user'):raise SystemExit('Existing input ISO and NEW output under /var/tmp required.')
tr=Path(os.environ.get('TOOLROOT','/var/tmp/tmc/tools/root'));env={**os.environ,'PATH':str(tr/'usr/bin')+':'+str(tr/'usr/sbin')+':'+os.environ['PATH'],'LD_LIBRARY_PATH':str(tr/'usr/lib/x86_64-linux-gnu'),'SOURCE_DATE_EPOCH':'1735689600'}
def run(*c):subprocess.run(list(map(str,c)),check=True,env=env)
with tempfile.TemporaryDirectory(prefix='tmc-dual-iso-',dir='/var/tmp') as work:
 w=Path(work);tree=w/'disc';run('xorriso','-osirrox','on','-indev',src,'-extract','/',tree)
 for q in [tree,*[x for x in tree.rglob('*') if x.is_dir()]]:q.chmod(q.stat().st_mode|0o700)
 for name in ['boot.catalog','BIOS.IMG']:
  q=tree/name
  if q.exists():q.unlink()
 run('python3',root/'tools/build-legacy.py','--verbose-config',src,tree/'BIOS.IMG')
 sy=root/'vendor/syslinux-6.04';sl=json.loads((sy/'vendor-lock.json').read_text())
 for n,h in sl['files'].items():assert hashlib.sha256((sy/n).read_bytes()).hexdigest()==h,n
 boot=tree/'boot/isolinux';boot.mkdir(parents=True,exist_ok=True)
 for n in ['isolinux.bin','ldlinux.c32','memdisk']:shutil.copyfile(sy/n,boot/n)
 (boot/'isolinux.cfg').write_text('DEFAULT duet\nPROMPT 0\nTIMEOUT 1\nLABEL duet\n  KERNEL /boot/isolinux/memdisk\n  INITRD /BIOS.IMG\n  APPEND harddisk raw\n')
 for n in ['COPYRIGHT.txt','GPL-2.txt','SOURCE.md']:shutil.copyfile(sy/n,tree/'LICENSES'/('Syslinux-'+n))
 # Also make OpenCore available to removable-media UEFI firmware via its
 # explicit path. Native EFI/BOOT and EFI.IMG stay byte-identical/direct UEFI.
 v=root/'vendor/opencore-1.0.8';oc=tree/'EFI/OC';(oc/'Drivers').mkdir(parents=True,exist_ok=True);native=tree/'EFI/TMC';native.mkdir(exist_ok=True)
 shutil.copyfile(tree/'EFI/BOOT/BOOTX64.EFI',native/'BOOTX64.EFI')
 shutil.copyfile(v/'OpenCore.efi',oc/'OpenCore.efi');[shutil.copyfile(v/n,oc/'Drivers'/n) for n in ['BiosVideo.efi','XhciDxe.efi','OpenUsbKbDxe.efi','UsbMouseDxe.efi']];shutil.copyfile(root/'boot/opencore.plist',oc/'config.plist')
 shutil.copyfile(v/'LICENSE.txt',tree/'LICENSES/OpenCore.txt')
 (tree/'BIOS-README.TXT').write_text('tinymacOS 10.0 Tramontana — major appearance and input maintenance release\nBIOS optical: ISOLINUX/MEMDISK RAM disk -> OpenDuet 1.0.8 BlockIO -> OpenCore -> native loader.\nUEFI: original direct EFI image/native loader; OpenCore also at EFI/OC/OpenCore.efi.\nInstalled disk: GPT/FAT16 + MBR reserved-gap OpenDuet bridge, BIOS and UEFI. No legacy USB/DD or physical hardware guarantee.\nSafari/WebKit not included.\n')
 for q in tree.rglob('*'):os.utime(q,(1735689600,1735689600))
 run('xorriso','-as','mkisofs','-r','-J','-V','TMC_10_TRAMONTANA','--modification-date=2025010100000000','--set_all_file_dates','2025010100000000','-b','boot/isolinux/isolinux.bin','-no-emul-boot','-boot-load-size','4','-boot-info-table','-eltorito-alt-boot','-e','EFI.IMG','-no-emul-boot','-o',dst,tree)
metadata={'native_iso_sha256':hashlib.file_digest(src.open('rb'),'sha256').hexdigest(),'filename':dst.name,'bytes':dst.stat().st_size,'sha256':hashlib.file_digest(dst.open('rb'),'sha256').hexdigest(),'bios':'El Torito ISOLINUX/MEMDISK RAM disk, OpenDuet BlockIO 1.0.8 -> OpenCore 1.0.8 -> native loader','uefi':'original direct El Torito EFI.IMG','bios_installer':True,'legacy_usb_dd_supported':False,'browser':False}
dst.with_suffix('.json').write_text(json.dumps(metadata,indent=2)+'\n');print(json.dumps(metadata,indent=2))
