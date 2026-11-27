"""Read-only whole-foreign-content verification on sparse disposable M2 media.
SEEK_DATA/HOLE skips filesystem-certified zero holes, not unexamined disk bytes.
Every allocated foreign sector is checked, not only boundary canaries.
"""
from pathlib import Path
import errno,hashlib,os

def verify_foreign(path,total,start,span):
    path=Path(path);assert str(path).startswith('/var/tmp/tmc-m2-capacity-') and path.name=='system.raw'
    ranges=[(2048,start-1),(start+span,total-2049)];expected={};found={}
    for i,(a,b) in enumerate(ranges):
        for pos in (a,b-7):
            block=('FOREIGN_%d_%d'%(i,pos)).encode().ljust(4096,b'R')
            for j in range(8):expected[pos+j]=hashlib.sha256(block[j*512:(j+1)*512]).hexdigest()
    with path.open('rb',buffering=0) as f:
        assert os.fstat(f.fileno()).st_size==total*512
        for a,b in ranges:
            at=a*512;limit=(b+1)*512
            while at<limit:
                try:data=os.lseek(f.fileno(),at,os.SEEK_DATA)
                except OSError as e:
                    if e.errno==errno.ENXIO:break
                    raise
                if data>=limit:break
                hole=min(limit,os.lseek(f.fileno(),data,os.SEEK_HOLE));assert data%512==0 and hole%512==0
                at=data
                while at<hole:
                    n=min(1<<20,hole-at);buf=os.pread(f.fileno(),n,at);assert len(buf)==n
                    for j in range(0,n,512):
                        sec=buf[j:j+512]
                        if any(sec):found[(at+j)//512]=hashlib.sha256(sec).hexdigest()
                    at+=n
    assert found==expected,'Foreign partition content changed outside the selected gap'
    return {'all_foreign_bytes_verified':True,'method':'SEEK_DATA/HOLE zero holes plus every allocated 512B sector','nonzero_foreign_sectors':len(found),'sha256':hashlib.sha256(repr(sorted(found.items())).encode()).hexdigest()}
