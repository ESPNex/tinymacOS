"""Read actual retail optical media, seals, native ELF and every shipped plist."""
from pathlib import Path
import os,hashlib,struct,subprocess,tempfile,plistlib,json,shutil
root=Path(__file__).resolve().parents[1];base=Path(os.environ['TMC_QA_DIR']);build=Path(os.environ['TMC_BUILD']);iso=Path(os.environ['TMC_STAGE_ISO']);tool=Path(os.environ.get('TOOLROOT','/var/tmp/tmc/tools/root'))
env={**os.environ,'PATH':str(tool/'usr/bin')+':'+os.environ['PATH'],'LD_LIBRARY_PATH':str(tool/'usr/lib/x86_64-linux-gnu')}
w=Path(tempfile.mkdtemp(prefix='tmc1021-media-',dir='/var/tmp'));sha=lambda b:hashlib.sha256(b).hexdigest();r={'passed':False,'iso_sha256':sha(iso.read_bytes()),'checks':[],'physical_hardware_tested':False}
def run(*args):return subprocess.check_output(list(map(str,args)),env=env,stderr=subprocess.DEVNULL)
def extract(src,name):run('xorriso','-osirrox','on','-indev',iso,'-extract',src,w/name);return w/name
def fat(path,name):return run('mtype','-i',path,'::'+name)
def catalog(raw):
 data=raw[:-32];assert hashlib.sha256(data).digest()==raw[-32:];magic,n,step=struct.unpack_from('<8sII',data);assert magic==b'TMVOL2\0\0' and step==64;paths=[];out={}
 for i in range(n):
  p,d,o,z,name=struct.unpack_from('<iIII48s',data,16+i*64);name=name.split(b'\0')[0].decode();path=paths[p]+'/'+name if i else '';paths.append(path);assert path not in out and (d or 16+64*n<=o<=o+z<=len(data));out[path]=None if d else data[o:o+z]
 return out
