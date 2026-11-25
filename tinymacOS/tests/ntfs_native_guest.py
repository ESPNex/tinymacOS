"""Native UEFI kernel/syscall NTFS resize on disposable GPT + MBR fixtures.
No real Windows installation or physical-hardware certification is implied.
"""
import hashlib,json,os,struct,subprocess,tempfile
from pathlib import Path
from retail_guest import Retail
from dualboot_fixtures import fixture,hashes,PARTS
import pixels,retail_guest
base=Path(os.environ['TMC_QA_DIR']);iso=Path(os.environ['TMC_STAGE_ISO'])
if iso.is_dir():iso=iso/'tinymacOS-10.2.4-x86_64.iso'
w=Path(tempfile.mkdtemp(prefix='tmc-ntfs-native-guest-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir()
font=Path(os.environ['TMC_FONT']).read_bytes();pixels.b=font;pixels.nf,pixels.ng=struct.unpack_from('<II',font,4);pixels.codes={c:j for j,c in enumerate(struct.unpack_from(f'<{pixels.ng}I',font,12))};pixels.start=12+pixels.ng*4;pixels.pix=pixels.start+pixels.nf*(pixels.ng*8+4);pixels.word.cache_clear()
old=retail_guest.locate;retail_guest.locate=lambda im,text,faces=(0,1,2,3,5,6,8),threshold=.88:old(im,text,faces,threshold)
os.environ.update(TMC_MEM='512',TMC_POINTER='tablet',TMC_SMP='2',OCR_ROOT=os.environ['TOOLROOT'])
r={'passed':False,'work':str(w),'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'checks':[],'physical_hardware_tested':False,'windows_boot_tested':False}
try:
    for scheme,size in [('gpt',6),('mbr',9)]:
        disk=w/(scheme+'/system.raw');before=fixture(disk,scheme)
        with Retail(iso,w,ev,scheme,disk=scheme,live=False) as g:
            g.enroll(name='NativeNTFS');g.key('f2');g.visible('Sessione locale')
            g.cmd(f'diskutil resizeVolume disk0s2 {size}','Solo anteprima');g.shot(scheme+'-preview')
            g.cmd(f'diskutil resizeVolume disk0s2 {size} --confirm','Filesystem e partizione ridimensionati');g.shot(scheme+'-applied')
            g.cmd('diskutil resizeVolume disk0s2 5 --confirm','NTFS non idoneo');g.shot(scheme+'-dirty-protected')
            g.cmd('off');g.p.wait(timeout=20)
        after=hashes(disk);assert before[0]==after[0] and before[2]==after[2]
        image=w/(scheme+'-resized.ntfs')
        with disk.open('rb') as f:f.seek(PARTS[1][0]*512);image.write_bytes(f.read(size*1024**2))
        nt=Path('/var/tmp/tmc-ntfs-tools/root');env={**os.environ,'LD_LIBRARY_PATH':str(nt/'usr/lib/x86_64-linux-gnu')+':'+os.environ.get('LD_LIBRARY_PATH','')}
        with (w/(scheme+'-independent.log')).open('wb') as log:subprocess.run([nt/'usr/sbin/ntfsresize','-f','-i',image],env=env,check=True,stdout=log,stderr=log)
        assert subprocess.check_output([nt/'usr/bin/ntfscat','-f',image,'/PRESERVE.TXT'],env=env)==b'EXISTING_FAT32_FILE_MUST_SURVIVE\n'
        r['checks'].append(f'{scheme}: actual kernel/syscall NTFS 8->{size}MiB, preview + apply, post-resize dirty refusal, independent consistency/file readback, FAT32+RAW hashes unchanged')
        print('PASS',r['checks'][-1],flush=True)
        # Disposable media are not deliverables; retain evidence and reports.
        image.unlink();disk.unlink()
    r['passed']=True
finally:
    (base/'ntfs-native-guest-result.json').write_text(json.dumps(r,indent=2)+'\n')
    print('Reports:',w,flush=True)
