#!/usr/bin/env python3
"""Inspect actual release media, not only the authored plist or vendor directory."""
from pathlib import Path
import os,json,hashlib,plistlib,subprocess,tempfile
q=Path(__file__).resolve().parent;root=q.parents[1]/'tinymacOS';iso=Path('/var/tmp/tmc-10.1.3/tinymacOS-10.1.3-x86_64.iso');tool=Path('/var/tmp/tmc/tools/root')
env={**os.environ,'PATH':str(tool/'usr/bin')+':'+str(tool/'usr/sbin')+':'+os.environ['PATH'],'LD_LIBRARY_PATH':str(tool/'usr/lib/x86_64-linux-gnu')}
lock=json.loads((root/'vendor/opencore-1.0.8/vendor-lock.json').read_text());name='Ps2KeyboardDxe.efi';digest=lock['files'][name]
assert digest=='1eb1aff3b18553be02403ff47d1588a215a73d27d9f0552cba68db9fb1441a32'
def run(c):return subprocess.run(c,env=env,check=True,capture_output=True).stdout
with tempfile.TemporaryDirectory(prefix='tmc1012-firmware-media-',dir='/var/tmp') as tmp:
 w=Path(tmp)
 for src,dst in [('/BIOS.IMG',w/'bios.img'),('/Install.esp',w/'install.esp'),('/EFI/OC',w/'root-oc')]:run(['xorriso','-osirrox','on','-indev',str(iso),'-extract',src,str(dst)])
 records=[]
 for label,img in [('optical BIOS',str(w/'bios.img')+'@@1048576'),('installed BIOS/UEFI ESP',str(w/'install.esp')),('ISO root OpenCore',None)]:
  def read(path):return run(['mtype','-i',img,'::/EFI/OC/'+path]) if img else (w/'root-oc'/path).read_bytes()
  cfg=plistlib.loads(read('config.plist'));drivers=cfg['UEFI']['Drivers'];active=[d['Path'] for d in drivers if d['Enabled']]
  assert name in active and 'OpenUsbKbDxe.efi' in active and 'XhciDxe.efi' in active
  assert 'UsbMouseDxe.efi' not in active,'Do not reintroduce discarded competing mouse driver configuration'
  data=read('Drivers/'+name);assert hashlib.sha256(data).hexdigest()==digest
  records.append({'path':label,'ps2_keyboard_enabled':True,'official_driver_sha256':digest,'competing_usb_mouse_driver_disabled':True})
 r={'passed':True,'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'release_zip_sha256':lock['release_sha256'],'driver_code_unmodified':True,'checks':records}
 (q/'firmware-contract-result.json').write_text(json.dumps(r,indent=2)+'\n');print('PASS actual ISO/BIOS image/installed ESP: official PS/2 firmware driver pinned, enabled and byte-exact; competing USB mouse driver disabled')
