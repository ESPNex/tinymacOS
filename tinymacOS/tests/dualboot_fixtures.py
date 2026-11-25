"""Disposable synthetic GPT/MBR media. Never accepts a device path.
Real mkfs.fat FAT32 and mkntfs NTFS filesystems with independent readers;
the previous boot is a named sentinel, NOT a Windows installation.
"""
import hashlib,os,struct,subprocess,uuid,zlib
from pathlib import Path
PARTS=[(2048,131072),(135168,16384),(153600,4096)]
SECTORS=524288

def fixture(path,scheme):
 path=Path(path);assert str(path).startswith('/var/tmp/') and not path.exists();path.parent.mkdir(parents=True,exist_ok=True)
 root=Path(__file__).resolve().parents[1];w=path.parent;env={**os.environ,'PATH':os.environ['TOOLROOT']+'/usr/sbin:'+os.environ['TOOLROOT']+'/usr/bin:'+os.environ['PATH'],'LD_LIBRARY_PATH':os.environ['TOOLROOT']+'/usr/lib/x86_64-linux-gnu'}
 subprocess.run(['as','--32',root/'tests/bios-previous-fixture.S','-o',w/'previous.o'],check=True)
 subprocess.run(['ld','-m','elf_i386','--oformat','binary','-Ttext','0x7c00','-e','_start',w/'previous.o','-o',w/'previous.bin'],check=True)
 with path.open('wb') as f:f.truncate(SECTORS*512)
 fat=w/'fat32.fat'
 with fat.open('wb') as f:f.truncate(131072*512)
 subprocess.run(['mkfs.fat','--invariant','-F32','-s1','-n','PRESERVE',fat],env=env,check=True,stdout=subprocess.DEVNULL)
 (w/'preserve.txt').write_text('EXISTING_FAT32_FILE_MUST_SURVIVE\n')
 subprocess.run(['mcopy','-i',fat,w/'preserve.txt','::/PRESERVE.TXT'],env=env,check=True)
 ntfs=w/'ntfs.img'
 with ntfs.open('wb') as f:f.truncate(PARTS[1][1]*512)
 nt=Path(os.environ.get('TMC_NTFS_TOOLS','/var/tmp/tmc-ntfs-tools/root'));nenv={**env,'LD_LIBRARY_PATH':str(nt/'usr/lib/x86_64-linux-gnu')+':'+env['LD_LIBRARY_PATH']}
 subprocess.run([nt/'usr/sbin/mkntfs','-F','-Q','-L','EXISTING',ntfs],env=nenv,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
 subprocess.run([nt/'usr/sbin/ntfscp',ntfs,w/'preserve.txt','/PRESERVE.TXT'],env=nenv,check=True)
 mbr=bytearray(512);mbr[:440]=(w/'previous.bin').read_bytes();mbr[440:444]=b'QA23';mbr[510:]=b'\x55\xaa'
 if scheme=='mbr':
  for i,((a,n),t) in enumerate(zip(PARTS,[12,7,0xda])):struct.pack_into('<B3sB3sII',mbr,446+i*16,128 if i==0 else 0,b'\xfe\xff\xff',t,b'\xfe\xff\xff',a,n)
 else:struct.pack_into('<B3sB3sII',mbr,446,0,b'\x00\x02\x00',0xee,b'\xff\xff\xff',1,SECTORS-1)
 with path.open('r+b') as f:
  f.write(mbr);f.seek(PARTS[0][0]*512);f.write(fat.read_bytes())
  f.seek(PARTS[1][0]*512);f.write(ntfs.read_bytes())
  f.seek(PARTS[2][0]*512);f.write(b'RAW_DATA_NOT_FREE_SPACE'.ljust(PARTS[2][1]*512,b'R'))
  if scheme=='gpt':
   p=bytearray(16384)
   for i,(a,n) in enumerate(PARTS):
    name=['FAT32 esistente','NTFS esistente','RAW protetto'][i].encode('utf-16le').ljust(72,b'\0');struct.pack_into('<16s16sQQQ72s',p,i*128,uuid.UUID('c12a7328-f81f-11d2-ba4b-00a0c93ec93b' if i==0 else 'ebd0a0a2-b9e5-4433-87c0-68b6b72699c7').bytes_le,uuid.uuid4().bytes_le,a,a+n-1,0,name)
   diskid=uuid.uuid4().bytes_le
   def header(at,other,entries):
    b=bytearray(512);struct.pack_into('<8sIIIIQQQQ16sQIII',b,0,b'EFI PART',65536,92,0,0,at,other,34,SECTORS-34,diskid,entries,128,128,zlib.crc32(p));struct.pack_into('<I',b,16,zlib.crc32(b[:92]));return b
   for at,data in [(1,header(1,SECTORS-1,2)),(2,p),(SECTORS-33,p),(SECTORS-1,header(SECTORS-1,1,SECTORS-33))]:f.seek(at*512);f.write(data)
 fat.unlink();ntfs.unlink();return hashes(path)

def hashes(path):
 out=[]
 with Path(path).open('rb') as f:
  for a,n in PARTS:f.seek(a*512);out.append(hashlib.sha256(f.read(n*512)).hexdigest())
 return out

def verify_files(path,work):
 root=Path(os.environ.get('TMC_NTFS_TOOLS','/var/tmp/tmc-ntfs-tools/root'));tools=Path(os.environ['TOOLROOT']);env={**os.environ,'LD_LIBRARY_PATH':str(root/'usr/lib/x86_64-linux-gnu')+':'+str(tools/'usr/lib/x86_64-linux-gnu')}
 fat=subprocess.check_output([tools/'usr/bin/mtype','-i',str(path)+'@@'+str(PARTS[0][0]*512),'::/PRESERVE.TXT'],env=env)
 image=Path(work)/'ntfs-readback.img'
 with Path(path).open('rb') as f:f.seek(PARTS[1][0]*512);image.write_bytes(f.read(PARTS[1][1]*512))
 ntfs=subprocess.check_output([root/'usr/bin/ntfscat',image,'/PRESERVE.TXT'],env=env);image.unlink()
 assert fat==ntfs==b'EXISTING_FAT32_FILE_MUST_SURVIVE\n'
