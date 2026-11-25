#!/usr/bin/env python3
"""Make the project's own read-only UDIF (one FAT16 volume, raw sector runs).
Only the integrated ISO ships. Internal build products live in its external scratch dir.
No Apple software, HFS/APFS, hdiutil or network access required.
"""
import hashlib, os, plistlib, struct, subprocess, sys, zlib
from pathlib import Path
from media_path import external

stage=external(sys.argv[1]);runtime=Path(sys.argv[2]).resolve();raw=stage/'base.raw'
env={**os.environ,'LC_ALL':'C','TZ':'UTC','MTOOLS_SKIP_CHECK':'1','MTOOLS_DATE_STRING':'2025-01-01T00:00:00','SOURCE_DATE_EPOCH':'1735689600'}
def run(*args):subprocess.run(args,env=env,stdout=subprocess.DEVNULL,check=True)
with raw.open('wb') as f:f.truncate(8<<20)
run('mkfs.fat','--invariant','-F16','-s1','-n','BASE SYSTEM',raw)
run('mcopy','-i',raw,runtime,'::/RECOVERY.ELF');run('mcopy','-i',raw,Path(sys.argv[3]).resolve(),'::/RECOVERY.VOL')
data=raw.read_bytes();n=len(data)//512;crc=zlib.crc32(data)
def checksum(c):return struct.pack('>III',2,32,c)+bytes(124)
def extent(kind,sector,count,offset,size):return struct.pack('>IIQQQQ',kind,0,sector,count,offset,size)
step=512  # 256 KiB. Uncompressed; every logical sector maps to its own file offset.
runs=[extent(1,i,min(step,n-i),512*i,512*min(step,n-i)) for i in range(0,n,step)]
runs.append(extent(0xffffffff,n,0,len(data),0))
blkx=(struct.pack('>4sIQQQII',b'mish',1,0,n,0,step,0)+bytes(24)+checksum(crc)+struct.pack('>I',len(runs))+b''.join(runs))
name='Base System (DOS_FAT_16 : 0)';xml=plistlib.dumps({'resource-fork':{'blkx':[{'Attributes':'0x0050','CFName':name,'Data':blkx,'ID':'0','Name':name}]}})
footer=bytearray(512);struct.pack_into('>4sIIIQQQQQII',footer,0,b'koly',4,512,1,0,0,len(data),0,0,1,1)
footer[64:80]=hashlib.sha256(data).digest()[:16];footer[80:216]=checksum(crc);struct.pack_into('>QQ',footer,216,len(data),len(xml));footer[352:488]=checksum(zlib.crc32(struct.pack('>I',crc)));struct.pack_into('>IQ',footer,488,2,n)
image=data+xml+footer;(stage/'BaseSystem.dmg').write_bytes(image);digest=hashlib.sha256(image).digest()
(stage/'baseseal.h').write_text(f'#define BASE_LEN {len(image)}u\n#define BASE_FORK {len(data)}u\nstatic const u8 base_sha[32]={{'+','.join(map(str,digest))+'};\n')
print(f'BaseSystem UDIF: {len(image)} B; data fork {len(data)} B; SHA-256 {digest.hex()}')
