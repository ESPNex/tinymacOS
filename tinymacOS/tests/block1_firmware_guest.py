"""Installed OpenCore tools on an ALREADY GUI-created disposable test disk.
Real virtual keyboard, screen and independent NVRAM readback. Not Windows or
physical GeForce acceptance. Refuses arbitrary input/disk paths.
"""
import json,os,re,struct,time,uuid
from pathlib import Path
from retail_guest import Retail
from uefi_variables import vars_active
qa=Path(os.environ['TMC_QA_DIR']);previous=json.loads((qa/'fs2-install-guest-result.json').read_text());assert previous['passed']
w=Path(previous['work']);assert str(w).startswith('/var/tmp/tmc-fs2-install-');ev=w/'evidence'
globalguid=uuid.UUID('8be4df61-93ca-11d2-aa0d-00e098032b8c').bytes_le
vendor=uuid.UUID('3f9a25c4-71b2-4e83-a4c1-5e9d278b6c10').bytes_le
before=vars_active(w/'target/vars.fd');original=before[(vendor,'tinymacOS-previousBootOrder')];assert original.startswith(b'TMCBOOT2')
restorable=b''.join(original[i:i+2] for i in range(8,len(original),2) if (globalguid,f'Boot{struct.unpack_from("<H",original,i)[0]:04X}') in before)
assert restorable or len(original)==8
order=before[(globalguid,'BootOrder')];first=struct.unpack_from('<H',order)[0];option=before[(globalguid,f'Boot{first:04X}')]
assert 'tinymacOS OpenCore'.encode('utf-16le') in option and '\\EFI\\OC\\OpenCore.efi'.encode('utf-16le') in option
r={'passed':False,'physical_hardware_tested':False,'windows_boot_tested':False,'iso_sha256':previous['iso_sha256'],'work':str(w),'checks':['Actual installed NVRAM first entry is dedicated OpenCore, original BootOrder backup retained']}
def ok(t):r['checks'].append(t);print('PASS',t,flush=True)
class Firmware(Retail):
 def wait(self,*a,**k):
  end=time.monotonic()+40
  while time.monotonic()<end:
   text=self.ocr(psm=6)
   if 'OpenCore Boot Menu' in text:self.key('down',dt=.05,hold=.025)
   if 'Diagnostica video UEFI' in text and 'Ripristina ordine di avvio' in text:self.menu=text;print(text,flush=True);return
  raise AssertionError('Installed OpenCore maintenance entries unavailable: '+self.ocr(psm=6))
 def choose(self,name):
  for line in self.menu.splitlines():
   if name in line:
    match=re.search(r'(\d+)\.',line);assert match,line
    self.key(match[1]);return
  raise AssertionError(name+' not in menu')
 def text_wait(self,needle,timeout=20):
  end=time.monotonic()+timeout
  while time.monotonic()<end:
   text=self.ocr(psm=6)
   if needle.lower() in text.lower():print(text,flush=True);return text
   time.sleep(.15)
  raise AssertionError(needle+': '+text)
try:
 os.environ.update(TMC_STORAGE='nvme',TMC_MEM='512',TMC_SMP='2',TMC_POINTER='tablet')
 with Firmware('/not-attached',w,ev,'firmware-video',disk='target',boot_disk=True) as g:
  g.choose('Diagnostica video UEFI');g.text_wait('Diagnostica video UEFI');g.shot('uefi-video-diagnostic')
  # Enumeration pauses every 14 modes; this tool never sets a GOP mode.
  for page in range(12):
   text=g.ocr(psm=6)
   if 'tornare a OpenCore' in text:break
   if 'Premi un tasto' in text:g.key('spc')
   time.sleep(.15)
  else:raise AssertionError('Video diagnostic did not finish enumerating')
  g.shot('uefi-video-diagnostic-final')
 ok('Installed OpenCore launches real read-only GOP/EDID diagnostic, lists firmware modes and returns to user prompt')
 after=vars_active(w/'target/vars.fd');assert after[(globalguid,'BootOrder')]==order and after[(vendor,'tinymacOS-previousBootOrder')]==original
 with Firmware('/not-attached',w,ev,'firmware-restore',disk='target',boot_disk=True) as g:
  g.choose('Ripristina ordine di avvio');g.text_wait('Ripristino ordine di avvio');g.key('r');g.text_wait('Operazione verificata');g.shot('uefi-original-boot-order-restored')
 after=vars_active(w/'target/vars.fd');assert after[(globalguid,'BootOrder')]==restorable and after[(vendor,'tinymacOS-default-disabled')]==struct.pack('<I',1)
 assert after[(globalguid,f'Boot{first:04X}')]==option and after[(vendor,'tinymacOS-previousBootOrder')]==original
 ok('User-confirmed real EFI tool restores original order of surviving devices (removed installer media skipped), retains OpenCore option/backup and persists auto-promotion disable flag')
 r['passed']=True
except Exception as e:r['failure']=str(e);raise
finally:(qa/'block1-firmware-guest-result.json').write_text(json.dumps(r,indent=2)+'\n')
