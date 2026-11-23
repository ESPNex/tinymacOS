#!/usr/bin/env python3
"""Inspect three actual OpenCore media paths. This does not certify runtime boot or Linux port."""
import hashlib,json,os,plistlib,struct,subprocess,tempfile,shutil,sys
from pathlib import Path
q=Path(__file__).resolve().parent;root=q.parents[1]/'tinymacOS';iso=Path(os.environ.get('TMC_STAGE_ISO','/var/tmp/tmc-10.1.7/linux-config-v10/tinymacOS-10.1.7-x86_64.iso'));build=Path(os.environ.get('TMC_STAGE_BUILD',str(iso.parent/'build')));v=root/'vendor/opencore-1.0.8';w=Path(tempfile.mkdtemp(prefix='tmc1017-media-',dir='/var/tmp'));tool=Path('/var/tmp/tmc/tools/root')
env={**os.environ,'PATH':str(tool/'usr/bin')+':'+os.environ['PATH'],'LD_LIBRARY_PATH':str(tool/'usr/lib/x86_64-linux-gnu')}
sys.path.insert(0,str(root/'tools'))
from firmware_drivers import driver_sources
firmware=driver_sources(root)
def run(*c):return subprocess.check_output(list(map(str,c)),env=env,stderr=subprocess.DEVNULL)
def sha(p):return hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
r={'passed':False,'iso_sha256':sha(iso),'checks':[],'linux_hcd_port':False}
try:
 for src,name in [('/EFI.IMG','efi.img'),('/Install.esp','install.esp'),('/BIOS.IMG','bios.img')]:run('xorriso','-osirrox','on','-indev',iso,'-extract',src,w/name)
 for label,path in [('UEFI optical',str(w/'efi.img')),('installed system',str(w/'install.esp')),('BIOS OpenDuet',str(w/'bios.img')+'@@1048576')]:
  def read(name):return run('mtype','-i',path,'::/'+name)
  assert read('EFI/BOOT/BOOTX64.EFI')==(v/'BOOTx64.efi').read_bytes()
  assert read('EFI/OC/OpenCore.efi')==(v/'OpenCore.efi').read_bytes()
  assert read('EFI/TMC/BOOTX64.EFI')==(build/'BOOTX64.EFI').read_bytes()
  cfg=plistlib.loads(read('EFI/OC/config.plist'));assert cfg['NVRAM']['Add']=={};drivers={d['Path']:d for d in cfg['UEFI']['Drivers']};assert drivers['UsbMouseDxe.efi']['Enabled']
  for name,d in drivers.items():
   if d['Enabled']:assert read('EFI/OC/Drivers/'+name)==firmware[name].read_bytes()
  assert drivers['TmcPs2KeyboardDxe.efi']['Enabled'] and not cfg['UEFI']['Input']['KeySupport']
  assert read('LICENSES/TMC-Ps2Firmware-NOTICE.txt')==(root/'licenses/TMC-Ps2Firmware-NOTICE.txt').read_bytes()
  assert read('LICENSES/TMC-Ps2Firmware-BSD-2-Clause-Patent.txt')==(root/'licenses/TMC-Ps2Firmware-BSD-2-Clause-Patent.txt').read_bytes()
  config=read('BOOTCFG.BIN');assert config[:8]==b'TMCBOOT1' and struct.unpack_from('<II',config,8)==(1,0) and config[16:48]==hashlib.sha256(config[:16]).digest()
  r['checks'].append({'medium':label,'official_opencore_fallback':True,'official_opencore_binary':True,'native_loader_second_stage':True,'default_verbose':False,'firmware_mouse_driver_enabled':True,'enabled_drivers_byte_exact':True,'declared_derived_ps2_driver':True,'derived_license_and_notice_present':True});print('PASS',label,'OpenCore fallback, native second stage, quiet BootConfig; official USB + declared derived PS/2 drivers',flush=True)
 for src,expected in [('/EFI/BOOT/BOOTX64.EFI',v/'BOOTx64.efi'),('/EFI/TMC/BOOTX64.EFI',build/'BOOTX64.EFI'),('/EFI/OC/OpenCore.efi',v/'OpenCore.efi')]:
  p=w/('root-'+expected.name);run('xorriso','-osirrox','on','-indev',iso,'-extract',src,p);assert p.read_bytes()==expected.read_bytes()
 print('PASS ISO9660 root also enters OpenCore, not the native fallback',flush=True);r['iso_root_enters_opencore']=True;r['passed']=True
finally:
 (q/'media-contract-result.json').write_text(json.dumps(r,indent=2)+'\n')
 shutil.rmtree(w)