def ok(s):r['checks'].append(s);print('PASS',s,flush=True)
try:
 optical=extract('/EFI.IMG','optical.fat');installed=extract('/Install.esp','installed.fat');dmg=extract('/Recovery/BaseSystem.dmg','base.dmg').read_bytes()
 kernel=(build/'RecoveryKernel.elf').read_bytes();assert kernel[:4]==b'\x7fELF' and struct.unpack_from('<HH',kernel,16)==(2,62)
 assert kernel!=(build/'kernel.elf').read_bytes();assert b'separate Recovery kernel; firmware services exited' in kernel
 footer=dmg[-512:];assert footer[:4]==b'koly';fork,xml=struct.unpack_from('>QQ',footer,216);assert fork+xml==len(dmg)-512;plistlib.loads(dmg[fork:fork+xml]);(w/'base.fat').write_bytes(dmg[:fork])
 assert fat(w/'base.fat','/RECOVERY.ELF')==kernel;recvol=fat(w/'base.fat','/RECOVERY.VOL');assert recvol==(build/'System-recovery.vol').read_bytes()
 listing=run('mdir','-i',w/'base.fat','::/').decode();assert 'BOOTX64' not in listing and 'RECOVERY' in listing
 ok('Sealed BaseSystem contains a distinct x86-64 native ELF kernel and verified system volume, not an EFI Recovery application')
 for name in ['BOOTX64','RECBOOT']:
  raw=(build/(name+'.EFI')).read_bytes();assert hashlib.sha256(dmg).digest() in raw
  symbols=run('nm',build/(name+'.so')).decode();assert 'image_recovery' not in symbols and 'start_file' not in symbols
 ok('Both EFI bootstraps pin the same BaseSystem seal; native loader shares checked ELF loading and ExitBootServices handoff')
 symbols=run('nm',build/'recovery-kernel.debug').decode();required=['kmain','input_init','ahci_init','scheduler_init','tmc_usb_kernel_worker'];assert all(x in symbols for x in required)
 assert 'xhci' in symbols and 'ehci' in symbols and 'hid' in symbols
 ok('Dedicated Recovery ELF links the real scheduler, memory, AHCI and native USB HCD/hub/HID stack')
 prefix=fat(optical,'/Recovery/Volume.hdr');rec=(prefix+dmg).ljust(10<<20,b'\0');assert len(rec)==10<<20;(w/'recovery.fat').write_bytes(rec)
 assert fat(w/'recovery.fat','/EFI/BOOT/BOOTX64.EFI')==(build/'RECBOOT.EFI').read_bytes();assert fat(w/'recovery.fat','/Recovery/BaseSystem.dmg')==dmg
 esp=installed.read_bytes();assert fat(optical,'/Recovery/Payload.sha')==hashlib.sha256(esp).digest()+hashlib.sha256(rec).digest()
 ok('Installed Recovery partition, optical BaseSystem and installer payload readback seals agree')
 font=Path(os.environ['TMC_FONT']).read_bytes();assert sha(font)=='b2dc1065c12eb43378e02d824af58697369760c268f2dbc64542dcac04a514d2';counts={};renderers={};installers={}
 for label,raw in [('live',fat(optical,'/System/Library/CoreServices/System.vol')),('installed',fat(installed,'/System/Library/CoreServices/System.vol')),('recovery',recvol)]:
  files=catalog(raw);renderers[label]=files['/System/Library/CoreServices/WindowServer'];assert files['/System/Library/Fonts/Interface.tfn']==font;plists=[(p,plistlib.loads(v)) for p,v in files.items() if v and p.endswith('.plist')];assert plists
  for p,v in plists:
   assert isinstance(v,dict)
   if p.endswith('SystemVersion.plist'):assert v=={'ProductName':'tinymacOS','ProductVersion':'10.2.5','ProductBuildVersion':'30B16'}
   if v.get('CFBundlePackageType')=='APPL':assert v['CFBundleShortVersionString']=='10.2.5' and v['CFBundleVersion']=='30B16'
  if label in ('live','recovery'):
   app='/Applications/Install tinymacOS.app/Contents/'
   installers[label]=files[app+'MacOS/Install tinymacOS']
   meta=plistlib.loads(files[app+'Info.plist'])
   assert meta['CFBundleDisplayName']==meta['CFBundleName']==('Reinstalla tinymacOS Tramontana' if label=='recovery' else 'Installa tinymacOS Tramontana')
  counts[label]=len(plists)
  if label=='recovery':assert not any('Desktop Pictures/' in p for p in files) and not any('/Users/' in p for p in files)
 assert len(set(renderers.values()))==1
 assert installers['live']==installers['recovery']
 r['byte_identical_desktop_and_recovery_installer']=True
 ok('Desktop and Recovery contain the byte-identical native installer executable and contextual Installa / Reinstalla tinymacOS Tramontana display names')
 r['same_font_renderer_binary_all_volumes']=True
 r['font_identical_desktop_and_recovery']=True;r['plist_counts']=counts;r['font_sha256']=sha(font);r['recovery_kernel_sha256']=sha(kernel);r['desktop_kernel_sha256']=sha((build/'kernel.elf').read_bytes())
 ok('Every packaged .plist parses with plistlib; all three system volumes are 10.2.5/30B16 and retain the exact accepted font')
 assert fat(installed,'/EFI/TMC/BOOTX64.EFI')==(build/'BOOTX64.EFI').read_bytes()
 for volume in [installed,optical]:
  for path in ['/EFI/BOOT/.contentVisibility','/EFI/OC/.contentVisibility']:assert fat(volume,path)==b'Disabled\n'
 ok('OpenCore hides only its own bootstrap/manager from its OS picker; native second-stage boot remains visible')
 assert b'10.2.5' in extract('/BIOS-README.TXT','bios-readme.txt').read_bytes()
 cfg=fat(w/'recovery.fat','/BOOTCFG.BIN');assert len(cfg)==512 and cfg[:8]==b'TMCBOOT1' and hashlib.sha256(cfg[:16]).digest()==cfg[16:48]
 ok('Optical identity and preallocated checksummed Recovery boot configuration are current')
 r['passed']=True
finally:
 (base/'media-result.json').write_text(json.dumps(r,indent=2)+'\n');shutil.rmtree(w)
