/* SPDX-License-Identifier: MIT
 * Monocypher XChaCha20-Poly1305 block envelope, fresh volumes only.
 * Flush must persist all preceding writes. Each record owns whole physical
 * 4KiB units, so a metadata RMW never touches another block's ciphertext.
 * Journal stores BOTH ciphertext and metadata before either target is written.
 * Nonce ranges reach BOTH redundant headers before a nonce is issued. Reserved
 * ranges are burned after every reopen. Does not provide anti-rollback: an old
 * valid same-index record or a whole-medium image can still be replayed.
 */
#include "vault2.h"
#include "sha256.h"
#include "monocypher/monocypher.h"
typedef struct PACK {u8 magic[8];u64 block,nonce;u8 uuid[16],tag[16],pad[FV2_UNIT-56];} Meta;
typedef struct PACK {u8 magic[8];u64 block,nonce;u8 mac[32],pad[FV2_UNIT-56];} Journal;
_Static_assert(sizeof(Meta)==FV2_UNIT,"vault2 record metadata ABI");
_Static_assert(sizeof(Journal)==FV2_UNIT,"vault2 journal ABI");
static int zero(const void*p,size_t n){const u8*b=p;u8 z=0;while(n--)z|=*b++;return !z;}
static int fail(Vault2*v,int code){v->poison=1;return code;}
static void*al(Vault2*v,size_t n){return v->io.alloc(v->io.context,n);}
static void fr(Vault2*v,void*p,size_t n){if(p){erase(p,n);v->io.free(v->io.context,p);}}
static int rd(Vault2*v,u64 n,void*p){return n<v->io.units&&!v->io.read(v->io.context,n,p)?0:fail(v,FV_IO);}
static int wr(Vault2*v,u64 n,const void*p){return n<v->io.units&&!v->io.write(v->io.context,n,p)&&!v->io.flush(v->io.context)?0:fail(v,FV_IO);}
u64 vault2_capacity(u64 units){if(units<FV2_START+5)return 0;u64 n=(units-FV2_START)/5;return n<=FV2_MAX_BLOCKS?n:0;}
static void hmac(const Fv2Header*h,const u8 key[32],u8 out[32]){crypto_blake2b_keyed(out,32,key,32,(const u8*)h,240);}
static int valid(const Fv2Header*h,u64 units){
 if(memcmp(h->magic,"TMCFV003",8)||h->version!=3||h->state!=FV_ON||h->units!=units||h->blocks!=vault2_capacity(units)||!h->blocks||!h->serial||!h->ceiling||!zero(h->reserved,sizeof h->reserved))return 0;
 u8 sum[32];sha_bytes(h,480,sum);int ok=!crypto_verify32(sum,h->sum);erase(sum,32);return ok;
}
static int save(Vault2*v){
 if(v->poison||v->h.serial==~0ull)return FV_STATE;
 u8*p=al(v,FV2_UNIT);if(!p)return fail(v,FV_IO);memset(p,0,FV2_UNIT);
 v->h.serial++;hmac(&v->h,v->key,v->h.mac);sha_bytes(&v->h,480,v->h.sum);memcpy(p,&v->h,512);
 int r=wr(v,1,p)||wr(v,0,p)?FV_IO:0;fr(v,p,FV2_UNIT);return r;
}
int vault2_probe(Vault2*v,Fv2Io io){
 if(!v)return FV_STATE;erase(v,sizeof *v);v->io=io;
 if(!io.read||!io.write||!io.flush||!io.alloc||!io.free||!vault2_capacity(io.units))return fail(v,FV_SPACE);
 u8*p=al(v,FV2_UNIT);if(!p)return fail(v,FV_IO);Fv2Header h[2];int good[2],blank[2];
 for(unsigned i=0;i<2;i++){
  if(rd(v,i,p)){fr(v,p,FV2_UNIT);return FV_IO;}
  memcpy(h+i,p,512);blank[i]=zero(p,FV2_UNIT);good[i]=valid(h+i,io.units)&&zero(p+512,FV2_UNIT-512);
 }
 fr(v,p,FV2_UNIT);
 if(!good[0]&&!good[1]){if(blank[0]&&blank[1])return 0;v->present=1;return fail(v,FV_CORRUPT);}
 if(good[0]&&good[1]&&h[0].serial==h[1].serial&&memcmp(h,h+1,512)){v->present=1;return fail(v,FV_CORRUPT);}
 v->h=good[0]&&(!good[1]||h[0].serial>=h[1].serial)?h[0]:h[1];v->present=1;return 1;
}
static void aad(const Vault2*v,const char domain[8],u64 id,u8 out[32]){memcpy(out,v->h.uuid,16);memcpy(out+16,domain,8);memcpy(out+24,&id,8);}
static int derive(const char*pass,const FvSlot*s,int recovery,u8 out[32]){
 if(!pass)return FV_AUTH;size_t n=strlen(pass);if(!n||n>64)return FV_AUTH;
 if(recovery){
  if(n!=64)return FV_AUTH;u8 raw[32];
  for(unsigned i=0;i<32;i++){
   int a=pass[2*i],b=pass[2*i+1];a=a>='0'&&a<='9'?a-'0':a>='a'&&a<='f'?a-'a'+10:a>='A'&&a<='F'?a-'A'+10:-1;b=b>='0'&&b<='9'?b-'0':b>='a'&&b<='f'?b-'a'+10:b>='A'&&b<='F'?b-'A'+10:-1;
   if(a<0||b<0){erase(raw,32);return FV_AUTH;}raw[i]=(a<<4)|b;
  }
  crypto_blake2b_keyed(out,32,raw,32,s->salt,16);erase(raw,32);
 }else{PBKDF p;pbk_begin(&p,(const u8*)pass,n,s->salt);pbk_run(&p,FV2_ROUNDS-1);memcpy(out,p.sum,32);erase(&p,sizeof p);}
 return 0;
}
static int unwrap(Vault2*v,const char*pass,u8 out[32]){
 for(int slot=1;slot>=0;slot--){
  u8 k[32],ad[32],mac[32];aad(v,"FV2KEY03",slot,ad);if(derive(pass,v->h.slot+slot,slot,k))continue;
  int r=crypto_aead_unlock(out,v->h.slot[slot].tag,k,v->h.slot[slot].nonce,ad,32,v->h.slot[slot].wrapped,32);erase(k,32);
  if(!r){hmac(&v->h,out,mac);r=crypto_verify32(mac,v->h.mac);erase(mac,32);if(!r)return 0;}erase(out,32);
 }
 return FV_AUTH;
}
static int wrap(Vault2*v,unsigned slot,const char*pass,const u8 random[40]){
 FvSlot*s=v->h.slot+slot;u8 k[32],ad[32];memcpy(s->salt,random,16);memcpy(s->nonce,random+16,24);aad(v,"FV2KEY03",slot,ad);
 int r=derive(pass,s,slot,k);if(!r)crypto_aead_lock(s->wrapped,s->tag,k,s->nonce,ad,32,v->key,32);erase(k,32);return r;
}
static void hex(const u8*p,char*out){for(unsigned i=0;i<32;i++){out[2*i]="0123456789ABCDEF"[p[i]>>4];out[2*i+1]="0123456789ABCDEF"[p[i]&15];}out[64]=0;}
static void nonce(Vault2*v,u64 counter,u8 out[24]){memcpy(out,v->h.uuid,16);memcpy(out+16,&counter,8);}
static int reserve(Vault2*v,u64*out){
 if(v->next>=v->h.ceiling){if(v->h.ceiling>~0ull-1024)return FV_STATE;v->next=v->h.ceiling;v->h.ceiling+=1024;int r=save(v);if(r)return r;}
 *out=v->next++;return 0;
}
static int meta_valid(Vault2*v,const Meta*m,u64 block){return !memcmp(m->magic,"FV2BLK03",8)&&m->block==block&&m->nonce&&m->nonce<v->h.ceiling&&!memcmp(m->uuid,v->h.uuid,16)&&zero(m->pad,sizeof m->pad);}
static void jmac(Vault2*v,const Journal*j,const u8*p,u8 out[32]){
 crypto_blake2b_ctx c;crypto_blake2b_keyed_init(&c,32,v->key,32);crypto_blake2b_update(&c,v->h.uuid,16);crypto_blake2b_update(&c,(const u8*)j,24);crypto_blake2b_update(&c,p,FV2_RECORD);crypto_blake2b_final(&c,out);erase(&c,sizeof c);
}
static int jvalid(Vault2*v,const Journal*j,const u8*p){
 const Meta*m=(const void*)(p+FV2_BLOCK);
 if(memcmp(j->magic,"FV2TXN03",8)||j->block>=v->h.blocks||!j->nonce||j->nonce>=v->h.ceiling||!zero(j->pad,sizeof j->pad)||!meta_valid(v,m,j->block)||m->nonce!=j->nonce)return 0;
 u8 mac[32];jmac(v,j,p,mac);int ok=!crypto_verify32(mac,j->mac);erase(mac,32);return ok;
}
static int apply(Vault2*v,const Journal*j,const u8*p,u8*scratch){
 u64 first=FV2_START+j->block*5;
 for(unsigned i=0;i<5;i++)if(wr(v,first+i,p+i*FV2_UNIT))return FV_IO;
 memset(scratch,0,FV2_UNIT);return wr(v,2,scratch)||wr(v,3,scratch)?FV_IO:0;
}
static int replay(Vault2*v){
 u8*p=al(v,FV2_RECORD),*a=al(v,FV2_UNIT),*b=al(v,FV2_UNIT);int r=FV_IO;
 if(!p||!a||!b){fail(v,FV_IO);goto done;}
 if(rd(v,2,a)||rd(v,3,b))goto done;
 if(zero(a,FV2_UNIT)&&zero(b,FV2_UNIT)){r=0;goto done;}
 for(unsigned i=0;i<5;i++)if(rd(v,4+i,p+i*FV2_UNIT))goto done;
 int av=jvalid(v,(Journal*)a,p),bv=jvalid(v,(Journal*)b,p);
 if(!av&&!bv){
  if(zero(a,FV2_UNIT)||zero(b,FV2_UNIT)){memset(a,0,FV2_UNIT);r=wr(v,2,a)||wr(v,3,a)?FV_IO:0;goto done;}
  r=fail(v,FV_CORRUPT);goto done;
 }
 if(av&&bv&&memcmp(a,b,FV2_UNIT)){r=fail(v,FV_CORRUPT);goto done;}
 /* Scratch must not alias the chosen descriptor before its target is read. */
 r=apply(v,(Journal*)(av?a:b),p,av?b:a);
 done:fr(v,p,FV2_RECORD);fr(v,a,FV2_UNIT);fr(v,b,FV2_UNIT);return r;
}
int vault2_create(Vault2*v,const char*pass,const u8 random[128],char recovery[65]){
 if(!v||v->poison||v->present||!vault2_capacity(v->io.units))return FV_STATE;
 if(!pass||strlen(pass)<8||strlen(pass)>64||!recovery)return FV_AUTH;if(!random||zero(random,32)||zero(random+32,16)||zero(random+48,32))return FV_ENTROPY;
 u8*z=al(v,FV2_UNIT);if(!z)return fail(v,FV_IO);
 /* Recheck blank headers at the destructive API boundary, even if a caller
  * skipped probe or another writer changed the medium since it was probed. */
 for(unsigned i=0;i<2;i++){if(rd(v,i,z)){fr(v,z,FV2_UNIT);return FV_IO;}if(!zero(z,FV2_UNIT)){fr(v,z,FV2_UNIT);return FV_STATE;}}
 memset(z,0,FV2_UNIT);int r=wr(v,2,z)||wr(v,3,z)?FV_IO:0;fr(v,z,FV2_UNIT);if(r)return r;
 memset(&v->h,0,sizeof v->h);memcpy(v->h.magic,"TMCFV003",8);v->h.version=3;v->h.state=FV_ON;v->h.units=v->io.units;v->h.blocks=vault2_capacity(v->io.units);v->h.ceiling=1;
 memcpy(v->key,random,32);memcpy(v->h.uuid,random+32,16);hex(random+48,recovery);u8 r2[40];crypto_blake2b_keyed(r2,40,random+48,32,random+88,40);
 r=wrap(v,0,pass,random+80)||wrap(v,1,recovery,r2)?FV_AUTH:0;erase(r2,40);
 if(r){vault2_lock(v);return r;}v->present=v->unlocked=1;v->next=1;r=save(v);if(r){erase(recovery,65);vault2_lock(v);}return r;
}
int vault2_unlock(Vault2*v,const char*pass){
 if(!v||!v->present||v->poison)return FV_STATE;u8 key[32];int r=unwrap(v,pass,key);if(r)return r;
 memcpy(v->key,key,32);erase(key,32);v->unlocked=1;v->next=v->h.ceiling;r=replay(v);if(r)vault2_lock(v);return r;
}
void vault2_lock(Vault2*v){if(v){erase(v->key,32);v->unlocked=0;v->next=0;}}
int vault2_read(Vault2*v,u64 block,void*out){
 if(!out)return FV_STATE;memset(out,0,FV2_BLOCK);
 if(!v||v->poison||block>=v->h.blocks)return FV_IO;if(!v->present||!v->unlocked)return FV_AUTH;
 u8*p=al(v,FV2_RECORD);if(!p)return fail(v,FV_IO);int r=FV_IO;u64 first=FV2_START+block*5;
 for(unsigned i=0;i<5;i++)if(rd(v,first+i,p+i*FV2_UNIT))goto done;
 Meta*m=(void*)(p+FV2_BLOCK);if(!meta_valid(v,m,block)){r=fail(v,FV_CORRUPT);goto done;}
 u8 n[24],ad[32];nonce(v,m->nonce,n);aad(v,"FV2DAT03",block,ad);
 r=crypto_aead_unlock(out,m->tag,v->key,n,ad,32,p,FV2_BLOCK)?fail(v,FV_CORRUPT):0;
 done:if(r)memset(out,0,FV2_BLOCK);fr(v,p,FV2_RECORD);return r;
}
int vault2_write(Vault2*v,u64 block,const void*data){
 if(!v||v->poison||block>=v->h.blocks||!data)return FV_IO;if(!v->present||!v->unlocked)return FV_AUTH;
 u64 counter;int r=reserve(v,&counter);if(r)return r;
 u8*p=al(v,FV2_RECORD),*d=al(v,FV2_UNIT);if(!p||!d){fr(v,p,FV2_RECORD);fr(v,d,FV2_UNIT);return fail(v,FV_IO);}
 memset(p,0,FV2_RECORD);memset(d,0,FV2_UNIT);Meta*m=(void*)(p+FV2_BLOCK);Journal*j=(void*)d;u8 n[24],ad[32];
 memcpy(m->magic,"FV2BLK03",8);m->block=block;m->nonce=counter;memcpy(m->uuid,v->h.uuid,16);nonce(v,counter,n);aad(v,"FV2DAT03",block,ad);crypto_aead_lock(p,m->tag,v->key,n,ad,32,data,FV2_BLOCK);
 memcpy(j->magic,"FV2TXN03",8);j->block=block;j->nonce=counter;jmac(v,j,p,j->mac);
 for(unsigned i=0;i<5;i++)if(wr(v,4+i,p+i*FV2_UNIT)){r=FV_IO;goto done;}
 if(wr(v,2,d)||wr(v,3,d)){r=FV_IO;goto done;}
 /* apply() may clear d only after reading its target, and never reuses j. */
 r=apply(v,j,p,d);
 done:fr(v,p,FV2_RECORD);fr(v,d,FV2_UNIT);return r;
}
int vault2_password(Vault2*v,const char*old,const char*pass,const u8 random[40]){
 if(!v||!v->present||!v->unlocked||v->poison)return FV_STATE;if(!pass||strlen(pass)<8||strlen(pass)>64)return FV_AUTH;if(!random||zero(random,40))return FV_ENTROPY;
 u8 k[32];int r=unwrap(v,old,k);erase(k,32);if(r)return r;r=wrap(v,0,pass,random);return r?r:save(v);
}
int vault2_recovery(Vault2*v,const char*pass,const u8 random[72],char out[65]){
 if(!v||!v->present||!v->unlocked||v->poison)return FV_STATE;if(!random||zero(random,72))return FV_ENTROPY;if(!out)return FV_STATE;
 u8 k[32];int r=unwrap(v,pass,k);erase(k,32);if(r)return r;hex(random,out);r=wrap(v,1,out,random+32);if(!r)r=save(v);if(r)erase(out,65);return r;
}
