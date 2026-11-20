#!/usr/bin/env python3
"""Extract ISO root into a new disposable GPT/FAT32 USB; boot with NO CD.
Not Rufus/Windows, not a real hardware test. All media outside /home/user.
"""
import hashlib,json,os,shutil,socket,struct,subprocess,tempfile,time,uuid,zlib,sys
from pathlib import Path
from PIL import Image
sys.path.insert(0,'/home/user/tinymacOS/tests')
from pixels import locate
w=Path(tempfile.mkdtemp(prefix='tmc-usb-242-',dir='/var/tmp'));e=Path(__file__).resolve().parents[1]/'evidence'
tr=Path('/var/tmp/tmc/tools/root');iso=Path('/var/tmp/tmc-maintenance-242/tinymacOS-2.4.2-x86_64.iso')
env={**os.environ,'PATH':str(tr/'usr/bin')+':'+str(tr/'usr/sbin')+':'+os.environ['PATH'],'LD_LIBRARY_PATH':str(tr/'usr/lib/x86_64-linux-gnu')}
def run(*a):subprocess.run(list(map(str,a)),env=env,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
run('xorriso','-osirrox','on','-indev',iso,'-extract','/',w/'root')
part=w/'part.fat';part_size=256*1024*1024
with part.open('wb') as f:f.truncate(part_size)
run('mkfs.fat','--invariant','-F32','-n','TMC242',part)
for p in (w/'root').iterdir():run('mcopy','-s','-i',part,p,'::/')
sectors=part_size//512+4096;first=2048;last=first+part_size//512-1
entries=bytearray(128*128);entries[:16]=uuid.UUID('c12a7328-f81f-11d2-ba4b-00a0c93ec93b').bytes_le;entries[16:32]=uuid.uuid4().bytes_le
struct.pack_into('<QQQ',entries,32,first,last,0);name='tinymacOS 2.4.2'.encode('utf-16le');entries[56:56+len(name)]=name
guid=uuid.uuid4().bytes_le
def hdr(lba,other,table):
 b=bytearray(512);struct.pack_into('<8sIIIIQQQQ16sQIII',b,0,b'EFI PART',65536,92,0,0,lba,other,34,sectors-34,guid,table,128,128,zlib.crc32(entries));struct.pack_into('<I',b,16,zlib.crc32(b[:92]));return b
raw=w/'usb-gpt.raw'
with raw.open('wb') as f:
 f.truncate(sectors*512);mbr=bytearray(512);mbr[446+4]=0xee;struct.pack_into('<II',mbr,454,1,sectors-1);mbr[510:]=b'\x55\xaa';f.write(mbr);f.seek(512);f.write(hdr(1,sectors-1,2));f.write(entries);f.seek((sectors-33)*512);f.write(entries);f.write(hdr(sectors-1,1,sectors-33));f.seek(first*512)
 with part.open('rb') as src:shutil.copyfileobj(src,f,1024*1024)
shutil.copy2(tr/'usr/share/OVMF/OVMF_VARS_4M.fd',w/'vars.fd');sock=w/'qmp.sock'
cmd=[str(tr/'usr/bin/qemu-system-x86_64'),'-L',str(tr/'usr/share/qemu'),'-L',str(tr/'usr/share/seabios'),'-machine','q35,i8042=off','-accel','tcg','-cpu','max','-smp','2','-m','1024','-vga','std','-drive',f'if=pflash,format=raw,readonly=on,file={tr}/usr/share/OVMF/OVMF_CODE_4M.fd','-drive',f'if=pflash,format=raw,file={w}/vars.fd','-device','qemu-xhci,id=xhci','-device','usb-kbd,bus=xhci.0,port=1','-device','usb-tablet,bus=xhci.0,port=2','-drive',f'if=none,id=usb,format=raw,readonly=on,file={raw}','-device','usb-storage,drive=usb,bus=xhci.0,port=3,bootindex=1','-object','rng-random,filename=/dev/urandom,id=rng','-device','virtio-rng-pci,rng=rng','-display','none','-serial',f'file:{w}/serial.log','-qmp',f'unix:{sock},server=on,wait=off']
p=subprocess.Popen(cmd,env=env,stdout=(w/'qemu.log').open('wb'),stderr=subprocess.STDOUT)
try:
 for _ in range(150):
  if sock.exists():break
  if p.poll() is not None:raise RuntimeError((w/'qemu.log').read_text())
  time.sleep(.1)
 s=socket.socket(socket.AF_UNIX);s.settimeout(15);s.connect(str(sock));f=s.makefile('rwb',buffering=0);f.readline()
 def q(command,**a):
  f.write(json.dumps({'execute':command,'arguments':a}).encode()+b'\n')
  while True:
   r=json.loads(f.readline())
   if 'error'in r:raise RuntimeError(r)
   if 'return'in r:return r['return']
 q('qmp_capabilities');end=time.monotonic()+60
 while time.monotonic()<end:
  q('screendump',filename=str(w/'frame.ppm'));im=Image.open(w/'frame.ppm').convert('RGB')
  if locate(im,'Crea un account',faces=(0,1,2,3,5,6,8)):break
  time.sleep(.3)
 else:raise AssertionError('No account setup observed on USB-only boot')
 im.save(e/'usb-only-boot.png');blocks=q('query-block');assert all(b.get('inserted',{}).get('file')!=str(iso) for b in blocks)
 result={'passed':True,'iso_sha256':hashlib.sha256(iso.read_bytes()).hexdigest(),'boot_medium':'fresh GPT/FAT32 USB with extracted ISO root','readonly':True,'cd_attached':False,'gop_setup_marker_verified':True,'physical_hardware_test':False,'rufus_executed':False}
 (e/'usb-only-boot.json').write_text(json.dumps(result,indent=2));print('PASS: 2-vCPU boot from readonly GPT/FAT32 USB, no CD; actual setup verified in framebuffer.')
 q('quit');p.wait(timeout=10)
finally:
 if p.poll() is None:p.terminate();p.wait(timeout=10)
