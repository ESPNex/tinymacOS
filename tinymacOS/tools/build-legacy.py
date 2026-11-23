#!/usr/bin/env python3
"""Build a NEW disposable BIOS/OpenDuet+OpenCore bootstrap disk from a built ISO.
Never writes a device or an existing file. Outputs stay outside /home/user.
This is not a BIOS installation routine for existing disks.
"""
import argparse,hashlib,json,os,shutil,struct,subprocess,tempfile
from pathlib import Path
from firmware_drivers import stage as stage_firmware_drivers
r=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser();ap.add_argument('iso',type=Path);ap.add_argument('output',type=Path);ap.add_argument('--quiet',action='store_true');ap.add_argument('--verbose-config',action='store_true');a=ap.parse_args()
iso=a.iso.resolve();output=a.output.resolve()
if output.is_relative_to('/home/user') or output.exists() or not str(output).startswith('/var/tmp/'):
 raise SystemExit('Output must be a NEW regular file under /var/tmp; never a device/existing file.')
v=r/'vendor/opencore-1.0.8';lock=json.loads((v/'vendor-lock.json').read_text())
for n,h in lock['files'].items():assert hashlib.sha256((v/n).read_bytes()).hexdigest()==h,n
tr=Path(os.environ.get('TOOLROOT','/var/tmp/tmc/tools/root'));env={**os.environ,'PATH':str(tr/'usr/bin')+':'+str(tr/'usr/sbin')+':'+os.environ['PATH'],'LD_LIBRARY_PATH':str(tr/'usr/lib/x86_64-linux-gnu')}
def run(*cmd):subprocess.run(list(map(str,cmd)),check=True,env=env,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
output.parent.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory(prefix='tmc-legacy-',dir='/var/tmp') as scratch:
 w=Path(scratch);root=w/'root';run('xorriso','-osirrox','on','-indev',iso,'-extract','/',root)
 for directory in [root,*[p for p in root.rglob('*') if p.is_dir()]]:directory.chmod(directory.stat().st_mode|0o700)
 for file in root.rglob('*'):
  if file.is_file():file.chmod(file.stat().st_mode|0o600)
 # The El Torito EFI image is NOT recursively copied inside another FAT image.
 for name in ['EFI.IMG','LEGACY.IMG','boot.catalog']:
  p=root/name
  if p.exists():p.unlink()
 oc=root/'EFI/OC';(oc/'Drivers').mkdir(parents=True,exist_ok=True)
 for name in ['OpenCore.efi'] :shutil.copyfile(v/name,oc/name)
 stage_firmware_drivers(oc/'Drivers')
 shutil.copyfile(r/'boot/opencore.plist',oc/'config.plist')
 # Both OpenDuet BDS and removable UEFI entry MUST enter OpenCore, never bypass
 # it through the native fallback. Native loader lives at a distinct path.
 native=root/'EFI/TMC';native.mkdir(exist_ok=True)
 assert (native/'BOOTX64.EFI').is_file(),'Input must already contain the native second stage under EFI/TMC'
 assert (root/'EFI/BOOT/BOOTX64.EFI').read_bytes()==(v/'BOOTx64.efi').read_bytes(),'OpenCore fallback required'
 if a.quiet:
  import plistlib
  p=oc/'config.plist';d=plistlib.loads(p.read_bytes());d['NVRAM']['Add']={};p.write_bytes(plistlib.dumps(d,sort_keys=False))
 if a.verbose_config:
  q=root/'BOOTCFG.BIN';q.unlink(missing_ok=True);head=struct.pack('<8sII',b'TMCBOOT1',1,1);q.write_bytes(head+hashlib.sha256(head).digest()+bytes(464))
 # Keep MEMDISK's BIOS Block I/O, but restore the matching upstream USB bus
 # modules omitted by the Block I/O-only firmware. No module code is patched.
 subprocess.run(['python3',str(r/'tools/openduet-input.py'),str(root/'boot')],check=True)
 (root/'LICENSES').mkdir(exist_ok=True);shutil.copyfile(v/'LICENSE.txt',root/'LICENSES/OpenCore.txt')
 fat=w/'esp.fat';part_sectors=196608 # 96 MiB FAT32, clean bootstrap volume
 with fat.open('xb') as f:f.truncate(part_sectors*512)
 run('mkfs.fat','--invariant','-F32','-s1','-R32','-h2048','-n','TMCLEGACY',fat)
 for p in root.iterdir():run('mcopy','-s','-i',fat,p,'::/')
 # Preserve BPB; replace only PBR bootstrap code, as upstream BootInstall does.
 with fat.open('r+b') as f:
  bpb=f.read(512);pbr=bytearray((v/'boot1f32').read_bytes());assert len(pbr)==512
  pbr[3:90]=bpb[3:90];pbr[496:510]=hashlib.sha256((oc/'config.plist').read_bytes()).digest()[:14]
  f.seek(0);f.write(pbr)
 mbr=bytearray(512);boot0=(v/'boot0').read_bytes()[:446];assert len(boot0)==446
 mbr[:len(boot0)]=boot0
 # Active FAT32 LBA partition; CHS saturated, BIOS stage uses extended reads.
 mbr[446:462]=struct.pack('<B3sB3sII',128,b'\xfe\xff\xff',12,b'\xfe\xff\xff',2048,part_sectors)
 mbr[440:444]=struct.pack('<I',0x544D4331)
 mbr[510:]=b'\x55\xaa'
 with output.open('xb') as f:
  f.truncate((part_sectors+2048)*512);f.write(mbr);f.seek(2048*512)
  with fat.open('rb') as src:shutil.copyfileobj(src,f,1024*1024)
print('QA BIOS bootstrap disk:',output)
print('OpenDuet BlockIO -> OpenCore 1.0.8 -> native tinymacOS loader; not Safari acceptance.')
