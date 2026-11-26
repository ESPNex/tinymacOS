#!/usr/bin/env python3
"""Repack a shipping FAT image with a genuine 4096-byte BPB (not 512e).
All filenames and contents are read back through mtools and hash-compared.
Only build scratch files, never devices or existing user media.
"""
from pathlib import Path
import hashlib,os,subprocess,sys,tempfile
src,dst=map(Path,sys.argv[1:3]);assert src.is_file() and not dst.exists()
assert str(dst.resolve()).startswith('/var/tmp/')
def run(*args):subprocess.run(list(map(str,args)),check=True,stdout=subprocess.DEVNULL)
with tempfile.TemporaryDirectory(prefix='tmc-native-fat-',dir='/var/tmp') as t:
 w=Path(t);a=w/'old';b=w/'new';a.mkdir();b.mkdir()
 run('mcopy','-s','-i',src,'::*',str(a)+'/')
 size=src.stat().st_size
 with dst.open('xb') as f:f.truncate(size)
 run('mkfs.fat','--invariant','-S4096','-s1','-R32','-F16' if size>=16*1024*1024 else '-F12',dst)
 for p in a.iterdir():run('mcopy','-s','-i',dst,p,'::/')
 run('mcopy','-s','-i',dst,'::*',str(b)+'/')
 def manifest(d):return {str(p.relative_to(d)):hashlib.sha256(p.read_bytes()).hexdigest() for p in d.rglob('*') if p.is_file()}
 assert manifest(a)==manifest(b),'4096-byte FAT repack changed a filename/content'
 raw=dst.read_bytes()[:512];assert int.from_bytes(raw[11:13],'little')==4096
 print('PASS native 4096-byte FAT BPB; all',len(manifest(a)),'file paths/contents independently read back')
