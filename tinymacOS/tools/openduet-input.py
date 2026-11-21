#!/usr/bin/env python3
"""Derive optical OpenDuet Block I/O + USB input from pinned upstream images.

The 1.0.8 Block I/O FV omits UHCI/EHCI/UsbBus. XhciDxe/OpenUsbKbDxe
alone cannot publish USB_IO/TextInput without UsbBus. Copy three entire,
unmodified FFS modules from the matching full OpenDuet image into free FV
space. Preserve BIOS Block I/O (MEMDISK boot), all existing FFS files, and
installed-disk firmware. Rebuild only the LZMA container, EFILDR lengths and
the size-dependent startup page-table metadata, as upstream build_duet.tool.
No network, extra Python packages, assembler, or physical device writes.

Upstream layout/algorithm: OpenCorePkg 1.0.8 build_duet.tool,
Utilities/BaseTools/{EfiLdrImage,GenPage}.c and OpenDuetPkg.fdf.
Module code is redistributed unchanged under the existing upstream license.
"""
from pathlib import Path
import argparse,hashlib,json,lzma,struct,uuid

PREFIX=7680 # StartX64.com + EfiX64.com, minus first 512-byte sector (upstream dd).
MODULES={
 'UhciDxe':('2fb92efa-2ee0-4bae-9eb6-7464125e1ef7','29716f8761dfaac6db6bc1f31b03c53f253cbeedeb32f01f71cf5da1c67c3f78'),
 'EhciDxe':('bdfe430e-8f2a-4db0-9991-6f856594777e','72ce65b0d398849c511aeb4f2e38124f1ab9e8450dd8b2e236e5f3de7eec6c7f'),
 'UsbBusDxe':('240612b7-a063-11d4-9a3a-0090273fc14d','5b5c7b811e99a61ed944b38581aff99267508bc1ea01f64a1682ec6aea1c8ad0'),
}
def sha(b):return hashlib.sha256(b).hexdigest()
def require(ok,message):
 if not ok:raise ValueError(message)
def tables(base):
 t=bytearray(6*4096);struct.pack_into('<Q',t,0,base+4096|3)
 for j in range(4):
  struct.pack_into('<Q',t,4096+8*j,base+(2+j)*4096|3)
  for k in range(512):struct.pack_into('<Q',t,(2+j)*4096+8*k,((j*512+k)<<21)|0x83)
 return t
def unpack_boot(blob):
 require(len(blob)>PREFIX+272 and blob[PREFIX:PREFIX+4]==b'EFIL','EFILDR signature')
 e=blob[PREFIX:];length,count=struct.unpack_from('<II',e,8)
 require(count==4 and 272<=length<=len(e),'EFILDR bounds/count')
 _,off,n,name=struct.unpack_from('<III52s',e,16+3*64)
 require(272<=off and off+n==length and name.startswith(b'DUETEFIMAINFV'),'FV payload bounds/name')
 raw=lzma.decompress(e[off:off+n],format=lzma.FORMAT_ALONE)
 require(len(raw)==524288 and raw[40:44]==b'_FVH','pinned FV size/signature')
 hdr=struct.unpack_from('<H',raw,48)[0]
 require(hdr==72 and sum(struct.unpack_from('<36H',raw))&65535==0,'FV header checksum')
 require(blob[152]==0xb8 and blob[157:160]==bytes.fromhex('0f22d8'),'startup CR3 operand')
 cr3=struct.unpack_from('<I',blob,153)[0];ptoff=cr3-0x20000-512
 require(cr3==0x20000+((length+8192+4095)&~4095),'upstream size-derived CR3')
 require(blob[ptoff:]==tables(cr3),'upstream identity page tables')
 return raw,off,length,cr3

