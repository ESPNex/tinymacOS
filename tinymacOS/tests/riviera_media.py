"""Independent readers of released ISO/FAT, Mach-O, GPT and data structures."""
import hashlib,os,plistlib,struct,subprocess,zlib
from pathlib import Path
from guest import audit_bytes,ROOT
H=lambda b:hashlib.sha256(b).hexdigest();DATA=51200;SZ=1680*512

def gpt(path):
 z=path.stat().st_size//512
 with path.open('rb') as f:
  def read(l,n):f.seek(l*512);return f.read(n*512)
  def hdr(l):
   h=bytearray(read(l,1));assert h[:8]==b'EFI PART' and struct.unpack_from('<II',h,8)==(65536,92);crc=struct.unpack_from('<I',h,16)[0];h[16:20]=bytes(4);assert zlib.crc32(h[:92])==crc;assert struct.unpack_from('<4Q',h,24)==(l,z-1 if l==1 else 1,34,z-34);a,n,s,crc=struct.unpack_from('<QIII',h,72);assert n==s==128;p=read(a,32);assert zlib.crc32(p)==crc;return p,bytes(h[56:72])
  p,id=hdr(1);assert (p,id)==hdr(z-1);assert read(0,1)[510:]==b'\x55\xaa';parts=[struct.unpack_from('<QQ',p,j*128+32) for j in range(4)];src=z-33-49152-1;rec=src-20480;assert parts==[(2048,51199),(51200,rec-1),(rec,src-1),(src,z-34)];s=read(src,1);assert s[:24]==b'TMCSYS20'+id and struct.unpack_from('<II',s,24)==(49152,1);source=read(src+1,49152);assert s[32:64]==hashlib.sha256(source).digest();return dict(esp=read(2048,49152),rec=read(rec,20480),src=source,parts=parts,id=id,data=read(DATA,1680),seal=read(DATA+57343,1),snapshot=read(DATA+57344,6288))

