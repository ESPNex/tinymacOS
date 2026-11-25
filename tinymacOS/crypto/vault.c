#include "vault.h"
#include "sha256.h"
#include "monocypher/monocypher.h"
/* Monocypher 4.0.2 XChaCha20-Poly1305; PBKDF2-HMAC-SHA256 password slot.
 * Random volume key; independent random 256-bit recovery credential.
 * No deterministic nonce is ever reused, including failed/crashed writes:
 * nonce ranges are committed to BOTH headers before issuing ciphertext.
 * Every write journals ciphertext AND its complete tag-table sector before
 * touching either destination. A failed write poisons the context until reopen.
 * Assumption: successful flush persists all preceding writes. Torn sectors are
 * detected, not silently accepted. Does not prevent full-medium rollback.
 */
#define HP (FV_META+0)
#define HB (FV_META+1)
#define JP (FV_META+2)
#define JB (FV_META+3)
#define JD (FV_META+4)
#define TABLE (FV_META+6)
typedef struct PACK {u8 magic[8];u64 nonce;u32 sector,plain;u8 reserved[456],mac[32];} Journal;
_Static_assert(sizeof(Journal)==512,"journal ABI");
static int zeros(const void *v,size_t n){const u8*p=v;u8 z=0;while(n--)z|=*p++;return !z;}
static int fail(Vault*v){v->poison=1;return FV_IO;}
static int rd(Vault*v,u32 l,void*p){return l<v->io.sectors?v->io.read(v->io.opaque,l,p):FV_IO;}
static int wr(Vault*v,u32 l,const void*p){return l<v->io.sectors&&!v->io.write(v->io.opaque,l,p)&&!v->io.flush(v->io.opaque)?0:fail(v);}
static int checksum(const FvHeader*h){u8 s[32];sha_bytes(h,480,s);int ok=!crypto_verify32(s,h->sum);erase(s,32);return ok;}
static int valid(const FvHeader*h){return !memcmp(h->magic,"TMCFV002",8)&&h->version==2&&h->state<=FV_DECRYPTING&&h->sectors==FV_PAYLOAD&&h->progress<=FV_PAYLOAD&&h->serial&&h->ceiling&&checksum(h);}
static void hmac(const FvHeader*h,const u8*k,u8 out[32]){crypto_blake2b_keyed(out,32,k,32,(const u8*)h,232);}
static int authenticated(const FvHeader*h,const u8*k){u8 m[32];hmac(h,k,m);int ok=!crypto_verify32(m,h->mac);erase(m,32);return ok;}
static int save(Vault*v){if(v->poison)return FV_IO;v->h.serial++;hmac(&v->h,v->key,v->h.mac);sha_bytes(&v->h,480,v->h.sum);return wr(v,HB,&v->h)||wr(v,HP,&v->h)?FV_IO:0;}
int vault_probe(Vault*v,FvIo io)
{
 erase(v,sizeof *v);v->io=io;if(io.sectors<FV_REQUIRED)return 0;
 FvHeader a,b;int x=rd(v,HP,&a),y=rd(v,HB,&b);if(x||y){v->poison=1;return FV_IO;}
 int av=valid(&a),bv=valid(&b);
 if(!av&&!bv){if(zeros(&a,512)&&zeros(&b,512))return 0;v->present=1;v->poison=1;return FV_CORRUPT;}
 if(av&&bv&&a.serial==b.serial&&memcmp(&a,&b,512)){v->present=1;v->poison=1;return FV_CORRUPT;}
 v->h=av&&(!bv||a.serial>=b.serial)?a:b;
 v->present=v->h.state!=FV_OFF;return v->present;
}
static void aad(const Vault*v,int slot,u8 a[24]){memcpy(a,v->h.uuid,16);u32 t[2]={2,(u32)slot};memcpy(a+16,t,8);}
static int derive(const char*pass,const FvSlot*s,int recovery,u8 k[32])
{
 size_t n=strlen(pass);if(!n||n>64)return FV_AUTH;
 if(recovery){if(n!=64)return FV_AUTH;u8 raw[32];for(int i=0;i<32;i++){int a=pass[i*2],b=pass[i*2+1];a=a>='0'&&a<='9'?a-'0':a>='a'&&a<='f'?a-'a'+10:a>='A'&&a<='F'?a-'A'+10:-1;b=b>='0'&&b<='9'?b-'0':b>='a'&&b<='f'?b-'a'+10:b>='A'&&b<='F'?b-'A'+10:-1;if(a<0||b<0){erase(raw,32);return FV_AUTH;}raw[i]=(a<<4)|b;}
  crypto_blake2b_keyed(k,32,raw,32,s->salt,16);erase(raw,32);
 }else{PBKDF p;pbk_begin(&p,(const u8*)pass,n,s->salt);pbk_run(&p,FV_ROUNDS-1);memcpy(k,p.sum,32);erase(&p,sizeof p);}return 0;
}
static int unwrap(Vault*v,const char*pass,u8 key[32])
{
 for(int i=1;i>=0;i--){u8 k[32],a[24];aad(v,i,a);if(derive(pass,&v->h.slot[i],i,k))continue;
  int r=crypto_aead_unlock(key,v->h.slot[i].tag,k,v->h.slot[i].nonce,a,24,v->h.slot[i].wrapped,32);erase(k,32);
  if(!r&&authenticated(&v->h,key))return 0;erase(key,32);
 }return FV_AUTH;
}
static int wrap(Vault*v,int slot,const char*pass,const u8 random[40])
{
 FvSlot*s=v->h.slot+slot;u8 k[32],a[24];memcpy(s->salt,random,16);memcpy(s->nonce,random+16,24);aad(v,slot,a);
 if(derive(pass,s,slot,k))return FV_AUTH;crypto_aead_lock(s->wrapped,s->tag,k,s->nonce,a,24,v->key,32);erase(k,32);return 0;
}
static void hex(const u8*in,char*out){for(int i=0;i<32;i++){out[2*i]="0123456789ABCDEF"[in[i]>>4];out[2*i+1]="0123456789ABCDEF"[in[i]&15];}out[64]=0;}
static void nonce(const Vault*v,u64 n,u8 out[24]){memcpy(out,v->h.uuid,16);memcpy(out+16,&n,8);}
static void jmac(Vault*v,const Journal*j,const u8 payload[1024],u8 out[32]){crypto_blake2b_ctx c;crypto_blake2b_keyed_init(&c,32,v->key,32);crypto_blake2b_update(&c,(const u8*)j,480);crypto_blake2b_update(&c,payload,1024);crypto_blake2b_final(&c,out);erase(&c,sizeof c);}
static int jvalid(Vault*v,const Journal*j,const u8 p[1024]){u8 h[32];if(memcmp(j->magic,"FVTXN002",8)||j->sector>=FV_PAYLOAD||j->plain>1||!j->nonce||j->nonce>=v->h.ceiling)return 0;jmac(v,j,p,h);int ok=!crypto_verify32(j->mac,h);erase(h,32);return ok;}
static int apply(Vault*v,const Journal*j,const u8 p[1024]){u8 z[512]={0};if(wr(v,j->sector,p)||wr(v,TABLE+j->sector/16,p+512)||wr(v,JP,z)||wr(v,JB,z))return FV_IO;return 0;}
static int replay(Vault*v)
{
 Journal a,b;u8 p[1024];if(rd(v,JP,&a)||rd(v,JB,&b)||rd(v,JD,p)||rd(v,JD+1,p+512))return fail(v);
 int av=jvalid(v,&a,p),bv=jvalid(v,&b,p);
 if(!av&&!bv){if(zeros(&a,512)&&zeros(&b,512))return 0;if(zeros(&a,512)||zeros(&b,512)){u8 z[512]={0};return wr(v,JP,z)||wr(v,JB,z)?FV_IO:0;}v->poison=1;return FV_CORRUPT;}
 if(av&&bv&&memcmp(&a,&b,512)){v->poison=1;return FV_CORRUPT;}
 return apply(v,av?&a:&b,p);
}
static int reserve(Vault*v,u64*out){if(v->next>=v->h.ceiling){if(v->h.ceiling>~0ull-1024)return FV_STATE;v->next=v->h.ceiling;v->h.ceiling+=1024;if(save(v))return FV_IO;}*out=v->next++;return 0;}
static int tagread(Vault*v,u32 l,FvTag*t,u8 table[512]){if(rd(v,TABLE+l/16,table))return fail(v);memcpy(t,table+(l%16)*32,32);if(!zeros(t->pad,8)||t->nonce>=v->h.ceiling){v->poison=1;return FV_CORRUPT;}return 0;}
int vault_read(Vault*v,u32 l,void*out)
{
 if(l>=FV_PAYLOAD||v->poison)return FV_IO;if(!v->present)return rd(v,l,out);if(!v->unlocked)return FV_AUTH;
 FvTag t;u8 table[512],cipher[512],n[24],ad[24];int r=tagread(v,l,&t,table);if(r)return r;if(rd(v,l,cipher))return fail(v);
 if(!t.nonce){if(v->h.state==FV_ON){v->poison=1;return FV_CORRUPT;}memcpy(out,cipher,512);return 0;}
 nonce(v,t.nonce,n);memcpy(ad,v->h.uuid,16);u64 sector=l;memcpy(ad+16,&sector,8);
 if(crypto_aead_unlock(out,t.tag,v->key,n,ad,24,cipher,512)){memset(out,0,512);v->poison=1;return FV_CORRUPT;}return 0;
}
static int transaction(Vault*v,u32 l,const void*data,int plain)
{
 u8 p[1024],n[24],ad[24];FvTag t;Journal j={0};int r=tagread(v,l,&t,p+512);if(r)return r;
 u64 counter;if(reserve(v,&counter))return FV_IO;memset(&t,0,sizeof t);
 if(plain)memcpy(p,data,512);else{t.nonce=counter;nonce(v,counter,n);memcpy(ad,v->h.uuid,16);u64 sector=l;memcpy(ad+16,&sector,8);crypto_aead_lock(p,t.tag,v->key,n,ad,24,data,512);}
 memcpy(p+512+(l%16)*32,&t,32);memcpy(j.magic,"FVTXN002",8);j.nonce=counter;j.sector=l;j.plain=plain;jmac(v,&j,p,j.mac);
 if(wr(v,JD,p)||wr(v,JD+1,p+512)||wr(v,JP,&j)||wr(v,JB,&j))return FV_IO;return apply(v,&j,p);
}
int vault_write(Vault*v,u32 l,const void*data){if(l>=FV_PAYLOAD||v->poison)return FV_IO;if(!v->present)return wr(v,l,data);if(!v->unlocked)return FV_AUTH;return transaction(v,l,data,v->h.state==FV_DECRYPTING&&l<v->h.progress);}
int vault_enable(Vault*v,const char*pass,const u8 random[128],char recovery[65])
{
 if(v->poison||v->present)return FV_STATE;if(v->io.sectors<FV_REQUIRED)return FV_SPACE;
 size_t len=strlen(pass);if(len<8||len>64)return FV_AUTH;if(zeros(random,128))return FV_ENTROPY;
 /* All payload sectors already belong to the known filesystem. Reserve only
  * the tail outside its declared live/snapshot/backup address space. */
 u8 z[512]={0};for(u32 i=FV_META+2;i<v->io.sectors;i++)if(wr(v,i,z))return FV_IO;
 memset(&v->h,0,sizeof v->h);memcpy(v->h.magic,"TMCFV002",8);v->h.version=2;v->h.sectors=FV_PAYLOAD;v->h.state=FV_ENCRYPTING;v->h.ceiling=1;
 memcpy(v->key,random,32);memcpy(v->h.uuid,random+32,16);hex(random+48,recovery);
 u8 r1[40],r2[40];memcpy(r1,random+80,40);/* Derive independent recovery salt/nonce from the secret random input. */
 crypto_blake2b_keyed(r2,40,random+48,32,random+88,40);
 if(wrap(v,0,pass,r1)||wrap(v,1,recovery,r2))return FV_AUTH;
 erase(r1,40);erase(r2,40);v->unlocked=1;v->present=1;v->next=1;return save(v);
}
int vault_unlock(Vault*v,const char*pass)
{
 if(!v->present||v->poison)return FV_STATE;u8 k[32];int r=unwrap(v,pass,k);if(r)return r;memcpy(v->key,k,32);erase(k,32);v->unlocked=1;
 /* All previously reserved numbers are burned after reboot. */
 v->next=v->h.ceiling;r=replay(v);if(r)vault_lock(v);return r;
}
void vault_lock(Vault*v){erase(v->key,32);v->unlocked=0;v->next=0;}
int vault_step(Vault*v,u32 budget)
{
 if(!v->present||v->poison||!v->unlocked)return FV_STATE;
 if(v->h.state==FV_ON)return 1;
 if(v->h.state!=FV_ENCRYPTING&&v->h.state!=FV_DECRYPTING)return FV_STATE;
 int plain=v->h.state==FV_DECRYPTING;u8 p[512],table[512];FvTag t;
 while(budget--&&v->h.progress<FV_PAYLOAD){u32 l=v->h.progress;int r=tagread(v,l,&t,table);if(r)return r;
  if((plain&&t.nonce)||(!plain&&!t.nonce)){r=vault_read(v,l,p);if(r)return r;r=transaction(v,l,p,plain);erase(p,512);if(r)return r;}v->h.progress++;
 }
 if(v->h.progress==FV_PAYLOAD){v->h.state=plain?FV_OFF:FV_ON;if(save(v))return FV_IO;if(plain){v->present=0;vault_lock(v);/* Remove keyslots, old tags and plaintext decryption journal. */
   memset(v->h.slot,0,sizeof v->h.slot);memset(v->h.mac,0,32);v->h.serial++;sha_bytes(&v->h,480,v->h.sum);if(wr(v,HB,&v->h)||wr(v,HP,&v->h))return FV_IO;
   u8 z[512]={0};for(u32 i=FV_META+2;i<v->io.sectors;i++)if(wr(v,i,z))return FV_IO;
  }return 1;}
 return save(v)?FV_IO:0;
}
int vault_decrypt(Vault*v,const char*pass){if(v->h.state!=FV_ON||!v->unlocked||v->poison)return FV_STATE;u8 k[32];int r=unwrap(v,pass,k);erase(k,32);if(r)return r;v->h.state=FV_DECRYPTING;v->h.progress=0;return save(v);}
int vault_password(Vault*v,const char*old,const char*pass,const u8 random[40]){if(!v->unlocked||v->poison||v->h.state!=FV_ON)return FV_STATE;if(strlen(pass)<8||strlen(pass)>64)return FV_AUTH;u8 k[32];int r=unwrap(v,old,k);erase(k,32);if(r)return r;if(wrap(v,0,pass,random))return FV_AUTH;return save(v);}
int vault_recovery(Vault*v,const char*pass,const u8 random[72],char out[65]){if(!v->unlocked||v->poison||v->h.state!=FV_ON)return FV_STATE;u8 k[32];int r=unwrap(v,pass,k);erase(k,32);if(r)return r;hex(random,out);if(wrap(v,1,out,random+32))return FV_AUTH;return save(v);}
