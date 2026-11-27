/* SPDX-License-Identifier: GPL-2.0-or-later
 * Streaming preserving export to a NEW REAL raw exFAT partition.
 * Caller validates free extent, zeroes it, verifies writes, then publishes.
 * Unicode upcase data: exfatprogs 1.2.9, original pinned source in vendor.
 */
#include "exfat_copy.h"
#define EXFAT_UPCASE_TABLE_SIZE 5836
#include "../vendor/exfat-upcase/upcase_table.h"
#define CLUSTER 4096u
#define PARTITION 0ull
#define LIMIT (2040ull*1024*1024*1024)
typedef struct Node {struct Node*next,*children;u64 id,size;u32 start,count,entries;u16 name[48];unsigned len;int dir;} Node;
typedef struct {ExCopyIo io;Node*root;u16*upper;u32 clusters,fat_sectors,heap,next,bitmap_start,bitmap_count,up_start;u64 volume,bitmap_bytes;int error;u8 buffer[4096];} Copy;
static void w16(u8*p,u16 v){p[0]=v;p[1]=v>>8;}
static void w32(u8*p,u32 v){w16(p,v);w16(p+2,v>>16);}
static void w64(u8*p,u64 v){w32(p,v);w32(p+4,v>>32);}
static u16 sum16(u16 sum,u8 b){return (u16)((sum>>1)|(sum<<15))+b;}
static u32 sum32(u32 sum,u8 b){return ((sum>>1)|(sum<<31))+b;}
static int put(Copy*c,u64 at,const void*p,size_t n){if(c->io.write(c->io.context,at,p,n)){c->error=-3;return -1;}return 0;}
static u64 cluster_offset(Copy*c,u32 cluster){return PARTITION+(u64)c->heap*512+(u64)(cluster-2)*CLUSTER;}
static int name16(const char*s,u16 out[48],unsigned*len){
 unsigned n=0;const u8*p=(const u8*)s;while(*p){u32 v=*p++;unsigned more=0;
  if(v>=0xc2&&v<=0xdf){v&=31;more=1;}else if(v>=0xe0&&v<=0xef){v&=15;more=2;}else if(v>=0xf0&&v<=0xf4){v&=7;more=3;}else if(v>=128)return -1;
  unsigned bytes=more;while(more--){if((*p&0xc0)!=0x80)return -1;v=(v<<6)|(*p++&63);}
  if((bytes==1&&v<128)||(bytes==2&&v<2048)||(bytes==3&&v<65536)||v>0x10ffff||(v>=0xd800&&v<=0xdfff)||v<32||v=='"'||v=='*'||v=='/'||v==':'||v=='<'||v=='>'||v=='?'||v=='\\'||v=='|')return -1;
  if(v>=65536){if(n+2>47)return -1;v-=65536;out[n++]=0xd800+(v>>10);out[n++]=0xdc00+(v&1023);}else{if(n>=47)return -1;out[n++]=v;}
 }if(!n||out[n-1]==' '||out[n-1]=='.')return -1;*len=n;return 0;
}
static int reserved(const u16*s,unsigned len){
 char stem[9];unsigned n=0;while(n<len&&s[n]!='.'&&n<8){u16 c=s[n];stem[n]=(c>='a'&&c<='z')?c-32:c<128?c:'?';n++;}stem[n]=0;
 if(eq(stem,"CON")||eq(stem,"PRN")||eq(stem,"AUX")||eq(stem,"NUL"))return 1;
 return n==4&&(stem[3]>='1'&&stem[3]<='9')&&((stem[0]=='C'&&stem[1]=='O'&&stem[2]=='M')||(stem[0]=='L'&&stem[1]=='P'&&stem[2]=='T'));
}
static int same(Copy*c,Node*a,Node*b){if(a->len!=b->len)return 0;for(unsigned i=0;i<a->len;i++)if(c->upper[a->name[i]]!=c->upper[b->name[i]])return 0;return 1;}
static void release(Copy*c,Node*n){while(n){Node*next=n->next;release(c,n->children);c->io.free(c->io.context,n);n=next;}}
static Node*tree(Copy*c,const ExEntry*e,unsigned depth){
 if(depth>48||e->size>LIMIT){c->error=-1;return 0;}Node*n=c->io.alloc(c->io.context,sizeof *n);if(!n){c->error=-1;return 0;}memset(n,0,sizeof *n);n->id=e->id;n->size=e->size;n->dir=e->dir;
 if(depth&&(name16(e->name,n->name,&n->len)||reserved(n->name,n->len))){c->error=-2;goto bad;}
 if(n->dir){u64 cookie=0,previous=~0ull;ExEntry child;Node**tail=&n->children;int r;
  n->entries=depth?0:3;
  while((r=c->io.next(c->io.context,n->id,&cookie,&child))==1){if(cookie==previous||!cookie){c->error=-1;goto bad;}previous=cookie;
   if(child.id==n->id){c->error=-1;goto bad;}Node*q=tree(c,&child,depth+1);if(!q)goto bad;
   for(Node*s=n->children;s;s=s->next)if(same(c,s,q)){release(c,q);c->error=-2;goto bad;}
   *tail=q;tail=&q->next;unsigned add=2+(q->len+14)/15;if(n->entries>0x7fffffffu-add){c->error=-1;goto bad;}n->entries+=add;
  }if(r<0){c->error=-1;goto bad;}n->size=((u64)(n->entries+1)*32+CLUSTER-1)/CLUSTER*CLUSTER;
 }
 n->count=(n->size+CLUSTER-1)/CLUSTER;return n;
 bad:release(c,n);return 0;
}
static int count(Node*n,u64*total){for(;n;n=n->next){if(*total>LIMIT/CLUSTER-n->count)return -1;*total+=n->count;if(count(n->children,total))return -1;}return 0;}
static void assign(Copy*c,Node*n){for(;n;n=n->next){n->start=n->count?c->next:0;c->next+=n->count;assign(c,n->children);}}
static u32 fat_value(Node*n,u32 cluster){for(;n;n=n->next){if(n->count&&cluster>=n->start&&cluster-n->start<n->count)return cluster-n->start+1<n->count?cluster+1:0xffffffffu;u32 v=fat_value(n->children,cluster);if(v)return v;}return 0;}
static int boot(Copy*c){
 u8 sector[512];u32 checksum=0;
 for(unsigned s=0;s<11;s++){
  memset(sector,0,512);
  if(!s){sector[0]=0xeb;sector[1]=0x76;sector[2]=0x90;memcpy(sector+3,"EXFAT   ",8);w64(sector+64,c->io.partition_lba?c->io.partition_lba(c->io.context):0);w64(sector+72,c->volume);w32(sector+80,24);w32(sector+84,c->fat_sectors);w32(sector+88,c->heap);w32(sector+92,c->clusters);w32(sector+96,c->root->start);w32(sector+100,c->io.serial);w16(sector+104,0x100);sector[108]=9;sector[109]=3;sector[110]=1;sector[111]=0x80;sector[112]=((u64)(c->next-2)*100+c->clusters-1)/c->clusters;sector[510]=85;sector[511]=170;}
  else if(s<=8)w32(sector+508,0xaa550000u);
  for(unsigned j=0;j<512;j++)if(s|| (j!=106&&j!=107&&j!=112))checksum=sum32(checksum,sector[j]);
  if(put(c,PARTITION+s*512,sector,512)||put(c,PARTITION+(s+12)*512,sector,512))return -1;
 }
 for(unsigned j=0;j<512;j+=4)w32(sector+j,checksum);
 return put(c,PARTITION+11*512,sector,512)||put(c,PARTITION+23*512,sector,512)?-1:0;
}
static u16 namehash(Copy*c,Node*n){u16 sum=0;for(unsigned i=0;i<n->len;i++){u16 v=c->upper[n->name[i]];sum=sum16(sum,v);sum=sum16(sum,v>>8);}return sum;}
static int dirwrite(Copy*c,Node*n,u64*offset,const void*data,size_t bytes){if(*offset>n->size||bytes>n->size-*offset)return -1;if(put(c,cluster_offset(c,n->start)+*offset,data,bytes))return -1;*offset+=bytes;return 0;}
static int write_tree(Copy*c,Node*n,int root){
 for(;n;n=n->next){
  if(!n->dir){u64 off=0;while(off<n->size){size_t k=MIN(sizeof c->buffer,n->size-off);if(c->io.read(c->io.context,n->id,off,c->buffer,k)!=(i64)k){c->error=-1;return -1;}if(put(c,cluster_offset(c,n->start)+off,c->buffer,k))return -1;off+=k;}continue;}
  u64 at=0;u8 entry[32];if(root){
   memset(entry,0,32);entry[0]=0x83;const char*label="Copia exFAT";entry[1]=strlen(label);for(unsigned i=0;label[i];i++)w16(entry+2+i*2,label[i]);if(dirwrite(c,n,&at,entry,32))return -1;
   memset(entry,0,32);entry[0]=0x81;w32(entry+20,c->bitmap_start);w64(entry+24,c->bitmap_bytes);if(dirwrite(c,n,&at,entry,32))return -1;
   memset(entry,0,32);entry[0]=0x82;u32 checksum=0;for(unsigned i=0;i<sizeof default_upcase_table;i++)checksum=sum32(checksum,default_upcase_table[i]);w32(entry+4,checksum);w32(entry+20,c->up_start);w64(entry+24,sizeof default_upcase_table);if(dirwrite(c,n,&at,entry,32))return -1;
  }
  for(Node*q=n->children;q;q=q->next){u8 set[192];unsigned names=(q->len+14)/15,bytes=(2+names)*32;memset(set,0,bytes);
   set[0]=0x85;set[1]=1+names;w16(set+4,q->dir?0x10:0x20);w32(set+8,0x5d7253c0u);w32(set+12,0x5d7253c0u);w32(set+16,0x5d7253c0u);set[22]=set[23]=set[24]=0x80;
   u8*s=set+32;s[0]=0xc0;s[1]=1;s[3]=q->len;w16(s+4,namehash(c,q));w64(s+8,q->size);w32(s+20,q->start);w64(s+24,q->size);
   for(unsigned i=0;i<names;i++){u8*p=set+64+i*32;p[0]=0xc1;for(unsigned j=0;j<15&&i*15+j<q->len;j++)w16(p+2+j*2,q->name[i*15+j]);}
   u16 sum=0;for(unsigned i=0;i<bytes;i++)if(i!=2&&i!=3)sum=sum16(sum,set[i]);w16(set+2,sum);if(dirwrite(c,n,&at,set,bytes))return -1;
  }
  if(write_tree(c,n->children,0))return -1;
 }return 0;
}
int exfat_copy(ExCopyIo io,u64 source,u64*required){
 if(!required||!io.next||!io.read||!io.write||!io.alloc||!io.free)return -1;*required=0;Copy*c=io.alloc(io.context,sizeof *c);if(!c)return -1;memset(c,0,sizeof *c);c->io=io;c->error=-1;
 c->upper=io.alloc(io.context,65536*2);if(!c->upper)goto done;
 unsigned pos=0,index=0;while(index<65536){if(pos+2>sizeof default_upcase_table)goto done;u16 v=default_upcase_table[pos]|(u16)default_upcase_table[pos+1]<<8;pos+=2;
  if(v==0xffff&&index!=0xffff){if(pos+2>sizeof default_upcase_table)goto done;unsigned n=default_upcase_table[pos]|(u16)default_upcase_table[pos+1]<<8;pos+=2;if(!n||n>65536-index)goto done;while(n--){c->upper[index]=index;index++;}}
  else c->upper[index++]=v;
 }
 ExEntry e={.id=source,.dir=1};c->root=tree(c,&e,0);if(!c->root)goto done;u64 used=0;if(count(c->root,&used))goto done;
 c->clusters=MAX(8192ull,used+128);c->bitmap_bytes=(c->clusters+7ull)/8;c->bitmap_count=(c->bitmap_bytes+CLUSTER-1)/CLUSTER;
 c->bitmap_start=2;c->up_start=2+c->bitmap_count;c->next=c->up_start+2;assign(c,c->root);
 if(c->next-2>c->clusters)goto done;c->fat_sectors=((u64)(c->clusters+2)*4+511)/512;c->heap=(24+c->fat_sectors+7)&~7u;c->volume=(u64)c->heap+(u64)c->clusters*8;
 if(c->volume>LIMIT/512)goto done;*required=((c->volume*512+1048575)/1048576)*1048576;
 if(io.prepare&&io.prepare(io.context,*required,(u64)(c->next-2)*CLUSTER+(u64)c->fat_sectors*512+24*512+32768))goto done;
 if(boot(c))goto done;
 for(u64 at=0;at<(u64)c->fat_sectors*512;at+=4096){unsigned bytes=MIN(4096ull,(u64)c->fat_sectors*512-at);memset(c->buffer,0,bytes);for(unsigned j=0;j<bytes;j+=4){u32 cluster=(at+j)/4,v=0;if(cluster==0)v=0xfffffff8;else if(cluster==1)v=0xffffffff;else if(cluster>=c->bitmap_start&&cluster<c->up_start)v=cluster+1<c->up_start?cluster+1:0xffffffff;else if(cluster>=c->up_start&&cluster<c->up_start+2)v=cluster==c->up_start?cluster+1:0xffffffff;else if(cluster<c->next)v=fat_value(c->root,cluster);w32(c->buffer+j,v);}if(put(c,24*512+at,c->buffer,bytes))goto done;}

 for(u64 at=0;at<c->bitmap_bytes;at+=4096){unsigned bytes=MIN(4096ull,c->bitmap_bytes-at);memset(c->buffer,0,bytes);for(unsigned j=0;j<bytes;j++){u64 bit=(at+j)*8;unsigned n=bit<c->next-2?MIN(8ull,c->next-2-bit):0;c->buffer[j]=(1u<<n)-1;}if(put(c,cluster_offset(c,c->bitmap_start)+at,c->buffer,bytes))goto done;}
 if(put(c,cluster_offset(c,c->up_start),default_upcase_table,sizeof default_upcase_table)||write_tree(c,c->root,1))goto done;c->error=0;
 done:{int result=c->error;release(c,c->root);io.free(io.context,c->upper);io.free(io.context,c);return result;}
}
