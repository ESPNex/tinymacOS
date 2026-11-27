#!/usr/bin/env python3
"""Build pinned patched FatFs and native bounded adapter outside the workspace.
The source archive and all members are SHA256 checked. No downloaded binaries,
OS services or hosted filesystem APIs are linked into the kernel.
"""
from pathlib import Path
import hashlib,json,subprocess,sys,tarfile,re
root=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]).resolve();assert not out.is_relative_to(root)
work=out/'fatfs-native';work.mkdir(parents=True,exist_ok=True);vendor=root/'vendor/fatfs';lock=json.loads((vendor/'ORIGIN.json').read_text());archive=vendor/lock['bundle'];assert hashlib.file_digest(archive.open('rb'),'sha256').hexdigest()==lock['sha256']
src=work/'source';src.mkdir(exist_ok=True)
with tarfile.open(archive) as t:
 assert all(m.isfile() and m.name in lock['source_files'] for m in t.getmembers());t.extractall(src,filter='data')
for name,row in lock['source_files'].items():assert hashlib.sha256((src/name).read_bytes()).hexdigest()==row['sha256'],name
source=src/'source';config=(source/'ffconf.h').read_text();settings={'FF_USE_MKFS':1,'FF_CODE_PAGE':437,'FF_USE_LFN':3,'FF_LFN_UNICODE':2,'FF_LFN_BUF':1020,'FF_FS_EXFAT':1,'FF_LBA64':1,'FF_MAX_SS':4096,'FF_MIN_SS':512,'FF_FS_RPATH':0,'FF_VOLUMES':1}
for key,value in settings.items():config,n=re.subn(r'(#define\s+'+key+r'\s+)\d+',lambda m:m[1]+str(value),config);assert n==1,key
(source/'ffconf.h').write_text(config)
flags=['-O2','-g0','-ffreestanding','-fno-pie','-fno-builtin','-fno-stack-protector','-fno-unwind-tables','-fno-asynchronous-unwind-tables','-mno-red-zone','-mgeneral-regs-only','-ffunction-sections','-fdata-sections','-fstack-usage','-fno-ident','-ffile-prefix-map='+str(src)+'=fatfs-native/source','-ffile-prefix-map='+str(root)+'=.','-I'+str(source),'-I'+str(root/'include')]
if '--sanitize' in sys.argv:flags+=['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer']
objects=[]
for p in [source/'ff.c',source/'ffunicode.c',root/'compat/fatfs/volume.c']:
 o=work/(p.name+'.o');subprocess.run(['gcc',*flags,'-c',str(p),'-o',str(o)],check=True);objects.append(str(o))
target=out/'k-native-fat.o';subprocess.run(['ld','-r',*objects,'-o',str(target)],check=True)
(work/'build.json').write_text(json.dumps({'origin_sha256':lock['sha256'],'object_sha256':hashlib.file_digest(target.open('rb'),'sha256').hexdigest(),'configuration':settings,'sanitized':'--sanitize' in sys.argv,'native':True,'partition_view_only':True},indent=2)+'\n')
print('PASS pinned FatFs R0.16 + patch 3: freestanding FAT32/exFAT read/write and raw-partition exFAT formatter built; not runtime or guest acceptance')
