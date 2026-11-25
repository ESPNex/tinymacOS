"""Development-only kernel/GUI/Terminal edit smoke test on disposable GPT media.
No device paths accepted. Does not claim NTFS resizing or real Windows boot.
"""
import hashlib,json,os,struct,subprocess,tempfile,time
from pathlib import Path
from retail_guest import Retail
from dualboot_fixtures import fixture,hashes,verify_files,PARTS
import pixels,retail_guest
root=Path(__file__).resolve().parents[1];base=Path(os.environ['TMC_QA_DIR']);iso=Path(os.environ['TMC_STAGE_ISO'])
w=Path(tempfile.mkdtemp(prefix='tmc-diskedit-guest-',dir='/var/tmp'));ev=w/'evidence';ev.mkdir();disk=w/'target/system.raw';before=fixture(disk,'gpt')
font=Path(os.environ['TMC_FONT']).read_bytes();pixels.b=font;pixels.nf,pixels.ng=struct.unpack_from('<II',font,4);pixels.codes={c:j for j,c in enumerate(struct.unpack_from(f'<{pixels.ng}I',font,12))};pixels.start=12+pixels.ng*4;pixels.pix=pixels.start+pixels.nf*(pixels.ng*8+4);pixels.word.cache_clear()
old=retail_guest.locate;retail_guest.locate=lambda im,text,faces=(0,1,2,3,5,6,8),threshold=.88:old(im,text,faces,threshold)
os.environ.update(TMC_MEM='512',TMC_POINTER='tablet',TMC_SMP='2',OCR_ROOT=os.environ['TOOLROOT'])
r={'passed':False,'work':str(w),'iso_sha256':hashlib.file_digest(iso.open('rb'),'sha256').hexdigest(),'checks':[],'physical_hardware_tested':False,'windows_boot_tested':False,'foreign_payload_before':before}
def ok(s):r['checks'].append(s);print('PASS',s,flush=True)
def vars_active(path):
    raw=path.read_bytes();out={}
    # EDK2 normal/authenticated variable headers; only committed VAR_ADDED.
    for at in range(len(raw)-60):
        if raw[at:at+3]!=b'\xaa\x55\x3f':continue
        for header,no,do,go in [(60,36,40,44),(32,8,12,16)]:
            ns,ds=struct.unpack_from('<I',raw,at+no)[0],struct.unpack_from('<I',raw,at+do)[0]
            if ns<4 or ns>1024 or ns%2 or ds>16384 or at+header+ns+ds>len(raw):continue
            name=raw[at+header:at+header+ns]
            if not name.endswith(b'\0\0'):continue
            try:name=name.decode('utf-16le').rstrip('\0')
            except UnicodeDecodeError:continue
            if not name or any(ord(c)<32 or ord(c)>126 for c in name):continue
            out[(raw[at+go:at+go+16],name)]=raw[at+header+ns:at+header+ns+ds]
    return out
try:
    with Retail(iso,w,ev,'edit-live',disk='target',live=False) as g:
        g.enroll(name='DiskEdit');g.key('f2');g.visible('Sessione locale')
        g.cmd('diskutil resizeVolume disk0s1 48','Solo anteprima');g.shot('fat32-preview')
        g.cmd('diskutil resizeVolume disk0s1 48 --confirm','Filesystem e partizione ridimensionati.')
        g.cmd('diskutil resizeVolume disk0s2 1 --confirm','NTFS non idoneo');g.shot('ntfs-honest-refusal')
        g.cmd('diskutil addPartition disk0 159744 32 Dati --confirm','Partizione nativa creata e verificata.',timeout=180)
        g.cmd('diskutil list','BTRFS nativo (subset)');g.shot('native-created')
        g.cmd('diskutil resizeVolume disk0s4 24 --confirm','Filesystem e partizione ridimensionati',timeout=180);g.shot('native-shrunk')
        g.cmd('diskutil resizeVolume disk0s4 32 --confirm','Filesystem e partizione ridimensionati',timeout=180);g.shot('native-grown')
        g.cmd('open -a "Disk Utility"');g.visible('Utility Disco');g.key('down');g.visible('Ridimensiona…')
        x,y=g.visible('Ridimensiona…');g.click(x+28,y+9);g.visible('Ridimensiona partizione');g.shot('resize-sheet');g.key('esc')
        g.key('down');g.visible('Elimina…');x,y=g.visible('Elimina…');g.click(x+25,y+8);g.visible('Elimina partizione');g.shot('delete-explicit-confirmation');g.key('ret');g.visible('Digita ELIMINA');g.key('esc')
        g.key('f2');g.visible('Sessione locale');g.cmd('diskutil deletePartition disk0s4 --confirm','Partizione eliminata. I dati non sono stati sovrascritti.')
        g.key('f11');g.visible('tinymacOS Tramontana');g.shot('installer-raster');g.key('ret');g.visible('Seleziona la destinazione');g.key('down');g.key('ret');g.visible('Conferma l’installazione');g.key('spc');g.key('ret');g.visible('Il disco è pronto',180);g.shot('install-after-edits');g.key('ret');g.p.wait(timeout=20)
    ok('Native kernel executes real FAT32 shrink, refuses undersized NTFS resize, creates/deletes native partition; GUI presents editable sheets and explicit deletion confirmation; installer uses generated raster')
    assert hashes(disk)[1:]==before[1:]
    fat=w/'shrunk.fat'
    with disk.open('rb') as f:f.seek(PARTS[0][0]*512);fat.write_bytes(f.read(98304*512))
    subprocess.run([str(Path(os.environ['TOOLROOT'])/'usr/sbin/fsck.fat'),'-n',str(fat)],check=True);fat.unlink();verify_files(disk,w)
    ok('Independent fsck.fat verifies guest-resized FAT32, mtype/ntfscat recover original files, NTFS and RAW payload hashes remain identical after all edits/install')
    class Installed(Retail):initial='DiskEdit'
    with Installed('/not-attached',w,ev,'installed-default',disk='target',boot_disk=True) as g:
        g.unlock();g.key('f2');g.visible('Sessione locale');g.cmd('diskutil deletePartition disk0s1 --confirm','Disco in uso: avvia da Recupero');g.shot('mounted-disk-protected');g.cmd('off');g.p.wait(timeout=20)
    ok('Current mounted disk rejects destructive edits in the native syscall/backend, even for another partition on that disk')
    import uuid
    globalguid=uuid.UUID('8be4df61-93ca-11d2-aa0d-00e098032b8c').bytes_le
    vendorguid=uuid.UUID('3f9a25c4-71b2-4e83-a4c1-5e9d278b6c10').bytes_le
    variables=vars_active(w/'target/vars.fd');order=variables[(globalguid,'BootOrder')];first=struct.unpack_from('<H',order)[0]
    option=variables[(globalguid,f'Boot{first:04X}')];assert 'tinymacOS OpenCore'.encode('utf-16le') in option and '\\EFI\\OC\\OpenCore.efi'.encode('utf-16le') in option
    assert variables[(vendorguid,'tinymacOS-previousBootOrder')].startswith(b'TMCBOOT2')
    r['uefi_default_entry']=f'Boot{first:04X}'
    ok('Independent OVMF NVRAM parse finds dedicated OpenCore load option FIRST in BootOrder and retained previousBootOrder backup after real installed boot')
    r['passed']=True
finally:
    (base/'diskedit-guest-result.json').write_text(json.dumps(r,indent=2)+'\n')
