/* SPDX-License-Identifier: MIT
 * Defensive FAT32/exFAT/NTFS reader, deliberately without writes.
 * Rejects encrypted/compressed/attribute-list NTFS streams rather than leaking
 * raw ciphertext or returning fabricated plaintext. Traversals are bounded.
 */
#include "foreignfs.h"
static u16 set_sum(u16 sum,const u8*b,int primary){for(unsigned i=0;i<32;i++)if(!primary||(i!=2&&i!=3))sum=(u16)((sum>>1)|(sum<<15))+b[i];return sum;}
static u16 le16(const u8*p){return p[0]|(u16)p[1]<<8;}
static u32 le32(const u8*p){return le16(p)|(u32)le16(p+2)<<16;}
static u64 le64(const u8*p){return le32(p)|(u64)le32(p+4)<<32;}
static int power(u32 n){return n&&!(n&(n-1));}
static int rd(ForeignFs*f,u64 at,void*p,size_t n){return !f||!f->read||at>f->bytes||n>f->bytes-at?-1:f->read(f->context,at,p,n);}
static int utf8(const u16*s,unsigned n,char out[48]){
 unsigned used=0;for(unsigned i=0;i<n&&s[i]&&s[i]!=0xffff;i++){
  u32 c=s[i];if(c>=0xd800&&c<=0xdbff){if(++i>=n||s[i]<0xdc00||s[i]>0xdfff)return -1;c=0x10000+((c-0xd800)<<10)+(s[i]-0xdc00);}else if(c>=0xdc00&&c<=0xdfff)return -1;
  unsigned len=c<128?1:c<2048?2:c<65536?3:4;if(c=='/'||c<32||used+len>=48)return -1;
  if(len==1)out[used++]=c;else{out[used++]=(len==2?0xc0:len==3?0xe0:0xf0)|(c>>(6*(len-1)));for(unsigned j=len-1;j;j--)out[used++]=0x80|((c>>(6*(j-1)))&63);}
 }out[used]=0;return used?0:-1;
}
/* Stable aliases for names beyond the OS's 47-byte directory ABI. The full
 * on-disk name is never rewritten. IDs make the aliases unambiguous. */
static void alias(char out[48],u64 id){strset(out,"Nome-lungo-",48);char num[24];number(num,id);append(out,num,48);}
static int cluster_ok(ForeignFs*f,u32 c){return c>=2&&(u64)c<f->clusters+2ull;}
static int next_cluster(ForeignFs*f,u32 c,u32*out){
 u8 b[4];if(!cluster_ok(f,c))return -1;u32 n,end;
 if(f->type==FS_FAT12){if(rd(f,f->fat+c+c/2,b,2))return -1;n=le16(b);n=c&1?n>>4:n&4095;end=0xff8;}
 else if(f->type==FS_FAT16){if(rd(f,f->fat+(u64)c*2,b,2))return -1;n=le16(b);end=0xfff8;}
 else{if(rd(f,f->fat+(u64)c*4,b,4))return -1;n=le32(b);if(f->type==FS_FAT32)n&=0xfffffff;end=f->type==FS_FAT32?0xffffff8u:0xfffffff8u;}
 if(n>=end){*out=0;return 0;}if(!cluster_ok(f,n)||n==c)return -1;*out=n;return 0;
}
static int chain_at(ForeignFs*f,u32 start,u64 hops,u32*out){
 u32 c=start,slow=start,fast=start;int detect=1;
 while(hops--){if(next_cluster(f,c,&c))return -1;if(!c){*out=0;return 0;}
  if(detect){if(next_cluster(f,slow,&slow))return -1;for(int i=0;i<2&&fast;i++)if(next_cluster(f,fast,&fast))return -1;
   if(!fast||!slow)detect=0;else if(fast==slow)return -1;
  }
 }*out=c;return 0;
}
static int cluster_at(ForeignFs*f,const FvNode*n,u64 index,u32*out){
 if(index>=f->clusters||!cluster_ok(f,n->start))return -1;u32 c=n->start;
 if(f->type==FS_EXFAT&&(n->flags&2)){if(index>f->clusters+1ull-c)return -1;c+=index;}
 else if(chain_at(f,c,index,&c)||!c)return -1;
 *out=c;return 0;
}
static i64 cluster_read(ForeignFs*f,const FvNode*n,u64 off,void*p,size_t bytes,int directory){
 if(!directory){if(off>=n->size)return 0;bytes=MIN(bytes,n->size-off);}size_t done=0;
 while(done<bytes){u64 at=off+done;u32 c;if(cluster_at(f,n,at/f->cluster,&c))return -1;
  size_t k=MIN(bytes-done,f->cluster-at%f->cluster);
  if(f->type==FS_EXFAT&&at>=n->valid&&!directory)memset((u8*)p+done,0,k);
  else{if(f->type==FS_EXFAT&&!directory)k=MIN(k,n->valid-at);if(rd(f,f->heap+(u64)(c-2)*f->cluster+at%f->cluster,(u8*)p+done,k))return -1;}
  done+=k;
 }return done;
}
static int dir_entry(ForeignFs*f,const FvNode*d,u64 index,u8 b[32]){
 if(f->root_entries&&(d->flags&0x80000000u)){if(index>=f->root_entries)return 0;return rd(f,f->fixed_root+index*32,b,32)?-1:1;}
 if(index>=(u64)f->clusters*f->cluster/32)return -1;
 if(d->size&&index>=d->size/32)return 0;
 u64 offset=index*32;u32 c=d->start;u64 hops=offset/f->cluster;
 if(!cluster_ok(f,c))return -1;
 if(f->type==FS_EXFAT&&(d->flags&2)){if(hops>f->clusters+1ull-c)return -1;c+=hops;}
 else{if(chain_at(f,c,hops,&c))return -1;if(!c)return 0;}
 return rd(f,f->heap+(u64)(c-2)*f->cluster+offset%f->cluster,b,32)?-1:1;
}
/* Consume mapping pairs directly. The encoded record bounds the input;
 * no expanded-run count or provisional 1 Ti-cluster ceiling is imposed. */
