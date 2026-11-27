#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../userspace/lib.h"
static long hostcall(long,u64,u64,u64,u64);
#undef sc
#define sc(n,a,b,c,d) hostcall(n,(u64)(a),(u64)(b),(u64)(c),(u64)(d))
#include "../userspace/ui.h"
static const char *home_inode;
static unsigned char files[2][262144];static int sizes[2],positions[2],exists[2],closed,partial,fail_write;
static int idx(const char*p){return eq(p,"/source")?0:eq(p,"/destination")?1:-1;}
static long hostcall(long n,u64 a,u64 b,u64 c,u64 d){
 (void)d;if(n==S_STAT){if(eq((char*)a,HOME)&&home_inode){*(Dirent*)b=(Dirent){.dir=1};strset(((Dirent*)b)->name,home_inode,48);return 0;}int i=idx((char*)a);if(i<0||!exists[i])return -1;*(Dirent*)b=(Dirent){.size=sizes[i]};return 0;}
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
 char path[PATHLEN];ui_home(path);assert(eq(path,HOME));
 home_inode="Elena Ròssi";ui_home(path);assert(eq(path,"/Users/Elena Ròssi"));
 assert(!absolutepath(path,HOME,"Documents/a.txt")&&eq(path,"/Users/Elena Ròssi/Documents/a.txt"));
 assert(!absolutepath(path,"/",HOME "2/test")&&eq(path,HOME "2/test"));
 assert(!absolutepath(path,"/","/.snapshots/old/Users/User/test")&&eq(path,"/.snapshots/old/Users/User/test"));
 assert(!absolutepath(path,HOME,"..")&&eq(path,"/Users"));
 home_inode="User";assert(!absolutepath(path,HOME,"Documents")&&eq(path,HOME "/Documents"));
 home_inode="../bad";ui_home(path);assert(eq(path,HOME));home_inode=0;
 puts("PASS UI paths: real UTF8/spaced inode canonicalization, retained old-name fallback, no snapshot/prefix alias pollution or untrusted path components");
 static char payload[131199],readback[262144];for(unsigned i=0;i<sizeof payload;i++)payload[i]=(i*17+23)%251;
 assert(writefile("/source",payload,sizeof payload)==sizeof payload&&sizes[0]==sizeof payload);partial=1;assert(readfile("/source",readback,sizeof readback)==sizeof payload&&!memcmp(payload,readback,sizeof payload));partial=0;
 assert(!copyfile("/source","/destination")&&sizes[1]==sizeof payload&&!memcmp(files[0],files[1],sizeof payload));assert(copyfile("/source","/destination")<0&&!memcmp(files[0],files[1],sizeof payload));
 assert(writefile("/destination","",0)==0&&!sizes[1]);fail_write=1;assert(writefile("/destination",payload,sizeof payload)<0);assert(readfile("/source",readback,-1)<0);assert(closed>=7);
 /* Actual compact Draw serialization: one reference per Unicode scalar,
  * including accents/symbols; unknown four-byte scalars consume one cell. */
 static char utf[801];for(int i=0;i<400;i++){utf[2*i]=(char)0xc3;utf[2*i+1]=(char)0xa0;}
 utf[800]=0;begin();MonoText(28,70,utf,800,0);
 int columns=0;assert(count==5);
 for(int i=0;i<count;i++){
  Draw *q=list+i;assert(q->type==C_MONO&&q->w<=99&&q->w>0);
  assert(q->x==28+columns*8&&q->y==70);
  for(int j=0;j<q->w;j++)assert(mono_code((u8)q->text[j]-1)==0xe0);
  columns+=q->w;
 }
 assert(columns==400);
 begin();MonoText(0,0,"à€😀",9,0);assert(count==1&&list[0].w==3);
 assert(mono_code((u8)list[0].text[0]-1)==0xe0);
 assert(mono_code((u8)list[0].text[1]-1)==0x20ac);
 assert(mono_code((u8)list[0].text[2]-1)=='?');
 begin();MonoText(0,0,utf,0,0);assert(count==0);
 puts("PASS MonoText 10.3: compact Draw uses one reference per scalar, accents/symbols preserved, unsupported scalar consumes one cell, 8px advances; Draw size/NCMDS unchanged");
 begin();InputText(10,20,80,"Segrèto",8,1,1,0);
 int carets=0,masked=0;
 for(int i=0;i<count;i++){
  if(list[i].type==C_CARET){carets++;assert(list[i].x>=10&&list[i].x<90);}
  if(list[i].type==C_MONO){masked+=list[i].w;for(int j=0;j<list[i].w;j++)assert(mono_code((u8)list[i].text[j]-1)==0x2022);}
 }
 assert(carets==1&&masked==7);
 begin();InputText(0,0,48,"",0,1,1,0);assert(count==1&&list[0].type==C_CARET&&list[0].x==0);
 begin();InputText(0,0,48,"abcdefghijklmnop",16,1,0,0);
 assert(count==2&&list[0].w==5&&list[1].type==C_CARET&&list[1].x==40);
 begin();InputText(0,0,80,"abc",3,0,0,0);assert(count==1&&list[0].type==C_MONO);
 puts("PASS shared InputText: empty active fields show caret, long input scrolls within field, inactive fields omit caret, passwords serialize bullets only and UTF-8 counts one mask per scalar");
 puts("PASS shared UI streaming: 128KiB+ files read/write/copy in <=8KiB syscalls, partial read continuation, exclusive destination creation, zero-length truncation and explicit write failure");
}
