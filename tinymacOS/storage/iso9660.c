/* SPDX-License-Identifier: MIT
 * Strict read-only ISO9660 primary namespace. No UDF/Joliet/Rock Ridge,
 * symlink interpretation, multi-extent files, interleaving or XAR support.
 */
#include "foreign_volume.h"
#include "abi.h"
static u32 leiso(const u8*p){return p[0]|(u32)p[1]<<8|(u32)p[2]<<16|(u32)p[3]<<24;}
static u32 beiso(const u8*p){return (u32)p[0]<<24|(u32)p[1]<<16|(u32)p[2]<<8|p[3];}
typedef struct {u64 at,bytes;int dir;} IsoNode;
static int isord(ForeignIo*i,u64 at,void*p,size_t n){
 if(!i||!i->read||i->sectors>~(u64)0/512||at>i->sectors*512||n>i->sectors*512-at)return -1;
 while(n){u8 b[512];unsigned off=at%512,take=MIN(n,512-off);if(i->read(i->context,at/512,1,b))return -1;memcpy(p,b+off,take);at+=take;p=(u8*)p+take;n-=take;}return 0;
}
static int record(ForeignIo*i,const u8*p,unsigned n,IsoNode*out){
 if(n<34||p[0]!=n||p[1]||p[26]||p[27]||(p[25]&~3u)||p[32]>n-33||leiso(p+2)!=beiso(p+6)||leiso(p+10)!=beiso(p+14)||p[28]!=1||p[29]||p[30]||p[31]!=1)return -1;
 u64 at=(u64)leiso(p+2)*2048,bytes=leiso(p+10);if(at>i->sectors*512||bytes>i->sectors*512-at)return -1;
 *out=(IsoNode){at,bytes,!!(p[25]&2)};return 0;
}
static int rootiso(ForeignIo*i,IsoNode*out){
 u8 b[2048];for(unsigned block=16;block<80;block++){
  if(isord(i,(u64)block*2048,b,sizeof b)||memcmp(b+1,"CD001",5)||b[6]!=1)return -1;
  if(b[0]==255)return -1;if(b[0]!=1)continue;
  if(b[128]!=0||b[129]!=8||b[130]!=8||b[131]!=0||leiso(b+80)!=beiso(b+84)||(u64)leiso(b+80)*2048>i->sectors*512)return -1;
  return record(i,b+156,b[156],out)||!out->dir?-1:0;
 }return -1;
}
static int nameiso(const u8*p,char*out){
 unsigned n=p[32];if(n==1&&(p[33]==0||p[33]==1))return 1;
 unsigned used=0;for(unsigned j=0;j<n;j++){u8 c=p[33+j];if(c==';')break;if(c<32||c>126||c=='/'||c=='\\'||used>=255)return -1;out[used++]=c;}
 while(used&&out[used-1]=='.')used--;out[used]=0;return used?0:-1;
}
static int sameiso(const char*a,const char*b){while(*a&&*b){int x=*a++,y=*b++;if(x>='a'&&x<='z')x-=32;if(y>='a'&&y<='z')y-=32;if(x!=y)return 0;}return !*a&&!*b;}
/* 1 record, 0 end, -1 corrupt. Cursor advances monotonically and is bounded. */
static int nextiso(ForeignIo*i,IsoNode*dir,u64*at,IsoNode*out,char*name){
 if(!dir->dir||dir->bytes>16*1024*1024)return -1;
 while(*at<dir->bytes){u8 p[256];if(isord(i,dir->at+*at,p,1))return -1;
  unsigned n=p[0];if(!n){*at=MIN(dir->bytes,(*at/2048+1)*2048);continue;}
  if(n>2048-*at%2048||n>dir->bytes-*at||isord(i,dir->at+*at,p,n)||record(i,p,n,out))return -1;*at+=n;
  int r=nameiso(p,name);if(r<0)return -1;if(!r)return 1;
 }return 0;
}
static int resolveiso(ForeignIo*i,const char*path,IsoNode*out){
 if(!path||*path!='/'||rootiso(i,out))return -1;unsigned depth=0;
 while(*path){while(*path=='/')path++;if(!*path)break;if(++depth>48)return -1;char wanted[256];unsigned n=0;
  while(*path&&*path!='/'){if(n>=255)return -1;wanted[n++]=*path++;}wanted[n]=0;if(eq(wanted,".")||eq(wanted,".."))return -1;
  u64 at=0;IsoNode found;char name[256];int r;while((r=nextiso(i,out,&at,&found,name))==1)if(sameiso(wanted,name))break;
  if(r!=1)return -1;*out=found;
 }return 0;
}
int iso_volume_stat(ForeignIo*i,const char*path,ForeignEntry*out){IsoNode n;if(!out||resolveiso(i,path,&n))return -1;memset(out,0,sizeof *out);out->size=n.bytes;out->dir=n.dir;out->ro=1;return 0;}
int iso_volume_list(ForeignIo*i,const char*path,ForeignEntry*out,int cap,int skip){
 IsoNode dir;if(cap<0||skip<0||(!out&&cap)||resolveiso(i,path,&dir)||!dir.dir)return -1;
 u64 at=0;IsoNode node;char name[256];int n=0,r=0;while(n<cap&&(r=nextiso(i,&dir,&at,&node,name))==1){if(skip){skip--;continue;}out[n]=(ForeignEntry){.size=node.bytes,.dir=node.dir,.ro=1};strset(out[n++].name,name,sizeof out[0].name);}return r<0?-1:n;
}
i64 iso_volume_read(ForeignIo*i,const char*path,u64 off,void*p,size_t n){IsoNode node;if((n&&!p)||resolveiso(i,path,&node)||node.dir)return -1;if(off>=node.bytes)return 0;n=MIN(n,node.bytes-off);return isord(i,node.at+off,p,n)?-1:(i64)n;}
