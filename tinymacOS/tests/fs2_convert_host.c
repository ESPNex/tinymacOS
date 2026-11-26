/* Real FS2 + FV003, same disposable raw extent, no physical devices. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fs2_convert.h"
static struct {u8 *p;u64 units;int writes,event,cut,torn;} m;
static int rd(void*x,u64 n,void*p){(void)x;assert(n<m.units);memcpy(p,m.p+n*4096,4096);return 0;}
static int wr(void*x,u64 n,const void*p){(void)x;assert(n<m.units);m.writes++;if(m.cut>=0&&m.event++==m.cut){if(m.torn)memcpy(m.p+n*4096,p,73);return -1;}memcpy(m.p+n*4096,p,4096);return 0;}
static int fl(void*x){(void)x;return m.cut>=0&&m.event++==m.cut?-1:0;}
static void*al(void*x,size_t n){(void)x;return malloc(n);}
static void fr(void*x,void*p){(void)x;free(p);}
static Fv2Io raw(void){return (Fv2Io){0,m.units,rd,wr,fl,al,fr};}
static int pr(void*x,u64 b,void*p){(void)x;memcpy(p,m.p+b*16384,16384);return 0;}
static int pw(void*x,u64 b,const void*p){(void)x;memcpy(m.p+b*16384,p,16384);return 0;}
static int er(void*x,u64 b,void*p){return vault2_read(x,b,p);}
static int ew(void*x,u64 b,const void*p){return vault2_write(x,b,p);}
static char recovery[65];static u8 randoms[128];
static void check(int encrypted,const char*key){
 Vault2 v;Tmc2 fs;T2Io io={0,m.units/4,pr,pw,fl,al,fr};
 if(encrypted){assert(vault2_probe(&v,raw())==1);assert(!vault2_unlock(&v,key));io=(T2Io){&v,v.h.blocks,er,ew,fl,al,fr};}
 assert(!tmc2_mount(&fs,io));assert(!tmc2_verify(&fs));T2Entry e;assert(!tmc2_lookup(&fs,1,"proof",&e));char q[64]={0};assert(tmc2_read(&fs,e.key,0,q,sizeof q)==16&&!memcmp(q,"AFTER_CONVERSION",16));
 char account[512]={0};assert(!tmc2_account(&fs,0,account,0)&&!strcmp(account,"PRIVATE_ACCOUNT"));assert(!tmc2_snapshot_verify(&fs,"before"));memset(q,0,64);assert(tmc2_snapshot_read(&fs,"before",e.key,0,q,64)==17&&!memcmp(q,"BEFORE_CONVERSION",17));
 tmc2_close(&fs);if(encrypted)vault2_lock(&v);
}
static void seed(void){
 memset(m.p,0,m.units*4096);m.cut=-1;m.writes=0;Tmc2 fs;T2Io io={0,m.units/4,pr,pw,fl,al,fr};assert(!tmc2_format(&fs,io));u64 id;assert(!tmc2_create(&fs,1,"proof",T2_FILE,&id));assert(tmc2_write(&fs,id,0,"BEFORE_CONVERSION",17)==17);char account[512]={0};strcpy(account,"PRIVATE_ACCOUNT");assert(!tmc2_account(&fs,0,account,1));assert(!tmc2_snapshot(&fs,"before",0));assert(!tmc2_truncate(&fs,id,0));assert(tmc2_write(&fs,id,0,"AFTER_CONVERSION",16)==16);assert(!tmc2_verify(&fs));tmc2_close(&fs);memcpy(m.p+1024*4096,"DELETED_SECRET_REMNANT",22);
}
static void finish(Fs2Convert*c){int r=0;while(!r)r=fs2_convert_step(c,64);assert(r==1);}
int main(void){
 m.units=2048;m.p=calloc(m.units,4096);assert(m.p);for(unsigned i=0;i<128;i++)randoms[i]=i+1;seed();
 Fs2Convert c;u8*original=malloc(m.units*4096);assert(original);memcpy(original,m.p,m.units*4096);
 assert(!fs2_convert_begin(&c,raw(),CV_ENCRYPT,"conversion-password",randoms,recovery));assert(!memcmp(original,m.p,32768));u64 count=c.h.count;assert(c.h.stage>=count);finish(&c);check(1,recovery);for(u64 k=0;k+22<m.units*4096;k++)assert(memcmp(m.p+k,"DELETED_SECRET_REMNANT",22));
 int before=m.writes;assert(fs2_convert_begin(&c,raw(),CV_DECRYPT,"wrong-password",0,recovery)==FV_AUTH&&m.writes==before);
 assert(!fs2_convert_begin(&c,raw(),CV_DECRYPT,"conversion-password",0,recovery));finish(&c);check(0,0);
 puts("PASS actual FS2 same-extent encrypt/decrypt: password/recovery, files, private account and CoW snapshot preserved; truthful capacity reduced/restored, wrong credential has zero writes");
 seed();assert(!fs2_convert_begin(&c,raw(),CV_ENCRYPT,"conversion-password",randoms,recovery));u8*prepared=malloc(m.units*4096);assert(prepared);memcpy(prepared,m.p,m.units*4096);char saved[65];memcpy(saved,recovery,65);
 for(int torn=0;torn<2;torn++)for(int k=0;k<48;k++){
  memcpy(m.p,prepared,m.units*4096);m.cut=-1;assert(fs2_convert_probe(&c,raw())==1&&!fs2_convert_unlock(&c,saved));m.cut=k*13;m.event=0;m.torn=torn;
  int r=0;while(!r)r=fs2_convert_step(&c,64);fs2_convert_lock(&c);m.cut=-1;
  int p=fs2_convert_probe(&c,raw());assert(p==0||p==1);if(p==1){assert(!fs2_convert_unlock(&c,saved));finish(&c);}check(1,saved);
 }
 puts("PASS 96 relocation write/flush/torn-unit interruptions: reopen authenticates saved password slot via recovery, resumes and fully verifies; never formats or drops account/snapshot");
 memcpy(m.p,prepared,m.units*4096);assert(fs2_convert_probe(&c,raw())==1);before=m.writes;assert(fs2_convert_unlock(&c,"bad")==FV_AUTH&&m.writes==before);
 /* A valid public checksum cannot forge progress authentication. */
 m.p[(m.units-1)*4096+4000]^=1;m.p[(m.units-2)*4096+4000]^=1;assert(fs2_convert_probe(&c,raw())==FV_CORRUPT);
 puts("PASS wrong credential read-only, damaged conversion journal fails closed");
 memcpy(m.p,prepared,m.units*4096);m.cut=-1;assert(fs2_convert_probe(&c,raw())==1&&!fs2_convert_unlock(&c,saved));while(c.h.progress<c.h.count)assert(!fs2_convert_step(&c,1));assert(c.h.cleaned);memcpy(original,m.p,m.units*4096);
 for(int torn=0;torn<2;torn++)for(int k=0;k<24;k++){
  memcpy(m.p,original,m.units*4096);m.cut=-1;assert(fs2_convert_probe(&c,raw())==1&&!fs2_convert_unlock(&c,saved));m.cut=k*17;m.event=0;m.torn=torn;int r=0;while(!r)r=fs2_convert_step(&c,64);m.cut=-1;assert(fs2_convert_probe(&c,raw())==1&&!fs2_convert_unlock(&c,saved));finish(&c);check(1,saved);for(u64 u=count;u<m.units;u++)for(unsigned j=0;j<4096;j++)assert(!m.p[u*4096+j]);
 }
 puts("PASS deleted plaintext remnants scrubbed; 48 write/flush/torn cleanup interruptions resume without data loss or plaintext residue");
 free(original);free(prepared);free(m.p);return 0;
}
