#!/usr/bin/env python3
"""Audit current versions in actual optical/installed media; include the updated About window."""
from pathlib import Path
import hashlib,json,os,plistlib,struct,subprocess,tempfile,zlib
q=Path(__file__).resolve().parent;home=q.parents[1];osroot=home/'tinymacOS';base=Path('/var/tmp/tmc-10.1.7');iso=Path(os.environ.get('TMC_STAGE_ISO',str(base/'tinymacOS-10.1.7-x86_64.iso')));build=Path(os.environ.get('TMC_STAGE_BUILD',str(base/'linux-mouse-v7/build')));w=Path(tempfile.mkdtemp(prefix='tmc1017-identity-',dir='/var/tmp'));tool=Path('/var/tmp/tmc/tools/root');env={**os.environ,'PATH':str(tool/'usr/bin')+':'+os.environ['PATH'],'LD_LIBRARY_PATH':str(tool/'usr/lib/x86_64-linux-gnu')}
def run(*args):return subprocess.check_output(list(map(str,args)),env=env,stderr=subprocess.DEVNULL)
def sha(p):return hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
def extract(src,dest):run('xorriso','-osirrox','on','-indev',iso,'-extract',src,w/dest);return w/dest
def fat(path,src):return run('mtype','-i',path,'::'+src)
def catalog(raw):
 b=raw[:-32];assert hashlib.sha256(b).digest()==raw[-32:];magic,n,step=struct.unpack_from('<8sII',b);assert magic==b'TMVOL2\0\0' and step==64;paths=[];out={}
 for i in range(n):
  parent,d,off,size,name=struct.unpack_from('<iIII48s',b,16+i*step);name=name.split(b'\0')[0].decode();p=paths[parent]+'/'+name if i else '';paths.append(p);out[p]=None if d else b[off:off+size]
 return out
r={'passed':False,'iso_sha256':sha(iso),'checks':[],'physical_hardware_tested':False}
try:
 optical=extract('/EFI.IMG','opt.fat');disk=extract('/Install.esp','disk.fat');about_path='/System/Library/CoreServices/About This Computer.app/Contents/MacOS/About This Computer'
 history_path='/System/Library/CoreServices/CoreTypes.bundle/Contents/Resources/IconsManifest.json'
 historical_manifest=run('git','--git-dir=/var/tmp/tmc-github-git/tinymacOS.git','show','678c7e4d1947dbd8feaa11042b13eb5bd550ba36:tinymacOS/assets/icons/manifest.json')
 for label,medium in [('optical',optical),('installed',disk)]:
  raw=fat(medium,'/System/Library/CoreServices/System.vol');assert raw==(build/('System-live.vol' if label=='optical' else 'System-disk.vol')).read_bytes();c=catalog(raw)
  assert plistlib.loads(c['/System/Library/CoreServices/SystemVersion.plist'])=={'ProductName':'tinymacOS','ProductVersion':'10.1.7','ProductBuildVersion':'30B8'}
  count=0
  for p,b in c.items():
   if p.endswith('/Contents/Info.plist') and '/Extensions/' not in p:
    info=plistlib.loads(b);assert info['CFBundleShortVersionString']=='10.1.7' and info['CFBundleVersion']=='30B8',p;count+=1
   if p==history_path:assert b==historical_manifest
   elif b:assert b'10.1.6' not in b and b'30B7' not in b,p
  assert b'Versione 10.1.7' in c[about_path] and b'Build 30B8' in c[about_path]
  r['checks'].append({'medium':label,'system_version':'10.1.7','build':'30B8','app_metadata_count':count,'about_version':'10.1.7','about_build':'30B8','old_current_version_absent_except_unchanged_icon_provenance':True})
 dmg=extract('/Recovery/BaseSystem.dmg','base.dmg').read_bytes();footer=dmg[-512:];assert footer[:4]==b'koly';xml,n=struct.unpack_from('>QQ',footer,216);assert xml+n==len(dmg)-512;assert zlib.crc32(dmg[:xml])==struct.unpack_from('>I',footer,88)[0];(w/'base.fat').write_bytes(dmg[:xml]);rec=fat(w/'base.fat','/EFI/BOOT/BOOTX64.EFI');assert rec==(build/'RECOVERY.EFI').read_bytes();assert b'tinymacOS Recovery 10.1.7 (30B8)' in rec and b'10.1.6' not in rec and b'[efi-pointer]' not in rec
 symbols=run('nm',build/'RECOVERY.so').decode();assert 'rec_pointer_start' not in symbols and 'rec_pointer_next' not in symbols
 about_src=run('git','--git-dir=/var/tmp/tmc-github-git/tinymacOS.git','show','678c7e4d1947dbd8feaa11042b13eb5bd550ba36:tinymacOS/userspace/about.c');assert about_src.replace(b'10.1.6',b'10.1.7').replace(b'30B7',b'30B8')==(osroot/'userspace/about.c').read_bytes()
 r.update(passed=True,recovery_version='10.1.7 / 30B8',normal_recovery_has_no_raw_pointer_trace=True,direct_mouse_usb_driver_not_linked_in_recovery=True,about_version_updated=True,about_layout_source_unchanged_except_version_and_build=True)
 print('PASS actual ISO: optical/installed metadata 10.1.7/30B8, About 10.1.7/30B8 with original layout preserved, Recovery version and firmware-only mouse linkage')
finally:(q/'identity-contract-result.json').write_text(json.dumps(r,indent=2)+'\n')