def medium(iso,work,check):
 work.mkdir(parents=True,exist_ok=True);env=dict(os.environ);prefix=env.get('TOOLROOT','');env.update(PATH=prefix+'/usr/bin:'+env['PATH'],LD_LIBRARY_PATH=prefix+'/usr/lib/x86_64-linux-gnu:'+env.get('LD_LIBRARY_PATH',''))
 def run(*a):return subprocess.run(a,env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,check=True).stdout
 def extract(src,name):run('xorriso','-osirrox','on','-indev',str(iso),'-extract',src,str(work/name));return (work/name).read_bytes()
 def fat(img,src,name):run('mcopy','-o','-i',str(work/img),'::'+src,str(work/name));return (work/name).read_bytes()
 extract('/EFI.IMG','opt.fat');dmg=extract('/Recovery/BaseSystem.dmg','BaseSystem.dmg');efi=extract('/EFI/BOOT/BOOTX64.EFI','outer.efi');kc=extract('/System/Library/KernelCollections/KernelCache','outer.cache');esp=fat('opt.fat','/Install.esp','disk.esp')
 assert fat('opt.fat','/Recovery/BaseSystem.dmg','embedded.dmg')==dmg
 prefix=fat('opt.fat','/Recovery/Volume.hdr','prefix.bin');assert len(prefix)%512==0 and len(prefix)+len(dmg)<10<<20
 rec=(prefix+dmg).ljust(10<<20,b'\0');(work/'rec-hd.fat').write_bytes(rec)
 assert fat('rec-hd.fat','/Recovery/BaseSystem.dmg','source.dmg')==dmg and hashlib.sha256(dmg).digest() in efi and hashlib.sha256(dmg).digest() in fat('rec-hd.fat','/EFI/BOOT/BOOTX64.EFI','inner-loader.efi')
 assert fat('opt.fat','/Recovery/Payload.sha','payload.sha')==hashlib.sha256(esp).digest()+hashlib.sha256(rec).digest();check('Un solo CD: DMG visibile, file di avvio El Torito e file nella sorgente Recovery HD identici; SHA-256 nei due loader e manifest installazione verificati')
 for f in ('opt.fat','disk.esp'):
  entry=fat(f,'/EFI/BOOT/BOOTX64.EFI',f+'.efi')
  if f=='disk.esp' and entry!=efi:
   assert entry==(ROOT/'vendor/opencore-1.0.8/BOOTx64.efi').read_bytes()
   assert fat(f,'/EFI/TMC/BOOTX64.EFI','installed-native.efi')==efi
   assert fat(f,'/EFI/OC/OpenCore.efi','installed-opencore.efi')==(ROOT/'vendor/opencore-1.0.8/OpenCore.efi').read_bytes()
  else:assert entry==efi
  assert fat(f,'/System/Library/KernelCollections/KernelCache',f+'.kc')==kc
 assert kc[:8]==b'TMKCACHE' and struct.unpack_from('<IIQQ',kc,8)==(2,62,64,len(kc)-64) and kc[32:64]==hashlib.sha256(kc[64:]).digest() and kc[64:68]==b'\x7fELF';check('KernelCache: vero payload ELF64, dimensione e SHA-256 corretti; stesso file su live e installato')
 def catalog(f):
  raw=fat(f,'/System/Library/CoreServices/System.vol',f+'.vol');v,h=raw[:-32],raw[-32:];assert h==hashlib.sha256(v).digest();magic,n,st=struct.unpack_from('<8sII',v);assert magic==b'TMVOL2\0\0' and st==64;paths=[];out={}
  for i in range(n):
   p,d,o,z,name=struct.unpack_from('<iIII48s',v,16+i*64);name=name.split(b'\0')[0].decode();q=(paths[p]+'/'+name) if i else '';paths.append(q);assert q not in out and (d or 16+64*n<=o<=o+z<=len(v));out[q]=None if d else v[o:o+z]
  return out
 live,disk=catalog('opt.fat'),catalog('disk.esp');short='/Applications/Install tinymacOS.app';ins=live[short+'/Contents/MacOS/Install tinymacOS'];assert set(disk)<=set(live) and all(p.startswith(short) for p in set(live)-set(disk)) and ins not in esp;check('Il solo delta del catalogo live è il bundle installatore; il suo eseguibile non è nei byte dell’ESP installato')
 cli=(ROOT/'include/commands.def').read_text().splitlines();assert all(disk.get(p) and disk[p][:4]==b'\xcf\xfa\xed\xfe' for p in cli);pl=[p for p in disk if p.endswith('/Contents/Info.plist')];kx=[p for p in pl if p.startswith('/System/Library/Extensions/')];ap=[p for p in pl if p not in kx];assert len(ap)==13 and len(kx)==2, (len(ap),len(kx));ex={p[:-10]+'MacOS/'+plistlib.loads(disk[p])['CFBundleExecutable'] for p in ap};assert len(ex)==13;ke={p[:-10]+'MacOS/'+plistlib.loads(disk[p])['CFBundleExecutable'] for p in kx};assert len(ke)==2
 for p in ke:
  raw=disk[p];assert raw[:4]==b'\x7fELF' and raw[4]==2 and struct.unpack_from('<HH',raw,16)==(1,62), p  # ELF64 x86-64 ET_REL
 for p in list(ex)+cli+['/System/Library/CoreServices/WindowServer']:
  raw=disk[p];magic,cpu,_,typ,n,size,*_=struct.unpack_from('<8I',raw);assert (magic,cpu,typ)==(0xfeedfacf,0x1000007,2);offset=32;entry=None;segs=[]
  for i in range(n):
   c,z=struct.unpack_from('<II',raw,offset);assert z>=8 and offset+z<=32+size
   if c==0x19:
    a,m,o,l,_,prot=struct.unpack_from('<QQQQii',raw,offset+24);assert m>=l and len(raw)>=o+l and not (prot&4 and prot&2);segs.append((o,l,prot))
   if c==0x80000028:entry=struct.unpack_from('<Q',raw,offset+8)[0]
   offset+=z
  assert offset==32+size and any(o<=entry<o+l and prot&4 for o,l,prot in segs)
 check(f'{len(ap)+len(kx)} bundle installati con Info.plist (13 app Mach-O x86-64, senza browser + 2 kext ELF64 ET_REL); {len(cli)} percorsi Unix eseguibili; segmenti W^X e LC_MAIN verificati')
 assert not any(p.endswith(('.c','.h','.S','.asm','.debug','.map','.o','.zip','.py')) for p in live);vol=next(v for k,v in disk.items() if k.endswith('WindowServer'));assert all(x not in b''.join(filter(None,live.values()))+kc+efi for x in (b'[spawn]',b'frames=',b'verified bytes=',b'EFIDebug',b'sample ticks=',b'[probe]',b' cr3='));check('Nessun sorgente o eseguibile di diagnostica nei cataloghi; assenti le note firme di tracing del vecchio guest')
 # Read the UDIF extent map, not a filename extension or a presumed fixed offset.
 k=dmg[-512:];assert k[:4]==b'koly' and struct.unpack_from('>II',k,4)==(4,512)
 xml,xlen=struct.unpack_from('>QQ',k,216);sectors,=struct.unpack_from('>Q',k,492);assert xml+xlen==len(dmg)-512
 table=plistlib.loads(dmg[xml:xml+xlen])['resource-fork']['blkx'][0]['Data'];assert table[:4]==b'mish';n,=struct.unpack_from('>I',table,200)
 raw=bytearray();assert len(table)==204+40*n
 for i in range(n):
  kind,_,start,count,off,z=struct.unpack_from('>IIQQQQ',table,204+40*i)
  if i==n-1:assert kind==0xffffffff;break
  assert kind==1 and off+z<=xml and 512*count==z and 512*start==len(raw);raw+=dmg[off:off+z]
 assert len(raw)==sectors*512 and zlib.crc32(raw)==struct.unpack_from('>I',k,88)[0]==struct.unpack_from('>I',table,72)[0];(work/'base.fat').write_bytes(raw)
 if os.environ.get('DMG2IMG'):
  run(os.environ['DMG2IMG'],'-s','-i',str(work/'BaseSystem.dmg'),'-o',str(work/'independent.raw'));assert (work/'independent.raw').read_bytes()==raw;check('dmg2img indipendente ha decodificato BaseSystem.dmg: volume FAT16 identico settore per settore')
 r_efi=fat('base.fat','/EFI/BOOT/BOOTX64.EFI','recovery.efi');lst=run('mdir','-i',str(work/'base.fat'),'-/','-b','::/').decode().strip().splitlines();assert len(lst)==3,lst;assert r_efi[:2]==b'MZ' and all(x not in r_efi for x in (b'TMVOL2',b'\xcf\xfa\xed\xfe\x07\x00\x00\x01',b'EFIDebug',b'WindowServer',b'Installa tinymacOS'));check('BaseSystem UDIF contiene solo il runtime UEFI di Recupero, niente installer o applicazioni desktop')
 return dict(esp=esp,rec=rec,dmg=dmg,install=ins,ks=H(kc),sha=H(iso.read_bytes()))
