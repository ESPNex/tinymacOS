#!/usr/bin/env python3
"""Create a NEW, disposable raw BIOS/UEFI NVMe bootstrap helper image.
No operating-system payload, no physical-device writes, no existing output.
Required on BIOS with no NVMe firmware/Option ROM: BIOS must fetch the initial
code from USB/SATA, then OpenDuet/OpenCore loads the source-built NVMe DXE and
finds the installed native loader on NVMe. Runtime uses the kernel NVMe driver.
"""
import argparse,hashlib,json,os,shutil,struct,subprocess,tempfile
from pathlib import Path
from firmware_drivers import stage
root=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('output',type=Path);ap.add_argument('--build',type=Path,required=True);a=ap.parse_args()
out=a.output.absolute()
if out.exists() or not str(out).startswith('/var/tmp/') or out.is_relative_to('/home/user') or not out.parent.is_dir():raise SystemExit('NEW output file under /var/tmp required; never a device or existing file')
tr=Path(os.environ.get('TOOLROOT','/var/tmp/tmc/tools/root'));env={**os.environ,'PATH':str(tr/'usr/bin')+':'+str(tr/'usr/sbin')+':'+os.environ['PATH'],'LD_LIBRARY_PATH':str(tr/'usr/lib/x86_64-linux-gnu')}
def run(*args):subprocess.run(list(map(str,args)),env=env,check=True,stdout=subprocess.DEVNULL)
run('python3',root/'tools/bios-image.py',a.build)
v=root/'vendor/opencore-1.0.8';lock=json.loads((v/'vendor-lock.json').read_text())
for name,digest in lock['files'].items():assert hashlib.sha256((v/name).read_bytes()).hexdigest()==digest
bridge=(a.build/'bios-mbr.bin').read_bytes();assert len(bridge)==440
# bios-image.py pins this exact, sector-padded official DUET image.
duet=(v/'bootX64').read_bytes()
assert duet[:(v/'bootX64').stat().st_size]==(v/'bootX64').read_bytes()
assert len(duet)%512==0 and 64+len(duet)//512<2048
with tempfile.TemporaryDirectory(prefix='tmc-nvme-helper-',dir='/var/tmp') as scratch:
 w=Path(scratch);tree=w/'tree';oc=tree/'EFI/OC';(oc/'Drivers').mkdir(parents=True);(tree/'EFI/BOOT').mkdir()
 (tree/'EFI/BOOT/.contentVisibility').write_text('Disabled\n');(oc/'.contentVisibility').write_text('Disabled\n')
 shutil.copyfile(v/'BOOTx64.efi',tree/'EFI/BOOT/BOOTX64.EFI');shutil.copyfile(v/'OpenCore.efi',oc/'OpenCore.efi');stage(oc/'Drivers');shutil.copyfile(root/'boot/opencore.plist',oc/'config.plist')
 licenses=tree/'LICENSES';licenses.mkdir()
 for name in ['LICENSE.txt']:shutil.copyfile(v/name,licenses/('OpenCore-'+name))
 shutil.copyfile(root/'vendor/tmc-nvme-dxe/LICENSE.txt',licenses/'NvmExpressDxe.txt');shutil.copyfile(root/'vendor/tmc-ps2-keymap/LICENSE.txt',licenses/'TmcPs2.txt')
 (tree/'README.TXT').write_text('tinymacOS 10.2.5 development NVMe boot helper\nKeep this USB/SATA helper attached when BIOS cannot read NVMe.\nNo kernel or user documents reside on this helper. Select the installed NVMe system in OpenCore.\nRequires actual BIOS USB/SATA boot support. Does not modify or flash system firmware.\nSource-built BSD NVMe DXE supplies pre-OS BlockIO only; the native kernel owns runtime queues.\nNot a standalone operating system or a claim of physical hardware certification.\n')
 fat=w/'helper.fat';sectors=32768
 with fat.open('xb') as f:f.truncate(sectors*512)
 run('mkfs.fat','--invariant','-F16','-s1','-h2048','-n','TMCNVMEBOOT',fat)
 for p in tree.iterdir():run('mcopy','-s','-i',fat,p,'::/')
 mbr=bytearray(512);mbr[:440]=bridge;mbr[440:444]=b'TNV2';mbr[446:462]=struct.pack('<B3sB3sII',128,b'\xfe\xff\xff',6,b'\xfe\xff\xff',2048,sectors);mbr[510:]=b'\x55\xaa'
 with out.open('xb') as f:
  f.truncate((2048+sectors)*512);f.write(mbr);f.seek(64*512);f.write(duet);f.seek(2048*512)
  with fat.open('rb') as p:shutil.copyfileobj(p,f)
report={'scope':'development NVMe bootstrap helper, not complete release acceptance','filename':out.name,'bytes':out.stat().st_size,'sha256':hashlib.file_digest(out.open('rb'),'sha256').hexdigest(),'requires_bios_readable_usb_or_sata':True,'firmware_flashing':False,'contains_os_payload':False,'physical_hardware_tested':False,'dxe_sha256':json.loads((root/'vendor/tmc-nvme-dxe/build-lock.json').read_text())['binary_sha256']}
out.with_suffix('.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
