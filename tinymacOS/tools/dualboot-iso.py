#!/usr/bin/env python3
"""Add a real El Torito BIOS/OpenDuet/OpenCore path to a native UEFI ISO.
BIOS uses El Torito hard-disk emulation of a NEW self-contained MBR/FAT32
OpenDuet volume, not merely EFI files copied to the ISO. No physical devices.
The installed OS layout remains UEFI; no BIOS installation claim is made.
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
 # Also make OpenCore available to removable-media UEFI firmware via its
 # explicit path. Native EFI/BOOT and EFI.IMG stay byte-identical/direct UEFI.
 v=root/'vendor/opencore-1.0.8';oc=tree/'EFI/OC';(oc/'Drivers').mkdir(parents=True,exist_ok=True);native=tree/'EFI/TMC';native.mkdir(exist_ok=True)
 shutil.copyfile(tree/'EFI/BOOT/BOOTX64.EFI',native/'BOOTX64.EFI')
 shutil.copyfile(v/'OpenCore.efi',oc/'OpenCore.efi');shutil.copyfile(v/'BiosVideo.efi',oc/'Drivers/BiosVideo.efi');shutil.copyfile(root/'boot/opencore.plist',oc/'config.plist')
 shutil.copyfile(v/'LICENSE.txt',tree/'LICENSES/OpenCore.txt')
 (tree/'BIOS-README.TXT').write_text('tinymacOS 2.4.4 Levante — OpenCore media revision 1\nBIOS optical: El Torito HDD emulation -> OpenDuet 1.0.8 BlockIO -> OpenCore -> native loader.\nUEFI: original direct EFI image/native loader; OpenCore also at EFI/OC/OpenCore.efi.\nInstaller/installed disk remain UEFI. No legacy USB/DD or physical hardware guarantee.\nSafari/WebKit not included.\n')
 for q in tree.rglob('*'):os.utime(q,(1735689600,1735689600))
 run('xorriso','-as','mkisofs','-r','-J','-V','TMC_2_4_4_OC1','--modification-date=2025010100000000','--set_all_file_dates','2025010100000000','-b','BIOS.IMG','-hard-disk-boot','-eltorito-alt-boot','-e','EFI.IMG','-no-emul-boot','-o',dst,tree)
metadata={'native_iso_sha256':hashlib.file_digest(src.open('rb'),'sha256').hexdigest(),'filename':dst.name,'bytes':dst.stat().st_size,'sha256':hashlib.file_digest(dst.open('rb'),'sha256').hexdigest(),'bios':'El Torito hard-disk emulation, OpenDuet BlockIO 1.0.8 -> OpenCore 1.0.8 -> native loader','uefi':'original direct El Torito EFI.IMG','bios_installer':False,'legacy_usb_dd_supported':False,'browser':False}
dst.with_suffix('.json').write_text(json.dumps(metadata,indent=2)+'\n');print(json.dumps(metadata,indent=2))
