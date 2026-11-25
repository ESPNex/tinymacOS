#!/usr/bin/env python3
"""Build the pinned GPL NTFS resize engine as freestanding native objects.
No downloaded binaries, autoconf, libc or Linux syscalls enter the kernel.
Corresponding, security-patched sources are included in the repository bundle.
"""
from pathlib import Path
import hashlib,json,os,subprocess,sys,tarfile
root=Path(__file__).resolve().parents[1]
out=Path(sys.argv[1]).resolve();assert not out.is_relative_to(root)
work=out/'ntfs-native';work.mkdir(parents=True,exist_ok=True)
v=root/'vendor/ntfs-native';lock=json.loads((v/'ORIGIN.json').read_text());archive=v/lock['bundle']
assert hashlib.sha256(archive.read_bytes()).hexdigest()==lock['sha256']
src=work/'source';src.mkdir(exist_ok=True)
with tarfile.open(archive) as t:
    assert all(m.name in lock['source_files'] and m.isfile() for m in t.getmembers())
    t.extractall(src,filter='data')
for name,h in lock['source_files'].items():assert hashlib.sha256((src/name).read_bytes()).hexdigest()==h,name
C=['gcc','-O2','-g0','-ffreestanding','-fno-pie','-fno-builtin','-fno-stack-protector','-fno-unwind-tables','-fno-asynchronous-unwind-tables','-mno-red-zone','-mgeneral-regs-only','-ffunction-sections','-fdata-sections','-fstack-usage','-DHAVE_CONFIG_H','-I'+str(src),'-I'+str(src/'include/ntfs-3g'),'-I'+str(root/'include')]
if '--sanitize' in sys.argv:C+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g']
def run(args):subprocess.run(list(map(str,args)),check=True)
# Check layout against upstream declarations using the exact current compiler.
abi=work/'abi.c'
abi.write_text('''#define ntfs_device OriginalDevice
#define ntfs_device_operations OriginalOps
#include "device.h"
#undef ntfs_device
#undef ntfs_device_operations
#include "'''+str(root/'compat/ntfs/native_abi.h')+'''"
#define CK(a,b) _Static_assert((a)==(b),"ntfs_device native ABI mismatch")
CK(sizeof(struct OriginalDevice),sizeof(NativeNtfsDevice));
CK(sizeof(struct OriginalOps),sizeof(NativeNtfsOps));
CK(offsetof(struct OriginalDevice,d_state),offsetof(NativeNtfsDevice,d_state));
CK(offsetof(struct OriginalDevice,d_private),offsetof(NativeNtfsDevice,d_private));
CK(offsetof(struct OriginalDevice,d_heads),offsetof(NativeNtfsDevice,d_heads));
''')
run(C+['-c',abi,'-o',work/'abi.o'])
objects=[]
for p in sorted((src/'libntfs-3g').glob('*.c'))+[src/'ntfsprogs/ntfsresize.c']:
    o=work/(p.name+'.o');run(C+['-c',p,'-o',o]);objects.append(o)
raw=work/'engine-raw.o'
run(['ld','-r','--gc-sections','-u','tmc_ntfsresize_run',*objects,'-o',raw])
run(['objcopy','--strip-unneeded',raw])
def prefix(src,dst):
    run(['objcopy','--prefix-symbols=ntp_',src,dst])
    # Instrumented host builds retain real sanitizer-runtime names, never
    # silently replace them with missing/no-op native symbols.
    restore=[]
    for line in subprocess.check_output(['nm','-u',dst],text=True).splitlines():
        n=line.split()[-1]
        if n.startswith(('ntp___asan_','ntp___ubsan_','ntp___sanitizer_')):restore+=['--redefine-sym',n+'='+n[4:]]
    if restore:run(['objcopy',*restore,dst])
engine=work/'engine.o';prefix(raw,engine)
fmt=work/'format-raw.o';run(C+['-c',root/'compat/ntfs/format.c','-o',fmt]);prefix(fmt,work/'format.o')
run(['ld','-r',engine,work/'format.o','-o',out/'k-native-ntfs.o'])
result={'origin_sha256':lock['sha256'],'object_sha256':hashlib.sha256((out/'k-native-ntfs.o').read_bytes()).hexdigest(),'sanitized':'--sanitize' in sys.argv,'native':True,'linux_syscalls':False,'undefined':subprocess.check_output(['nm','-u',out/'k-native-ntfs.o'],text=True).splitlines()}
(work/'build.json').write_text(json.dumps(result,indent=2)+'\n')
print('NTFS native resize: source/ABI verified, integer-only freestanding engine + bounded block adapter namespace')
