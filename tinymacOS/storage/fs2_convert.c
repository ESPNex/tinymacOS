/* SPDX-License-Identifier: MIT */
#include "fs2_convert.h"
#include "sha256.h"
#include "../crypto/monocypher/monocypher.h"
#include <stddef.h>
static u32 crc(const void*p,size_t n){const u8*s=p;u32 c=~0u;while(n--){c^=*s++;for(int j=0;j<8;j++)c=(c>>1)^(0xedb88320u&-(c&1));}return ~c;}
static int fail(Fs2Convert*c,int r){c->poison=1;return r;}
static void*al(Fs2Convert*c,size_t n){return c->io.alloc(c->io.context,n);}
static void fr(Fs2Convert*c,void*p,size_t n){if(p){erase(p,n);c->io.free(c->io.context,p);}}
static int readu(Fs2Convert*c,u64 n,void*p){return n>=c->io.units||c->io.read(c->io.context,n,p)?FV_IO:0;}
static int writeu(Fs2Convert*c,u64 n,const void*p){
 u8*q=al(c,FV2_UNIT);if(!q)return fail(c,FV_IO);
 int r=n>=c->io.units||c->io.write(c->io.context,n,p)||c->io.flush(c->io.context)||readu(c,n,q)||memcmp(p,q,FV2_UNIT)?FV_IO:0;
 fr(c,q,FV2_UNIT);return r?fail(c,r):0;
}
static void mac(const CvHeader*h,const u8 key[32],u8 out[32]){crypto_blake2b_keyed(out,32,key,32,(const u8*)h,offsetof(CvHeader,mac));}
static int valid(const CvHeader*h,u64 units){
 if(memcmp(h->magic,"TMCVX004",8)||h->version!=1||(h->mode!=CV_ENCRYPT&&h->mode!=CV_DECRYPT)||h->units!=units||!h->serial||h->count<(h->mode==CV_ENCRYPT?9u:8u)||!h->stage||h->stage>=units-2||h->count>units-2-h->stage||h->progress>h->count||h->cleaned>units-2||(h->cleaned&&(h->progress!=h->count||h->cleaned<h->count)))return 0;
 for(unsigned i=0;i<sizeof h->pad;i++)if(h->pad[i])return 0;
 u8 sum[32];sha_bytes(h,offsetof(CvHeader,sum),sum);int okay=!crypto_verify32(sum,h->sum);erase(sum,32);return okay;
}
static int save(Fs2Convert*c){
 if(c->h.serial==~0ull)return fail(c,FV_STATE);c->h.serial++;
 mac(&c->h,c->key,c->h.mac);sha_bytes(&c->h,offsetof(CvHeader,sum),c->h.sum);
 return writeu(c,c->io.units-1,&c->h)||writeu(c,c->io.units-2,&c->h)?FV_IO:0;
}
void fs2_convert_lock(Fs2Convert*c){if(c){erase(c->key,32);c->unlocked=0;}}
int fs2_convert_probe(Fs2Convert*c,Fv2Io io){
 if(!c)return FV_STATE;erase(c,sizeof *c);c->io=io;
 if(!io.read||!io.write||!io.flush||!io.alloc||!io.free||io.units<64)return fail(c,FV_SPACE);
 CvHeader*a=al(c,sizeof *a),*b=al(c,sizeof *b);int r=FV_IO;
 if(!a||!b)goto done;
 if(readu(c,io.units-2,a)||readu(c,io.units-1,b))goto done;
 int av=valid(a,io.units),bv=valid(b,io.units);
 if(!av&&!bv){
  int marked=!memcmp(a->magic,"TMCVX004",8)||!memcmp(b->magic,"TMCVX004",8);
  if(readu(c,0,a))goto done;
  marked|=!memcmp(a->magic,"TMCVX004",8);r=marked?FV_CORRUPT:0;goto done;
 }
 if(av&&bv&&a->serial==b->serial&&memcmp(a,b,sizeof *a)){r=FV_CORRUPT;goto done;}
 c->h=av&&(!bv||a->serial>=b->serial)?*a:*b;c->present=1;r=1;
 done:fr(c,a,sizeof *a);fr(c,b,sizeof *b);if(r<0){c->present=1;c->poison=1;}return r;
}
/* Credential-only read-only envelope. Unlock authenticates its saved header;
 * journal replay sees two blank descriptors and cannot write to the medium. */
