#!/usr/bin/env python3
"""Inventory actual packed namespace and FAT16 installer ESP, not build guesses.
Read-only: emits JSON; optional installed raw disk verifies on-disk ESP and
inventories actual Recovery FAT. Never mounts, formats or writes an input.
"""
import argparse,hashlib,json,struct
from pathlib import Path
ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('build',type=Path);ap.add_argument('--installed-disk',type=Path);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args()
def sha(b):return hashlib.sha256(b).hexdigest()
def catalog(p):
 b=p.read_bytes();assert b[:8]==b'TMVOL2\0\0' and sha(b[:-32])==b[-32:].hex();n,z=struct.unpack_from('<II',b,8);assert z==64 and 16+n*z<=len(b)-32;paths=[];out=[]
 for i in range(n):
  parent,directory,off,size,name=struct.unpack_from('<iIII48s',b,16+i*z);name=name.split(b'\0')[0].decode();assert directory in (0,1) and (not i or 0<=parent<i);path='/' if not i else paths[parent].rstrip('/')+'/'+name;paths.append(path);assert directory or 16+n*z<=off<=off+size<=len(b)-32
  out.append({'path':path,'directory':bool(directory),'bytes':size,'dot_name':name.startswith('.'),**({} if directory else {'sha256':sha(b[off:off+size])})})
 return {'sha256':sha(b),'entries':out,'count':n,'runtime_injected_actual_objects':['/System/Library/KernelCollections/KernelCache','/System/Library/CoreServices/boot.efi']}
def fat(b):
 assert len(b)>=512 and b[510:512]==b'\x55\xaa';bps=struct.unpack_from('<H',b,11)[0];spc=b[13];reserved=struct.unpack_from('<H',b,14)[0];nf=b[16];roots=struct.unpack_from('<H',b,17)[0];spf=struct.unpack_from('<H',b,22)[0];total=struct.unpack_from('<H',b,19)[0] or struct.unpack_from('<I',b,32)[0];assert bps==512 and spc and not spc&(spc-1) and reserved and nf in (1,2) and roots and spf and total*512<=len(b)
 root=(reserved+nf*spf)*512;data=root+((roots*32+511)//512)*512;clusters=(total*512-data)//(spc*512);assert 4085<=clusters<65525
 active=set();out=[]
 def chain(c):
  seen=set();parts=[]
  while c<0xfff8:
   assert 2<=c<clusters+2 and c not in seen;seen.add(c);parts.append(b[data+(c-2)*spc*512:data+(c-1)*spc*512]);c=struct.unpack_from('<H',b,reserved*512+c*2)[0]
  return b''.join(parts)
 def scan(raw,prefix,depth):
  assert depth<32;lfn={};checksum=None
  for off in range(0,len(raw),32):
   e=raw[off:off+32]
   if not e[0]:break
   if e[0]==0xe5:lfn={};continue
   attr=e[11]
   if attr==15:
    index=e[0]&31
    if e[0]&64:lfn={};checksum=e[13]
    assert index and e[12]==0 and e[26:28]==b'\0\0'
    lfn[index]=e[1:11]+e[14:26]+e[28:32];continue
   if attr&8:lfn={};continue
   stem=e[:8].decode('ascii').rstrip();ext=e[8:11].decode('ascii').rstrip();name=stem+('.'+ext if ext else '')
   check=0
   for ch in e[:11]:check=(((check&1)<<7)+(check>>1)+ch)&255
   if lfn and checksum==check and set(lfn)==set(range(1,max(lfn)+1)):
    name=b''.join(lfn[i] for i in sorted(lfn)).decode('utf-16le').split('\0')[0].rstrip('\uffff')
   lfn={}
   if name in ('.','..'):continue
   c=struct.unpack_from('<H',e,26)[0];size=struct.unpack_from('<I',e,28)[0];directory=bool(attr&16);path=prefix+'/'+name;payload=chain(c) if c>=2 else b'';assert directory or size<=len(payload)
   out.append({'path':path,'directory':directory,'bytes':size,'dot_name':name.startswith('.'),'fat_hidden':bool(attr&2),'fat_system':bool(attr&4),**({} if directory else {'sha256':sha(payload[:size])})})
   if directory:
    assert c>=2 and c not in active;active.add(c);scan(payload,path,depth+1);active.remove(c)
 scan(b[root:root+roots*32],'',0);return out
r={'build':str(a.build),'catalogs':{m:catalog(a.build/f'System-{m}.vol') for m in ('disk','live','recovery')},'installer_esp':fat((a.build/'install.esp').read_bytes()),'scope':{'catalog_dotfiles_filtered':False,'finder_app_contents_action':'ACT_PACKAGE / Mostra contenuto pacchetto','private_account_slots_are_files':False,'build_intermediates_installed':False,'efi_recovery_fat_mounted_in_finder':False,'visibility_requirement_fully_met':False}}
if a.installed_disk:
 assert str(a.installed_disk.resolve()).startswith('/var/tmp/'),'Only disposable QA images may be audited as installed media'
 with a.installed_disk.open('rb') as f:
  f.seek(1024);p=f.read(512);ranges=[struct.unpack_from('<QQ',p,i*128+32) for i in (0,1,2,3)];esp,_,rec,_=ranges
  f.seek(esp[0]*512);eb=f.read((esp[1]-esp[0]+1)*512);f.seek(rec[0]*512);rb=f.read((rec[1]-rec[0]+1)*512)
 r['installed_disk']={'path':str(a.installed_disk),'esp_matches_build_payload':eb==(a.build/'install.esp').read_bytes(),'esp':fat(eb),'recovery':fat(rb)}
r['build_only_examples']=[p.name for p in sorted(a.build.iterdir()) if p.suffix in ('.o','.debug','.map','.so')]
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(r,indent=2)+'\n');print(json.dumps({'catalog_entries':{k:v['count'] for k,v in r['catalogs'].items()},'esp_entries':len(r['installer_esp']),'esp_dotfiles':[e['path'] for e in r['installer_esp'] if e['dot_name']],'fat_hidden_files':[e['path'] for e in r['installer_esp'] if e['fat_hidden']],'visibility_requirement_fully_met':False},indent=2))