def ffs_files(volume):
 at=72;files={}
 while at+24<=len(volume) and volume[at:at+24]!=b'\xff'*24:
  size=int.from_bytes(volume[at+20:at+23],'little')
  require(24<=size<0xffffff and at+size<=len(volume),'FFS bounds')
  f=volume[at:at+size];key=str(uuid.UUID(bytes_le=bytes(f[:16])))
  require(key not in files,'duplicate FFS GUID')
  require((sum(f[:24])-f[17]-f[23])&255==0,'FFS header checksum')
  files[key]=bytes(f);at=(at+size+7)&~7
 require(volume[at:]==b'\xff'*(len(volume)-at),'FV free space')
 return files,at

def derive(full,block):
 fv,_,_,_=unpack_boot(full);bv,off,_,_=unpack_boot(block)
 source,_=ffs_files(fv);original,end=ffs_files(bv);volume=bytearray(bv);copied=[]
 for guid in original.keys()&source.keys():require(original[guid]==source[guid],'mismatched upstream common FFS')
 for name,(guid,digest) in MODULES.items():
  require(guid not in original and guid in source,'missing/already present USB FFS')
  f=source[guid];require(sha(f)==digest and f[18]==7,'pinned USB driver FFS digest/type')
  require(end+len(f)<=len(volume),'insufficient FV space')
  volume[end:end+len(f)]=f;end=(end+len(f)+7)&~7
  copied.append({'name':name,'guid':guid,'ffs_bytes':len(f),'ffs_sha256':digest})
 final,_=ffs_files(volume)
 require(final==original|{g:source[g] for g,_ in MODULES.values()},'FFS preservation')
 compressed=bytearray(lzma.compress(volume,format=lzma.FORMAT_ALONE,preset=7))
 # Firmware's LzmaUefiDecompressGetInfo requires an explicit uncompressed size.
 compressed[5:13]=struct.pack('<Q',len(volume))
 require(lzma.decompress(compressed,format=lzma.FORMAT_ALONE)==volume,'LZMA round trip')
 e=bytearray(block[PREFIX:PREFIX+off])+compressed
 struct.pack_into('<I',e,8,len(e));struct.pack_into('<I',e,16+3*64+8,len(compressed))
 prefix=bytearray(block[:PREFIX]);cr3=0x20000+((len(e)+8192+4095)&~4095)
 require(cr3+6*4096<0x90000,'startup low-memory bound')
 struct.pack_into('<I',prefix,153,cr3)
 # Only the documented PAGE_TABLE immediate changes in the upstream startup.
 require(prefix[:153]==block[:153] and prefix[157:]==block[157:PREFIX],'startup preservation')
 result=prefix+e;ptoff=cr3-0x20000-512
 require(len(result)<=ptoff,'page-table overlap')
 result+=bytes(ptoff-len(result));result+=tables(cr3)
 check,_,_,_=unpack_boot(result);require(check==volume,'derived firmware validation')
 return bytes(result),{'derived_sha256':sha(result),'bytes':len(result),'source_full_sha256':sha(full),'source_blockio_sha256':sha(block),'original_ffs_preserved':len(original),'copied_modules':copied,'page_table_address':cr3,'upstream_module_code_unchanged':True,'installed_disk_firmware_unchanged':True}

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('output',type=Path);a=ap.parse_args()
 output=a.output.resolve();require(str(output).startswith('/var/tmp/') and not output.exists(),'NEW external regular output required')
 vendor=Path(__file__).resolve().parents[1]/'vendor/opencore-1.0.8';lock=json.loads((vendor/'vendor-lock.json').read_text())
 images={n:(vendor/n).read_bytes() for n in ['bootX64','bootX64-blockio']}
 for name,b in images.items():require(sha(b)==lock['files'][name],'upstream source hash: '+name)
 blob,record=derive(images['bootX64'],images['bootX64-blockio']);output.write_bytes(blob)
 output.with_name(output.name+'.input.json').write_text(json.dumps(record,indent=2)+'\n')
 print('Optical OpenDuet Block I/O + unmodified UHCI/EHCI/UsbBus:',record['derived_sha256'])
if __name__=='__main__':main()
