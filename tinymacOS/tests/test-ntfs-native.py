#!/usr/bin/env python3
"""Real native-engine shrink/grow/hibernation/error tests, disposable files only.
Requires mkntfs, ntfscp, ntfscat, ntfsresize in PATH (independent host tools).
TMC_NTFS_TEST_DIR optionally preserves generated reports/images outside repo.
"""
from pathlib import Path
import hashlib,json,os,shutil,struct,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
for name in ['mkntfs','ntfscp','ntfscat','ntfsresize']:
    if not shutil.which(name):raise SystemExit('Missing independent test tool: '+name)
w=Path(tempfile.mkdtemp(prefix='ntfs-native-',dir=os.getenv('TMC_NTFS_TEST_DIR','/var/tmp')))
checks=[]
def run(args,**kw):return subprocess.run(list(map(str,args)),check=True,**kw)
def digest(p):return hashlib.file_digest(p.open('rb'),'sha256').hexdigest()
def check(text):checks.append(text);print('PASS',text,flush=True)
try:
    sanitize=os.getenv('TMC_NTFS_SANITIZE')=='1'
    run(['python3',root/'tools/build-ntfs.py',w]+(['--sanitize'] if sanitize else []))
    C=['gcc','-O1','-g','-fno-builtin','-no-pie','-DTMC_NTFS_HOST','-ffunction-sections','-fdata-sections','-I'+str(root/'include')]
    if sanitize:C+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
    run(C+[root/'tests/ntfs_native_host.c',root/'storage/ntfs_resize.c',root/'kernel/string.c',root/'crypto/sha256.c',w/'k-native-ntfs.o','-Wl,--gc-sections','-o',w/'host'])
    payload=w/'payload';payload.write_bytes(bytes(range(256))*1200)
    for kind,target in [('shrink',49152),('grow',163840),('hibernated',49152),('corrupt',49152),('fault',49152)]:
        image=w/(kind+'.ntfs');raw=w/(kind+'.raw');after=w/(kind+'-after.ntfs')
        with image.open('wb') as f:f.truncate(64*1024**2)
        with (w/(kind+'-mkntfs.log')).open('wb') as log:run(['mkntfs','-F','-Q',image],stdout=log,stderr=log)
        run(['ntfscp',image,payload,'/CHECK.BIN'])
        if kind=='hibernated':
            h=w/'hiberfil';h.write_bytes(b'hibr'+bytes(8192-4));run(['ntfscp',image,h,'/hiberfil.sys'])
        if kind=='corrupt':
            with image.open('r+b') as f:f.seek(510);f.write(b'XX')
        limit=max(131072,target)*512;prefix=2048*512;suffix=2048*512
        # Padded partition with untouched canaries and growth room.
        with raw.open('wb') as f:
            f.write(b'P'*prefix)
            with image.open('rb') as src:shutil.copyfileobj(src,f)
            f.seek(prefix+limit);f.write(b'S'*suffix)
        before=digest(raw)
        reject=kind in ['hibernated','corrupt']
        with (w/(kind+'.log')).open('wb') as log:run([w/'host',raw,131072,target,'reject' if reject else 'fault' if kind=='fault' else 'apply'],stdout=log,stderr=log)
        with raw.open('rb') as f:
            assert f.read(prefix)==b'P'*prefix;f.seek(prefix+limit);assert f.read()==b'S'*suffix
        if reject:
            assert digest(raw)==before;check(kind+': three native refusals, zero writes, exact whole-image hash unchanged')
        elif kind=='fault':check('injected third write fails: no success/rollback claim, partition boundary canaries unchanged')
        else:
            with raw.open('rb') as f,after.open('wb') as out:f.seek(prefix);out.write(f.read(target*512))
            with (w/(kind+'-independent.log')).open('wb') as log:run(['ntfsresize','-f','-i',after],stdout=log,stderr=log)
            found=subprocess.check_output(['ntfscat','-f',after,'/CHECK.BIN']);assert found==payload.read_bytes()
            check(kind+': three zero-write previews, real apply, exact backup BPB, independent allocation scan and 307200-byte file readback, bounds intact')
        # Fixtures are disposable; preserve reports, not redundant dense copies.
        image.unlink();raw.unlink();after.unlink(missing_ok=True)
    (w/'result.json').write_text(json.dumps({'passed':True,'sanitized':sanitize,'checks':checks,'physical_hardware':False,'windows_boot':False},indent=2)+'\n')
    print('Reports:',w)
finally:
    if not os.getenv('TMC_NTFS_TEST_DIR'):shutil.rmtree(w)
