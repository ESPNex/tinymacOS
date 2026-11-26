#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../userspace/ui.h"
static unsigned char files[2][262144];static int sizes[2],positions[2],exists[2],closed,partial,fail_write;
static int idx(const char*p){return eq(p,"/source")?0:eq(p,"/destination")?1:-1;}
static long hostcall(long n,u64 a,u64 b,u64 c,u64 d){
 (void)d;if(n==S_STAT){int i=idx((char*)a);if(i<0||!exists[i])return -1;*(Dirent*)b=(Dirent){.size=sizes[i]};return 0;}
 if(n==S_CREATE){int i=idx((char*)a);if(i<0||exists[i])return -1;exists[i]=1;sizes[i]=0;return 0;}
 if(n==S_OPEN){int i=idx((char*)a);if(i<0||!exists[i])return -1;positions[i]=0;return i+1;}
 if(n==S_CLOSE){closed++;return 0;}
 if(n==S_READ||n==S_WRITE){assert(a>=1&&a<=2&&c<=MAXFILE);int i=a-1;
  if(n==S_READ){int take=MIN(c,sizes[i]-positions[i]);if(partial)take=MIN(take,3173);memcpy((void*)b,files[i]+positions[i],take);positions[i]+=take;return take;}
  if(fail_write)return -1;assert(positions[i]+c<=sizeof files[i]);memcpy(files[i]+positions[i],(void*)b,c);positions[i]+=c;sizes[i]=positions[i];return c;
 }
 assert(0);return -1;
}
#undef sc
#define sc(n,a,b,c,d) hostcall(n,(u64)(a),(u64)(b),(u64)(c),(u64)(d))
#include "../userspace/ui.c"
int main(void){
 static char payload[131199],readback[262144];for(unsigned i=0;i<sizeof payload;i++)payload[i]=(i*17+23)%251;
 assert(writefile("/source",payload,sizeof payload)==sizeof payload&&sizes[0]==sizeof payload);partial=1;assert(readfile("/source",readback,sizeof readback)==sizeof payload&&!memcmp(payload,readback,sizeof payload));partial=0;
 assert(!copyfile("/source","/destination")&&sizes[1]==sizeof payload&&!memcmp(files[0],files[1],sizeof payload));assert(copyfile("/source","/destination")<0&&!memcmp(files[0],files[1],sizeof payload));
 assert(writefile("/destination","",0)==0&&!sizes[1]);fail_write=1;assert(writefile("/destination",payload,sizeof payload)<0);assert(readfile("/source",readback,-1)<0);assert(closed>=7);
 puts("PASS shared UI streaming: 128KiB+ files read/write/copy in <=8KiB syscalls, partial read continuation, exclusive destination creation, zero-length truncation and explicit write failure");
}
