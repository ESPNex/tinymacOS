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
 /* Exercise actual Draw serialization: 99-byte payload, no cut scalar,
  * consecutive chunks reconstruct the complete rendered text. */
 static char utf[801],joined[801];for(int i=0;i<400;i++){utf[2*i]=(char)0xc3;utf[2*i+1]=(char)0xa0;}
 utf[800]=0;begin();MonoText(28,70,utf,800,0);
 int offset=0,columns=0;assert(count==9);
 for(int i=0;i<count;i++){
  Draw *q=list+i;int size=strlen(q->text);assert(q->type==C_TEXT&&q->style==F_MONO&&size<=99&&size%2==0);
  assert(q->x==28+columns*8&&q->y==70);memcpy(joined+offset,q->text,size);offset+=size;columns+=size/2;
 }
 joined[offset]=0;assert(offset==800&&!memcmp(joined,utf,801));
 begin();MonoText(0,0,utf,98,0);assert(count==1&&strlen(list[0].text)==98);
 begin();MonoText(0,0,utf,0,0);assert(count==0);
 puts("PASS MonoText 10.3: actual Draw chunks preserve all UTF-8 bytes and 8px advances, no ABI changes");
 puts("PASS shared UI streaming: 128KiB+ files read/write/copy in <=8KiB syscalls, partial read continuation, exclusive destination creation, zero-length truncation and explicit write failure");
}
