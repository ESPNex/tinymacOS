/* SPDX-License-Identifier: MIT
 * TMCFS2 -> bounded installer transport -> explicitly fresh TMCFS2 target.
 * Parent-before-child records, copied through aligned locals. No disk parser.
 */
#include "tmcbfs2_seed.h"
typedef struct {u8 magic[8];u64 bytes,count;u32 checksum,reserved;u8 accounts[1024];} Seed;
_Static_assert(sizeof(Seed)==1056,"installer transport ABI");
static u32 checksum(const u8*p,size_t n){u32 c=~0u;for(size_t i=0;i<n;i++){u8 b=i>=24&&i<28?0:p[i];c^=b;for(int k=0;k<8;k++)c=(c>>1)^(0xedb88320u&-(c&1));}return ~c;}
static int name_ok(const char*s){if(!s[0]||s[47]||eq(s,".")||eq(s,".."))return 0;for(unsigned i=0;s[i];i++)if(s[i]=='/'||(u8)s[i]<32)return 0;return 1;}
static size_t next(const T2Entry*e,size_t at){return at+sizeof *e+(e->kind==T2_FILE?e->size:0);}
int tmc2_seed_validate(const void*image,size_t bytes){
 if(!image||bytes<sizeof(Seed))return -1;const u8*p=image;Seed h;memcpy(&h,p,sizeof h);
 if(memcmp(h.magic,"TMCSEED2",8)||h.reserved||h.bytes<sizeof h||h.bytes>bytes||h.count>T2_SEED_MAX_ENTRIES||h.count>(h.bytes-sizeof h)/sizeof(T2Entry)||h.checksum!=checksum(p,h.bytes))return -1;
 size_t at=sizeof h;for(u64 i=0;i<h.count;i++){
  T2Entry e;if(at>h.bytes||sizeof e>h.bytes-at)return -1;memcpy(&e,p+at,sizeof e);
  if(e.key<2||e.parent<1||e.root||e.reserved>1||!name_ok(e.name)||(e.kind!=T2_FILE&&e.kind!=T2_DIR)||(e.kind==T2_DIR&&e.size)||e.size>h.bytes-at-sizeof e)return -1;
  int parent=e.parent==1;size_t before=sizeof h;
  for(u64 j=0;j<i;j++){T2Entry prior;memcpy(&prior,p+before,sizeof prior);if(e.key==prior.key||(e.parent==prior.parent&&eq(e.name,prior.name)))return -1;if(e.parent==prior.key){if(prior.kind!=T2_DIR)return -1;parent=1;}before=next(&prior,before);}
  if(!parent)return -1;at=next(&e,at);
 }return at==h.bytes?0:-1;
}
static int export_dir(Tmc2*c,u64 parent,u8*p,size_t cap,size_t*at,u64*count,unsigned depth){
 if(depth>48)return -1;u64 cookie=0;T2Entry e;int r;
 while((r=tmc2_readdir(c,parent,&cookie,&e))==1){
  if(*count>=T2_SEED_MAX_ENTRIES||*at>cap||sizeof e>cap-*at||e.size>cap-*at-sizeof e)return -1;
  u64 root=e.root;e.root=0;memcpy(p+*at,&e,sizeof e);e.root=root;*at+=sizeof e;(*count)++;
  if(e.kind==T2_FILE){if(tmc2_read(c,e.key,0,p+*at,e.size)!=(i64)e.size)return -1;*at+=e.size;}
  else if(export_dir(c,e.key,p,cap,at,count,depth+1))return -1;
 }return r<0?-1:0;
}
int tmc2_seed_export(Tmc2*c,void*image,size_t bytes){
 if(!c||!image||bytes<sizeof(Seed)||tmc2_verify(c))return -1;u8*p=image;memset(p,0,bytes);Seed h={0};memcpy(h.magic,"TMCSEED2",8);size_t at=sizeof h;
 for(unsigned i=0;i<2;i++)if(tmc2_account(c,i,h.accounts+i*512,0))return -1;
 if(export_dir(c,1,p,bytes,&at,&h.count,0))return -1;h.bytes=at;memcpy(p,&h,sizeof h);h.checksum=checksum(p,at);memcpy(p,&h,sizeof h);return tmc2_seed_validate(p,bytes);
}
typedef struct {u64 source,target;} Id;
static int transfer(Tmc2*c,const void*image,size_t bytes,int verify){
 if(tmc2_seed_validate(image,bytes))return -1;const u8*p=image;Seed h;memcpy(&h,p,sizeof h);
 if(tmc2_inode_count(c)!=(verify?(i64)h.count+1:1))return -1;
 Id*ids=c->io.alloc(c->io.context,MAX((size_t)1,h.count*sizeof(Id)));if(!ids)return -1;size_t at=sizeof h;int r=-1;
 for(u64 i=0;i<h.count;i++){
  T2Entry e,d;memcpy(&e,p+at,sizeof e);u64 parent=1;
  if(e.parent!=1){u64 j;for(j=0;j<i&&ids[j].source!=e.parent;j++);if(j==i)goto done;parent=ids[j].target;}
  ids[i].source=e.key;
  if(verify){if(tmc2_lookup(c,parent,e.name,&d)||d.kind!=e.kind||d.size!=e.size||d.reserved!=e.reserved)goto done;ids[i].target=d.key;
   u8 buffer[512];for(u64 off=0;off<e.size;off+=sizeof buffer){unsigned n=MIN(sizeof buffer,e.size-off);if(tmc2_read(c,d.key,off,buffer,n)!=n||memcmp(buffer,p+at+sizeof e+off,n))goto done;}}
  else{if(tmc2_create(c,parent,e.name,e.kind,&ids[i].target))goto done;if(e.kind==T2_FILE&&e.size&&tmc2_write(c,ids[i].target,0,p+at+sizeof e,e.size)!=(i64)e.size)goto done;}
  at=next(&e,at);
 }
 /* Apply readonly bits only after every descendant is populated. */
 at=sizeof h;for(u64 i=0;i<h.count;i++){T2Entry e;memcpy(&e,p+at,sizeof e);if(!verify&&e.reserved&&tmc2_readonly(c,ids[i].target,1))goto done;at=next(&e,at);}
 for(unsigned i=0;i<2;i++){u8 account[512];if(verify){if(tmc2_account(c,i,account,0)||memcmp(account,h.accounts+i*512,512))goto done;}else if(tmc2_account(c,i,h.accounts+i*512,1))goto done;}
 r=0;
 done:c->io.free(c->io.context,ids);return r;
}
int tmc2_seed_import(Tmc2*c,const void*p,size_t n){return transfer(c,p,n,0);}
int tmc2_seed_verify(Tmc2*c,const void*p,size_t n){return transfer(c,p,n,1);}