static int credread(void*x,u64 n,void*p){CvHeader*h=x;memset(p,0,FV2_UNIT);if(n<2)memcpy(p,&h->credential,512);else if(n>3)return -1;return 0;}
static int nowrite(void*x,u64 n,const void*p){(void)x;(void)n;(void)p;return -1;}
static int noflush(void*x){(void)x;return -1;}
typedef struct {Fs2Convert*c;u64 offset,limit;Vault2*v;int cipher;} View;
static void*va(void*x,size_t n){View*v=x;return al(v->c,n);}
static void vf(void*x,void*p){View*v=x;v->c->io.free(v->c->io.context,p);}
static int vr(void*x,u64 n,void*p){View*v=x;return n>=v->limit?-1:readu(v->c,v->offset+n,p);}
static int vw(void*x,u64 n,const void*p){View*v=x;return n>=v->limit?-1:v->c->io.write(v->c->io.context,v->offset+n,p);}
static int vflush(void*x){View*v=x;return v->c->io.flush(v->c->io.context);}
static int tr(void*x,u64 b,void*p){View*v=x;if(v->cipher)return vault2_read(v->v,b,p);for(unsigned i=0;i<4;i++)if(vr(x,b*4+i,(u8*)p+i*FV2_UNIT))return -1;return 0;}
static int tw(void*x,u64 b,const void*p){View*v=x;if(v->cipher)return vault2_write(v->v,b,p);for(unsigned i=0;i<4;i++)if(vw(x,b*4+i,(const u8*)p+i*FV2_UNIT))return -1;return vflush(x);}
static int credential(Fs2Convert*,const char*,u8[32]);
int fs2_convert_unlock(Fs2Convert*c,const char*pass){
 if(!c||!c->present||c->poison)return FV_STATE;
 u8 key[32];int r=credential(c,pass,key);
 if(!r){u8 digest[32];mac(&c->h,key,digest);if(crypto_verify32(digest,c->h.mac))r=FV_CORRUPT;erase(digest,32);}
 if(!r){memcpy(c->key,key,32);c->unlocked=1;}erase(key,32);return r;
}
typedef struct {Fs2Convert*c;CvHeader*h;} Credential;
static int cr(void*x,u64 n,void*p){return credread(((Credential*)x)->h,n,p);}
static void*ca(void*x,size_t n){return al(((Credential*)x)->c,n);}
static void cf(void*x,void*p){Fs2Convert*c=((Credential*)x)->c;c->io.free(c->io.context,p);}
static int credential(Fs2Convert*c,const char*pass,u8 key[32]){
 Credential ct={c,&c->h};Fv2Io io={&ct,c->io.units,cr,nowrite,noflush,ca,cf};Vault2 v;int r=vault2_probe(&v,io)==1?vault2_unlock(&v,pass):FV_CORRUPT;
 if(!r)memcpy(key,v.key,32);vault2_lock(&v);erase(&v,sizeof v);return r;
}
static int verify_target(Fs2Convert*c,View*view,u64 blocks){(void)c;Tmc2 fs;T2Io io={view,blocks,tr,tw,vflush,va,vf};if(tmc2_mount(&fs,io))return FV_CORRUPT;int r=tmc2_verify(&fs);tmc2_close(&fs);return r?FV_CORRUPT:0;}
int fs2_convert_begin(Fs2Convert*c,Fv2Io io,unsigned mode,const char*pass,const u8 random[128],char recovery[65]){
 int r=fs2_convert_probe(c,io);if(r)return r<0?r:FV_STATE;
 if((mode!=CV_ENCRYPT&&mode!=CV_DECRYPT)||!pass||!recovery)return FV_STATE;
 if(mode==CV_ENCRYPT&&(strlen(pass)<8||strlen(pass)>64||!random))return FV_AUTH;
 View source={.c=c,.limit=io.units,.cipher=mode==CV_DECRYPT};Vault2 old={0},new={0};source.v=&old;
 if(source.cipher){
 u8*j=al(c,FV2_UNIT);if(!j)return FV_IO;
 for(unsigned u=2;u<4;u++){if(readu(c,u,j)){r=FV_IO;break;}for(unsigned k=0;k<FV2_UNIT;k++)if(j[k]){r=FV_STATE;break;}if(r)break;}
 fr(c,j,FV2_UNIT);if(r)return r;
 r=vault2_probe(&old,io)==1?vault2_unlock(&old,pass):FV_STATE;if(r)return r;}
 else{u8*h=al(c,FV2_UNIT);if(!h)return FV_IO;r=readu(c,0,h)||memcmp(h,"TMCBFS2",8)?FV_STATE:0;fr(c,h,FV2_UNIT);if(r)return r;}
 Tmc2 fs;T2Io si={&source,source.cipher?old.h.blocks:io.units/4,tr,tw,vflush,va,vf};
 if(tmc2_mount(&fs,si)){vault2_lock(&old);return FV_CORRUPT;}
 r=tmc2_verify(&fs);u64 cursor=0,b=0,high=0;int it;
 while(!r&&(it=tmc2_allocated_next(&fs,&cursor,&b))==1)high=b;
 if(!r&&it<0)r=FV_CORRUPT;
 u64 blocks=mode==CV_ENCRYPT?vault2_capacity(io.units):io.units/4;
 u64 count=mode==CV_ENCRYPT?FV2_START+5*(high+1):4*(high+1);
 u64 source_end=mode==CV_DECRYPT?FV2_START+5*(high+1):4*(high+1);
 if(!blocks||high>=blocks||count>io.units-2||source_end>io.units-2-count||count>io.units-2-count)r=FV_SPACE;
 if(r){tmc2_close(&fs);vault2_lock(&old);return r;}
 u64 stage=io.units-2-count;View target={.c=c,.offset=stage,.limit=count,.cipher=mode==CV_ENCRYPT,.v=&new};
 u8*p=al(c,T2_BLOCK);if(!p){r=FV_IO;goto done;}
 /* Only the verified unreferenced tail is written during preparation. */
 memset(p,0,T2_BLOCK);for(u64 i=0;i<count;i++)if(vw(&target,i,p)){r=FV_IO;goto done;}if(vflush(&target)){r=FV_IO;goto done;}
 if(target.cipher){Fv2Io vi={&target,io.units,vr,vw,vflush,va,vf};r=vault2_probe(&new,vi)==0?vault2_create(&new,pass,random,recovery):FV_STATE;if(r)goto done;}
 cursor=0;
 while((it=tmc2_allocated_next(&fs,&cursor,&b))==1){
  if(tr(&source,b,p)){r=FV_IO;goto done;}
  if(b<2&&fs.valid[b]){T2Super*s=(void*)p;s->blocks=blocks;s->crc=0;s->crc=crc(p,T2_BLOCK);}
  if(tw(&target,b,p)){r=FV_IO;goto done;}
 }
 if(it<0||(r=verify_target(c,&target,blocks)))goto done;
 memset(&c->h,0,sizeof c->h);memcpy(c->h.magic,"TMCVX004",8);c->h.version=1;c->h.mode=mode;c->h.units=io.units;c->h.stage=stage;c->h.count=count;
 c->h.credential=target.cipher?new.h:old.h;memcpy(c->key,target.cipher?new.key:old.key,32);c->present=c->unlocked=1;r=save(c);
 done:fr(c,p,T2_BLOCK);tmc2_close(&fs);vault2_lock(&old);vault2_lock(&new);erase(&old,sizeof old);erase(&new,sizeof new);
 if(r){erase(recovery,65);fs2_convert_lock(c);}return r;
}
int fs2_convert_step(Fs2Convert*c,unsigned budget){
 if(!c||!c->present||!c->unlocked||c->poison||!budget||budget>256)return FV_STATE;
 u8*p=al(c,FV2_UNIT);if(!p)return FV_IO;int r=0;
 if(!c->h.progress){memset(p,0,FV2_UNIT);memcpy(p,"TMCVX004",8);if(writeu(c,0,p)){r=FV_IO;goto done;}}
 u64 header=c->h.mode==CV_ENCRYPT?9:8,body=c->h.count-header;
 while(budget&&c->h.progress<c->h.count){
  budget--;
  u64 at=c->h.progress<body?c->h.progress+header:c->h.progress-body;
  if(readu(c,c->h.stage+at,p)||writeu(c,at,p)){r=fail(c,FV_IO);goto done;}
  c->h.progress++;if(save(c)){r=FV_IO;goto done;}
 }
 if(c->h.progress==c->h.count&&!c->h.cleaned){
  Vault2 v={0};View target={.c=c,.limit=c->io.units,.cipher=c->h.mode==CV_ENCRYPT,.v=&v};
  if(target.cipher){if(vault2_probe(&v,c->io)!=1){r=fail(c,FV_CORRUPT);goto done;}memcpy(v.key,c->key,32);v.unlocked=1;}
  r=verify_target(c,&target,target.cipher?v.h.blocks:c->io.units/4);vault2_lock(&v);erase(&v,sizeof v);
  if(r){fail(c,r);goto done;}
  /* Prefix has been independently verified. Durably record that fact before
   * scrubbing the complete unused tail, including old free-space remnants and
   * the staged copy. Scrubbing is resumable; no source is needed from here. */
  c->h.cleaned=c->h.count;if(save(c)){r=FV_IO;goto done;}
 }
 if(c->h.cleaned){
  memset(p,0,FV2_UNIT);
  while(budget&&c->h.cleaned<c->io.units-2){budget--;if(writeu(c,c->h.cleaned,p)){r=FV_IO;goto done;}c->h.cleaned++;if(save(c)){r=FV_IO;goto done;}}
  if(c->h.cleaned<c->io.units-2)goto done;
  if(writeu(c,c->io.units-2,p)||writeu(c,c->io.units-1,p)){r=FV_IO;goto done;}
  fs2_convert_lock(c);c->present=0;r=1;
 }
 done:fr(c,p,FV2_UNIT);return r;
}