static int runs(ForeignFs*f,const u8*p,size_t len,u64 target,FvRun*out,unsigned*used){
 u64 vcn=0;i64 lcn=0;size_t at=0;int found=0;u64 physical=f->bytes/f->cluster,maximum=~(u64)0/f->cluster;
 while(at<len&&p[at]){unsigned a=p[at]&15,b=p[at]>>4;at++;if(!a||a>8||b>8||a+b>len-at)return -1;
  u64 length=0,delta=0;for(unsigned i=0;i<a;i++)length|=(u64)p[at+i]<<(8*i);at+=a;
  for(unsigned i=0;i<b;i++)delta|=(u64)p[at+i]<<(8*i);if(b&&b<8&&(p[at+b-1]&128))delta|=~0ull<<(8*b);at+=b;
  if(!length||length>maximum||vcn>maximum-length)return -1;
  i64 change=(i64)delta;if(b&&((change>0&&lcn>INT64_MAX-change)||(change<0&&change< -lcn)))return -1;if(b)lcn+=change;
  if(b&&((u64)lcn>=physical||length>physical-(u64)lcn))return -1;
  if(out&&target>=vcn&&target-vcn<length){*out=(FvRun){vcn,(u64)lcn,length,!b};found=1;}
  vcn+=length;
 }if(at>=len||!vcn||(out&&!found))return -1;if(used)*used=at+1;return 0;
}
static int run_read(ForeignFs*f,const u8*r,unsigned nr,u64 off,void*p,size_t len){
 while(len){u64 v=off/f->cluster;FvRun run;if(runs(f,r,nr,v,&run,NULL))return -1;
  size_t n=MIN((u64)len,(run.length-(v-run.vcn))*f->cluster-off%f->cluster);
  if(run.sparse)memset(p,0,n);else if(rd(f,(run.lcn+v-run.vcn)*f->cluster+off%f->cluster,p,n))return -1;
  p=(u8*)p+n;off+=n;len-=n;
 }return 0;
}
static int record(ForeignFs*f,u64 id,u8 b[4096],int first){
 if(f->record>4096||id>=f->mft_size/f->record)return -1;
 if(first?rd(f,f->mft,b,f->record):run_read(f,f->runs,f->nruns,id*f->record,b,f->record))return -1;
 if(memcmp(b,"FILE",4))return -1;u16 at=le16(b+4),n=le16(b+6);
 if(n!=f->record/f->bps+1||at<8||(u32)at+n*2>f->record)return -1;
 for(unsigned i=1;i<n;i++){u8*p=b+i*f->bps-2;if(le16(p)!=le16(b+at))return -1;memcpy(p,b+at+i*2,2);}
 if(le32(b+24)>f->record||le32(b+24)<le16(b+20)||le16(b+20)<24)return -1;return 0;
}
static const u8*attr_next(const u8*b,u32*at,u32 limit){
 if(*at>limit||limit-*at<8)return 0;const u8*a=b+*at;if(le32(a)==0xffffffff)return 0;u32 size=le32(a+4);
 if(size<24||size>limit-*at||size&7){*at=0xffffffff;return 0;}*at+=size;return a;
}
static int data_attr(ForeignFs*f,const u8*b,u64*size,const u8**data,const u8**mapping,unsigned*nr,u64*valid){
 u32 at=le16(b+20),limit=le32(b+24);const u8*a;int found=0;
 while((a=attr_next(b,&at,limit))){u32 type=le32(a),len=le32(a+4);if(type==0x20)return -1;
  if(type!=0x80||a[9])continue;if(found++||le16(a+12)&0x4001)return -1;
  if(a[8]){if(len<64||le64(a+16)||le16(a+32)<64||le16(a+32)>=len||runs(f,a+le16(a+32),len-le16(a+32),0,NULL,nr))return -1;*size=le64(a+48);*valid=le64(a+56);if(*valid>*size)return -1;*data=0;*mapping=a+le16(a+32);}
  else{u32 n=le32(a+16),off=le16(a+20);if(off<24||off>len||n>len-off)return -1;*size=*valid=n;*data=a+off;*nr=0;}
 }return at==0xffffffff||!found?-1:0;
}
int fv_mount(ForeignFs*f,void*context,u64 bytes,int(*read)(void*,u64,void*,size_t)){
 if(!f||!read||bytes<512)return -1;memset(f,0,sizeof *f);f->context=context;f->read=read;f->bytes=bytes;u8 b[512];if(rd(f,0,b,512))return -1;
 if(b[510]!=85||b[511]!=170)return -1;
 if(!memcmp(b+3,"NTFS    ",8)){
  f->type=FS_NTFS;f->bps=le16(b+11);if((f->bps!=512&&f->bps!=4096)||!power(b[13])||b[13]>128||!le64(b+40)||le64(b+40)>bytes/f->bps)return -1;
  f->cluster=f->bps*b[13];if(le64(b+48)>=bytes/f->cluster)return -1;f->mft=le64(b+48)*f->cluster;int v=(int8_t)b[64];f->record=v<0&&v>=-12?1u<<-v:v>0?(u32)v*f->cluster:0;
  if(!f->record||f->record>4096||f->record<f->bps||f->mft>=bytes)return -1;f->mft_size=bytes;u8 rec[4096];if(record(f,0,rec,1))return -1;
  const u8*data,*mapping;u64 valid;if(data_attr(f,rec,&f->mft_size,&data,&mapping,&f->nruns,&valid)||data||!f->nruns||f->nruns>sizeof f->runs||f->mft_size>bytes)return -1;memcpy(f->runs,mapping,f->nruns);f->mft_size=MIN(f->mft_size,valid);if(f->mft_size<6ull*f->record)return -1;return 0;
 }
 if(!memcmp(b+3,"EXFAT   ",8)){
  f->type=FS_EXFAT;if(b[108]<9||b[108]>12||b[109]>25-b[108]||b[110]!=1||le16(b+106)&1)return -1;
  f->bps=1u<<b[108];f->cluster=f->bps<<b[109];u64 total=le64(b+72);f->fat=(u64)le32(b+80)*f->bps;f->heap=(u64)le32(b+88)*f->bps;f->clusters=le32(b+92);f->root=le32(b+96);
  if(!total||total>bytes/f->bps||!f->clusters||!cluster_ok(f,f->root)||f->fat<24ull*f->bps||f->heap<f->fat+(u64)le32(b+84)*f->bps||(u64)(f->clusters+2ull)*4>(u64)le32(b+84)*f->bps||f->heap>total*f->bps||(u64)f->clusters*f->cluster>total*f->bps-f->heap)return -1;
  return 0;
 }
 f->bps=le16(b+11);u32 spc=b[13],nf=b[16],reserved=le16(b+14),fat=le16(b+22),total=le16(b+19),roots=le16(b+17);int fat32=!fat;
 if(!total)total=le32(b+32);if(fat32)fat=le32(b+36);
 if((f->bps!=512&&f->bps!=4096)||!power(spc)||spc>128||!reserved||!nf||nf>2||!fat||!total||total>bytes/f->bps||(fat32&&roots)||(!fat32&&!roots))return -1;
 f->cluster=f->bps*spc;f->fat=(u64)reserved*f->bps;f->fixed_root=((u64)reserved+(u64)nf*fat)*f->bps;
 f->heap=f->fixed_root+((u64)roots*32+f->bps-1)/f->bps*f->bps;
 if(f->heap>=(u64)total*f->bps)return -1;u64 clusters=((u64)total*f->bps-f->heap)/f->cluster;if(!clusters||clusters>=0xffffff5u)return -1;f->clusters=clusters;
 f->type=clusters<4085?FS_FAT12:clusters<65525?FS_FAT16:FS_FAT32;if(fat32!=(f->type==FS_FAT32))return -1;
 if(fat32){f->root=le32(b+44);u16 flags=le16(b+40);if(flags&128){unsigned active=flags&15;if(active>=nf)return -1;f->fat+=(u64)active*fat*f->bps;}if(!cluster_ok(f,f->root))return -1;}
 else f->root_entries=roots;
 u64 required=f->type==FS_FAT12?((clusters+2)*3+1)/2:(clusters+2)*(f->type==FS_FAT16?2:4);
 return required>(u64)fat*f->bps?-1:0;
}
int fv_root(ForeignFs*f,FvNode*n){
 if(!f||!n)return -1;memset(n,0,sizeof *n);n->dir=1;n->id=f->type==FS_NTFS?5:f->root;n->start=n->id;
 if(f->root_entries)n->flags=0x80000000u;strset(n->name,"/",48);
 if(f->type==FS_NTFS){u8 b[4096];if(record(f,5,b,0)||(le16(b+22)&3)!=3)return -1;}
 else{u8 b[32];if(dir_entry(f,n,0,b)!=1)return -1;}
 return 0;
}
int fv_next(ForeignFs*f,const FvNode*d,u64*cookie,FvNode*n){
 if(!f||!d||!d->dir||!cookie||!n)return -1;
 if(f->type==FS_NTFS){
  while(*cookie<f->mft_size/f->record){u64 id=(*cookie)++;if(id<16)continue;u8 b[4096];if(record(f,id,b,0))return -1;if(!(le16(b+22)&1))continue;
   u32 at=le16(b+20),limit=le32(b+24);const u8*a;int have=0;u64 size=0;char name[48];
   while((a=attr_next(b,&at,limit))){if(le32(a)!=0x30||a[8])continue;u32 len=le32(a+4),off=le16(a+20),vlen=le32(a+16);if(off>len||vlen>len-off||vlen<66)return -1;
    const u8*v=a+off;unsigned chars=v[64];if(v[65]==2||chars*2>vlen-66||(le64(v)&0xffffffffffffull)!=d->id)continue;u16 text[255];for(unsigned i=0;i<chars;i++)text[i]=le16(v+66+i*2);if(utf8(text,chars,name))alias(name,id);size=le64(v+48);have=1;break;
   }if(at==0xffffffff)return -1;if(!have)continue;
   if(!(le16(b+22)&2)){
    at=le16(b+20);int have_data=0;
    while((a=attr_next(b,&at,limit)))if(le32(a)==0x80&&!a[9]){
     u32 len=le32(a+4);if(a[8]){if(len<64)return -1;size=le64(a+48);}else size=le32(a+16);have_data=1;break;
    }
    if(at==0xffffffff)return -1;if(!have_data)size=0;
   }
   *n=(FvNode){.id=id,.start=(u32)id,.size=size,.valid=size,.dir=!!(le16(b+22)&2)};if(id>0xffffffffu)return -1;strset(n->name,name,48);return 1;
  }return 0;
 }
 u16 lfn[260];memset(lfn,0,sizeof lfn);unsigned expected=0;u8 checksum=0;
 for(;;){u8 b[32];u64 id=(*cookie)++;int r=dir_entry(f,d,id,b);if(r<=0)return r;if(!b[0])return 0;
  if(f->type==FS_FAT12||f->type==FS_FAT16||f->type==FS_FAT32){
   if(b[0]==0xe5){expected=0;continue;}if(b[11]==15){unsigned ord=b[0]&31;if(b[0]&64){memset(lfn,0xff,sizeof lfn);expected=ord;checksum=b[13];}if(!ord||ord>20||ord!=expected||b[13]!=checksum||b[12]||le16(b+26)){expected=0;continue;}
    static const u8 at[13]={1,3,5,7,9,14,16,18,20,22,24,28,30};for(unsigned j=0;j<13;j++)lfn[(ord-1)*13+j]=le16(b+at[j]);expected--;continue;}
   if((b[11]&8)||b[0]=='.'){expected=0;continue;}*n=(FvNode){.id=id,.start=(f->type==FS_FAT32?(u32)le16(b+20)<<16:0)|le16(b+26),.size=le32(b+28),.valid=le32(b+28),.dir=!!(b[11]&16)};
   u8 sum=0;for(int i=0;i<11;i++)sum=((sum&1)?128:0)+(sum>>1)+b[i];
   if(!expected&&lfn[0]!=0&&lfn[0]!=0xffff&&sum==checksum&&!utf8(lfn,260,n->name))return 1;
   unsigned used=0;for(int i=0;i<8&&b[i]!=' ';i++)n->name[used++]=(b[12]&8)&&b[i]>='A'&&b[i]<='Z'?b[i]+32:b[i];if(b[8]!=' '){n->name[used++]='.';for(int i=8;i<11&&b[i]!=' ';i++)n->name[used++]=(b[12]&16)&&b[i]>='A'&&b[i]<='Z'?b[i]+32:b[i];}n->name[used]=0;return 1;
  }
  if(b[0]!=0x85)continue;u16 expected_sum=le16(b+2),actual_sum=set_sum(0,b,1);unsigned secondary=b[1];if(secondary<2||secondary>18)return -1;u8 stream[32];if(dir_entry(f,d,(*cookie)++,stream)!=1||stream[0]!=0xc0)return -1;
  actual_sum=set_sum(actual_sum,stream,0);unsigned chars=stream[3],parts=(chars+14)/15;if(!chars||parts+1!=secondary)return -1;u16 text[255];unsigned k=0;
  for(unsigned i=0;i<parts;i++){u8 name[32];if(dir_entry(f,d,(*cookie)++,name)!=1||name[0]!=0xc1)return -1;actual_sum=set_sum(actual_sum,name,0);for(int j=0;j<15&&k<chars;j++)text[k++]=le16(name+2+j*2);}
  if(actual_sum!=expected_sum)return -1;
  *n=(FvNode){.id=id,.start=le32(stream+20),.size=le64(stream+24),.valid=le64(stream+8),.flags=stream[1],.dir=!!(le16(b+4)&16)};
  if(n->valid>n->size||n->size>f->bytes||(n->size&&!cluster_ok(f,n->start)))return -1;if(utf8(text,chars,n->name))alias(n->name,id);return 1;
 }
}
int fv_lookup(ForeignFs*f,const FvNode*d,const char*name,FvNode*n){u64 cookie=0;int r;while((r=fv_next(f,d,&cookie,n))==1)if(eq(n->name,name))return 0;return -1;}
i64 fv_read(ForeignFs*f,const FvNode*n,u64 off,void*p,size_t bytes){
 if(!f||!n||n->dir||(!p&&bytes))return -1;if(off>=n->size)return 0;bytes=MIN(bytes,n->size-off);
 if(f->type!=FS_NTFS)return cluster_read(f,n,off,p,bytes,0);
 u8 b[4096];if(record(f,n->id,b,0))return -1;u64 size,valid;const u8*data,*r;unsigned nr;
 if(data_attr(f,b,&size,&data,&r,&nr,&valid)||size!=n->size)return -1;
 if(data){memcpy(p,data+off,bytes);return bytes;}
 size_t initialized=off>=valid?0:MIN(bytes,valid-off);if(run_read(f,r,nr,off,p,initialized))return -1;memset((u8*)p+initialized,0,bytes-initialized);return bytes;
}
