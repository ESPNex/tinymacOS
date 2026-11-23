#!/usr/bin/env python3
"""Execute host contracts and inspect the actual production Recovery object.
The four independent guest receipts remain the runtime integration evidence.
"""
import hashlib,json,os,plistlib,re,struct,subprocess,sys,tempfile
from pathlib import Path
q=Path(__file__).resolve().parent;root=q.parents[1]/'tinymacOS'
base=Path(os.environ.get('TMC_FIRMWARE_STAGE','/var/tmp/tmc-10.1.7/linux-mouse-v7'))
build=base/'build';iso=base/'tinymacOS-10.1.7-x86_64.iso'
tool=Path('/var/tmp/tmc/tools/root');w=Path(tempfile.mkdtemp(prefix='tmc-fw-kbd-host-',dir='/var/tmp'))
def sha(p):return hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
sys.path.insert(0,str(root/'tools'))
from firmware_drivers import driver_sources
r={'passed':False,'physical_hardware_tested':False,'iso_sha256':sha(iso),'work':str(w),'tests':[]}
try:
 sources=driver_sources(root)
 cfg=plistlib.loads((root/'boot/opencore.plist').read_bytes())
 assert not cfg['UEFI']['Input']['KeySupport']
 assert {x['Path'] for x in cfg['UEFI']['Drivers'] if x['Enabled']}==set(sources)
 flags=['gcc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie','-I'+str(root/'include')]
 for name in ['firmware_keyboard','firmware_keyboard_transport']:
  exe=w/name;args=flags[:]
  if name.endswith('_transport'):args+=['-fshort-wchar','-DGNU_EFI_USE_MS_ABI','-I'+str(tool/'usr/include/efi'),'-I'+str(tool/'usr/include/efi/x86_64')]
  subprocess.run(args+[str(root/'tests'/f'{name}.c'),'-o',str(exe)],check=True)
  result=subprocess.check_output([str(exe)],text=True);print(result,end='');r['tests'].append({'name':name,'asan_ubsan':True,'result':result.strip()})
 obj=build/'rec-keyboard.c.o';assembly=subprocess.check_output(['objdump','-d',str(obj)],text=True)
 assert not re.search(r'\s(?:in|out)[bwl]?\s',assembly)
 source=(root/'recovery/keyboard.c').read_text()
 assert '../drivers/' not in source and 'KUsb' not in source and 'keyboard_byte' not in source
 guid=struct.pack('<IHH8B',0xe2dbc617,0xb651,0x4ab7,0xb3,0xa8,0x7f,0x28,0xc6,0xb1,0x91,0x40)
 assert guid in sources['TmcPs2KeyboardDxe.efi'].read_bytes()
 files=['recovery/keyboard.c','recovery/firmware_keyboard_decode.h','include/keyboard_it.h','include/base.h','tests/firmware_keyboard.c','tests/firmware_keyboard_transport.c','tools/firmware_drivers.py']
 r.update(passed=True,production_object_sha256=sha(obj),recovery_efi_sha256=sha(build/'RECOVERY.EFI'),derived_driver_sha256=sha(sources['TmcPs2KeyboardDxe.efi']),derived_source_binary_lock_verified=True,no_recovery_port_io_instructions=True,no_native_recovery_keyboard_driver_inclusion=True,key_support_disabled=True,raw_protocol_guid_in_compiled_firmware=True,sources={name:sha(root/name) for name in files},kernelcache_sha256=sha(build/'KernelCache'))
finally:(q/'firmware-keyboard-contract-result.json').write_text(json.dumps(r,indent=2)+'\n')
print('PASS production Recovery keyboard: firmware-only transport contracts and actual object inspection')
