#!/usr/bin/env python3
"""Build the installed-disk MBR bridge, pin/check its OpenDuet payload.
No device writes, no NASM/download required. Outputs outside the workspace.
"""
import hashlib,json,re,subprocess,sys
from pathlib import Path
from media_path import external
r=Path(__file__).resolve().parents[1];out=external(sys.argv[1]);out.mkdir(exist_ok=True,parents=True)
v=r/'vendor/opencore-1.0.8';lock=json.loads((v/'vendor-lock.json').read_text())
for n,h in lock['files'].items():assert hashlib.sha256((v/n).read_bytes()).hexdigest()==h,n
blob=(v/'bootX64').read_bytes();assert blob[:12]==b'\xeb\nDUETX64   ' and len(blob)%512==0
h=(r/'include/biosboot.h').read_text();lba=int(re.search(r'#define TMC_DUET_LBA (\d+)u',h)[1]);count=len(blob)//512
assert lba>=34 and lba+count<=2048 and 0x20200+len(blob)<0x90000
subprocess.run(['as','--32','--defsym','DUET_LBA='+str(lba),'--defsym','DUET_SECTORS='+str(count),str(r/'boot/bios-mbr.S'),'-o',str(out/'bios-mbr.o')],check=True)
subprocess.run(['ld','-m','elf_i386','--oformat','binary','-Ttext','0x7c00','-e','_start',str(out/'bios-mbr.o'),'-o',str(out/'bios-mbr.bin')],check=True)
assert (out/'bios-mbr.bin').stat().st_size==440
print('Installed BIOS bridge: 440-byte MBR + official OpenDuet full USB/AHCI',count,'sectors at LBA',lba)
subprocess.run(['as','--32','--defsym','DUET_SECTORS='+str(count),str(r/'boot/bios-coexist.S'),'-o',str(out/'bios-coexist.o')],check=True)
subprocess.run(['ld','-m','elf_i386','--oformat','binary','-Ttext','0x600','-e','_start',str(out/'bios-coexist.o'),'-o',str(out/'bios-coexist.bin')],check=True)
assert (out/'bios-coexist.bin').stat().st_size==440
