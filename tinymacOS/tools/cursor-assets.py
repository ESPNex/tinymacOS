#!/usr/bin/env python3
"""Retain the supplied 32px CUR DIBs losslessly; trim only unused resolutions.
ANI frames, timing, sequence and metadata are retained (frame CURs compacted).
Never executes archive contents or extracts paths from the archive.
"""
import hashlib,json,struct,sys,zipfile
from pathlib import Path
NAMES=['Normal','Text','Select','Move','Horizontal Resize','Vertical Resize','Diagonal Resize 1','Diagonal Resize 2','Unavailable','Busy','Working','Help','Precision','Handwriting','Alternate','Location','Person']
def compact(b):
    assert b[:4]==b'\0\0\2\0'
    for i in range(struct.unpack_from('<H',b,4)[0]):
        e=bytearray(b[6+i*16:22+i*16])
        if e[:2]!=b'  ':continue
        n,off=struct.unpack_from('<II',e,8);d=b[off:off+n];assert len(d)==n
        struct.pack_into('<I',e,12,22)
        return b'\0\0\2\0\1\0'+e+d
    raise ValueError('No original 32px DIB')
def chunks(b):
    p=0
    while p<len(b):
        assert p+8<=len(b)
        tag=b[p:p+4];n=struct.unpack_from('<I',b,p+4)[0];v=b[p+8:p+8+n];assert len(v)==n
        if tag==b'icon':v=compact(v)
        elif tag==b'LIST':v=v[:4]+chunks(v[4:])
        yield_bytes=tag+struct.pack('<I',len(v))+v+b'\0'*(len(v)%2)
        yield yield_bytes
        p+=8+n+n%2
    assert p==len(b)
# Wrapper permits recursion without exposing archive filenames to the filesystem.
_chunk_iter=chunks
def chunks(b):return b''.join(_chunk_iter(b))
def main():
    src=Path(sys.argv[1]);dst=Path(__file__).resolve().parents[1]/'assets/cursors';dst.mkdir(exist_ok=True)
    entries=[]
    with zipfile.ZipFile(src) as z:
        for name in NAMES:
            ext='.ani' if name in ['Busy','Working'] else '.cur';raw=z.read('Cursors/'+name+ext)
            if ext=='.cur':out=compact(raw)
            else:
                assert raw[:4]==b'RIFF' and raw[8:12]==b'ACON'
                body=b'ACON'+chunks(raw[12:]);out=b'RIFF'+struct.pack('<I',len(body))+body
            (dst/(name+ext)).write_bytes(out)
            entries.append({'name':name+ext,'source_sha256':hashlib.sha256(raw).hexdigest(),'sha256':hashlib.sha256(out).hexdigest(),'bytes':len(out)})
    (dst/'manifest.json').write_text(json.dumps({'source':'User-supplied Cursors.zip from Google Drive','archive_sha256':hashlib.sha256(src.read_bytes()).hexdigest(),'processing':'Original 32px DIB bytes, hotspot, ANI metadata/order/timing preserved; unused resolutions removed; no redrawing','files':entries},indent=2)+'\n')
    print('Imported',len(entries),'cursors;',sum(e['bytes'] for e in entries),'bytes')
if __name__=='__main__':main()
