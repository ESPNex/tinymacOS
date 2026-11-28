#!/usr/bin/env python3
"""Verify the shipped raw-run UDIF independently of its generator.
Not a general DMG reader: compressed/encrypted/split images are not claimed.
Usage: python3 tests/baseimage8_verify.py /external/path/BaseSystem.dmg
"""
import hashlib,json,plistlib,struct,subprocess,sys,tempfile,zlib
from pathlib import Path

def verify(path):
    data=Path(path).read_bytes();assert len(data)>=512
    footer=data[-512:];magic,version,header,flags=struct.unpack_from('>4sIII',footer)
    assert (magic,version,header)==(b'koly',4,512)
    assert struct.unpack_from('>I',footer,56)[0]==1,'split DMG not accepted'
    data_start,data_length=struct.unpack_from('>QQ',footer,24)
    xml_start,xml_length=struct.unpack_from('>QQ',footer,216)
    assert data_start==0 and data_length<=len(data)-512
    assert xml_start>=data_length and xml_length<=len(data)-512-xml_start
    sectors=struct.unpack_from('>Q',footer,492)[0];assert sectors*512==data_length
    checksum_kind,checksum_bits,checksum=struct.unpack_from('>III',footer,80)
    assert (checksum_kind,checksum_bits)==(2,32)
    assert zlib.crc32(data[:data_length])==checksum
    plist=plistlib.loads(data[xml_start:xml_start+xml_length])
    blocks=plist['resource-fork']['blkx'];assert len(blocks)==1
    block=blocks[0]['Data'];assert block[:4]==b'mish'
    block_version,start,count,offset=struct.unpack_from('>IQQQ',block,4)
    assert (block_version,start,count,offset)==(1,0,sectors,0)
    assert struct.unpack_from('>III',block,64)==(2,32,checksum)
    nruns=struct.unpack_from('>I',block,200)[0];assert 204+nruns*40==len(block)
    reconstructed=bytearray();expected=0;ended=False
    for i in range(nruns):
        kind,reserved,first,n,off,length=struct.unpack_from('>IIQQQQ',block,204+40*i)
        assert reserved==0 and first==expected and not ended
        if kind==0xffffffff:
            assert i==nruns-1 and n==0 and length==0;ended=True;continue
        assert kind==1,'only raw UDIF extents verified by this test'
        assert n>0 and n*512==length and off<=data_length and length<=data_length-off
        reconstructed+=data[off:off+length];expected+=n
    assert ended and expected==sectors and reconstructed==data[:data_length]
    # qemu-img's independent DMG driver must detect AND decode the DMG.
    info=json.loads(subprocess.check_output(['qemu-img','info','--output=json',str(path)]))
    assert info['format']=='dmg' and info['virtual-size']==sectors*512
    with tempfile.TemporaryDirectory(prefix='tmc-dmg8-',dir='/var/tmp') as directory:
        raw=Path(directory)/'independent.raw'
        subprocess.run(['qemu-img','convert','-f','dmg','-O','raw',str(path),str(raw)],check=True)
        assert raw.read_bytes()==reconstructed
        listing=subprocess.check_output(['mdir','-i',str(raw),'::/'],text=True)
        assert 'RECOVERY ELF' in listing and 'RECOVERY VOL' in listing,listing
    return {'format':'UDIF v4, raw extents, FAT16','bytes':len(data),'logical_bytes':len(reconstructed),'raw_runs':nruns-1,'sha256':hashlib.sha256(data).hexdigest(),'independent_decoder':'qemu-img: autodetected dmg; full decode byte-identical; FAT16 mdir lists RECOVERY.ELF and RECOVERY.VOL','scope':'Validation of existing BaseSystem generator, not completion of native hdiutil or support for arbitrary DMG variants'}
if __name__=='__main__':print(json.dumps(verify(Path(sys.argv[1])),indent=2))
