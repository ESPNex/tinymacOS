/* SPDX-License-Identifier: MIT
 * Native, strictly read-only HFS/HFS+/HFSX catalogue and data-fork reader.
 * On-disk structures are decoded bytewise; no host ABI/packed-struct trust.
 * Supports embedded HFS+ wrappers, data/resource forks and extent overflow.
 * Does not replay journals or resolve Finder aliases, hard links, decmpfs/xattr
 * compression or symlinks. Such special-file reads fail
 * explicitly, rather than returning an apparently empty or unrelated file.
 */
#include "foreign_volume.h"
#include "abi.h"
static u16 hb2(const u8*p){return (u16)p[0]<<8|p[1];}
static u32 hb4(const u8*p){return (u32)hb2(p)<<16|hb2(p+2);}
static u64 hb8(const u8*p){return (u64)hb4(p)<<32|hb4(p+4);}
typedef struct {u32 start,count;} HExtent;
typedef struct {u64 size;u32 id,blocks;unsigned n;u8 type;HExtent e[8];} HFork;
typedef struct {u16 size,depth;u32 first,last,nodes,root;HFork*fork;u8*buffer;} HTree;
typedef struct {
 ForeignIo*io;u64 base,bytes,alloc;u32 block,blocks;int plus,sensitive;
 HFork catalogue,extents;HTree cat,ext;u8*nodes;u64 bootstrap[32];unsigned bootstrap_depth;
} Hfs;
typedef struct {u32 id;u64 size;int dir,special;HFork data,resource;char name[1021];} HFile;
static int hread_io(ForeignIo*i,u64 at,void*p,size_t n){
 if(!i||!i->read||i->sectors>~(u64)0/512||at>i->sectors*512||n>i->sectors*512-at)return -1;
 while(n){unsigned part=at%512,take=MIN(n,512-part);u8 b[512];
  if(!part&&n>=512){unsigned blocks=MIN((u64)128,n/512);take=blocks*512;if(i->read(i->context,at/512,blocks,p))return -1;}
  else{if(i->read(i->context,at/512,1,b))return -1;memcpy(p,b+part,take);}
  at+=take;p=(u8*)p+take;n-=take;
 }return 0;
}
static int hread(Hfs*h,u64 at,void*p,size_t n){return at>h->bytes||n>h->bytes-at?-1:hread_io(h->io,h->base+at,p,n);}
static int extents_valid(Hfs*h,const HExtent*e,unsigned n,u64*count){
 u64 total=0;int ended=0;for(unsigned j=0;j<n;j++){
  if(!e[j].count){if(e[j].start)return -1;ended=1;continue;}
  if(ended||e[j].start>=h->blocks||e[j].count>h->blocks-e[j].start)return -1;
  total+=e[j].count;
 }if(count)*count=total;return 0;
}
static int forkplus(Hfs*h,HFork*f,const u8*p,u32 id){
 memset(f,0,sizeof *f);f->size=hb8(p);f->blocks=hb4(p+12);f->id=id;f->n=8;
 for(unsigned j=0;j<8;j++)f->e[j]=(HExtent){hb4(p+16+j*8),hb4(p+20+j*8)};
 u64 embedded;if(f->blocks>h->blocks||f->size>(u64)f->blocks*h->block||extents_valid(h,f->e,8,&embedded)||embedded>f->blocks)return -1;return 0;
}
static int forkclassic(Hfs*h,HFork*f,const u8*p,u64 size,u32 id){
 memset(f,0,sizeof *f);f->size=size;f->id=id;f->n=3;f->blocks=(size+h->block-1)/h->block;
 for(unsigned j=0;j<3;j++)f->e[j]=(HExtent){hb2(p+j*4),hb2(p+2+j*4)};
 return size>(u64)h->blocks*h->block||extents_valid(h,f->e,3,NULL)?-1:0;
}
static int forkread(Hfs*,HFork*,u64,void*,size_t,int);
static int recordh(HTree*t,unsigned index,const u8**p,unsigned*n){
 u8*b=t->buffer;unsigned count=hb2(b+10);if(index>=count||count>(t->size-14)/2-1)return -1;
 unsigned at=hb2(b+t->size-2*(index+1)),end=hb2(b+t->size-2*(index+2));
 if(at<14||end<at||end>t->size-2*(count+1))return -1;*p=b+at;*n=end-at;return 0;
}
static int tree_init(Hfs*h,HTree*t,HFork*f,u8*buffer,int overflow){
 u8 b[64];if(f->size<512||forkread(h,f,0,b,sizeof b,overflow)||b[8]!=1||b[9]||hb2(b+10)<3)return -1;
 unsigned size=hb2(b+32);if(size<512||size>32768||(size&(size-1))||f->size<size)return -1;
 t->size=size;t->depth=hb2(b+14);t->root=hb4(b+16);t->first=hb4(b+24);t->last=hb4(b+28);t->nodes=hb4(b+36);t->fork=f;t->buffer=buffer;
 if(!t->nodes||t->nodes>f->size/size||t->depth>32||t->first>=t->nodes||t->last>=t->nodes||t->root>=t->nodes)return -1;
 /* Empty B-trees are allowed, but cannot claim a non-empty leaf chain. */
 if(!t->depth&&(t->first||t->last||t->root||hb4(b+20)))return -1;
 if(forkread(h,f,0,buffer,size,overflow))return -1;const u8*header;unsigned n;
 if(recordh(t,0,&header,&n)||header!=buffer+14||n<106)return -1;return 0;
}
/* All iteration is monotonic through the linked leaf catalogue, bounded by
 * declared nodes and checked backlinks. A corrupt/cyclic chain fails closed. */
static int scan_h(Hfs*h,HTree*t,int overflow,int(*visit)(Hfs*,const u8*,unsigned,void*),void*arg){
 if(!t->depth)return 0;u32 node=t->first,previous=0;
 for(u32 steps=0;steps<t->nodes;steps++){
  if(!node||node>=t->nodes||forkread(h,t->fork,(u64)node*t->size,t->buffer,t->size,overflow)||t->buffer[8]!=255||t->buffer[9]!=1||hb4(t->buffer+4)!=previous)return -1;
  unsigned count=hb2(t->buffer+10);u32 next=hb4(t->buffer);if(!count||count>(t->size-14)/2-1)return -1;
  for(unsigned k=0;k<count;k++){const u8*p;unsigned n;if(recordh(t,k,&p,&n))return -1;int r=visit(h,p,n,arg);if(r)return r;}
  if(!next)return node==t->last?0:-1;previous=node;node=next;
 }return -1;
}
typedef struct {u32 id;u64 block;HExtent e[8];unsigned n;u64 first;int found;u8 type;} Overflow;
static int overflow_record(Hfs*h,const u8*p,unsigned n,void*arg){
 Overflow*x=arg;unsigned key=h->plus?12:8,entries=h->plus?8:3,unit=h->plus?8:4;
 if(n<key+entries*unit)return -1;
 unsigned keylen=h->plus?hb2(p):p[0];if(keylen!=(h->plus?10:7))return -1;
 unsigned type=h->plus?p[2]:p[1];u32 id=h->plus?hb4(p+4):hb4(p+2);u64 start=h->plus?hb4(p+8):hb2(p+6);
 if(type!=x->type||id!=x->id||start>x->block)return 0;
 HExtent e[8]={0};for(unsigned j=0;j<entries;j++)e[j]=h->plus?(HExtent){hb4(p+key+j*8),hb4(p+key+j*8+4)}:(HExtent){hb2(p+key+j*4),hb2(p+key+j*4+2)};
 u64 blocks;if(extents_valid(h,e,entries,&blocks)||!blocks||start+blocks>h->blocks)return -1;
 if(x->block-start>=blocks)return 0;if(x->found)return -1;x->found=1;x->first=start;x->n=entries;memcpy(x->e,e,sizeof e);return 1;
}
static int forkread(Hfs*h,HFork*f,u64 at,void*p,size_t n,int overflow){
 if(at>f->size||n>f->size-at)return -1;
 while(n){u64 block=at/h->block,relative=block;HExtent extent={0};
  for(unsigned j=0;j<f->n;j++){if(relative<f->e[j].count){extent=f->e[j];break;}relative-=f->e[j].count;}
  if(!extent.count){
   /* The extents file can have overflow extents too. Bootstrap through its
    * already addressable leaf prefix; reject genuine dependency cycles, not
    * every file fragmented beyond the volume-header extent records. */
   if(!h->ext.nodes)return -1;int self=overflow&&f->id==3;
   if(self){if(h->bootstrap_depth>=COUNT(h->bootstrap))return -1;for(unsigned j=0;j<h->bootstrap_depth;j++)if(h->bootstrap[j]==block)return -1;h->bootstrap[h->bootstrap_depth++]=block;}
   Overflow x={.id=f->id,.block=block,.type=f->type};HTree tree=h->ext;
   /* A nested bootstrap must not overwrite a partially fetched outer node. */
   if(self){tree.buffer=h->io->alloc(h->io->context,tree.size);if(!tree.buffer){h->bootstrap_depth--;return -1;}}
   int status=scan_h(h,&tree,1,overflow_record,&x);
   if(self){h->io->free(h->io->context,tree.buffer);h->bootstrap_depth--;}if(status<0||!x.found)return -1;relative=block-x.first;
   for(unsigned j=0;j<x.n;j++){if(relative<x.e[j].count){extent=x.e[j];break;}relative-=x.e[j].count;}
   if(!extent.count)return -1;
  }
  unsigned offset=at%h->block;size_t take=MIN((u64)n,(extent.count-relative)*h->block-offset);
  u64 physical=h->alloc+((u64)extent.start+relative)*h->block+offset;
  if(hread(h,physical,p,take))return -1;at+=take;p=(u8*)p+take;n-=take;
 }return 0;
}
static int open_hfs(Hfs*h,ForeignIo*i){
 memset(h,0,sizeof *h);h->io=i;if(!i||!i->alloc||!i->free||i->sectors>~(u64)0/512)return -1;h->bytes=i->sectors*512;u8 b[512];
 if(hread(h,1024,b,512))return -1;u16 signature=hb2(b);
 if(signature==0x4244&&(hb2(b+124)==0x482b||hb2(b+124)==0x4858)){
  u32 block=hb4(b+20);u64 base=(u64)hb2(b+28)*512+(u64)hb2(b+126)*block,bytes=(u64)hb2(b+128)*block;
  if(block<512||block%512||base>h->bytes||bytes>h->bytes-base||bytes<2048)return -1;h->base=base;h->bytes=bytes;
  if(hread(h,1024,b,512))return -1;signature=hb2(b);
 }
 if(signature==0x482b||signature==0x4858){
  h->plus=1;h->sensitive=signature==0x4858;
  if(hb2(b+2)!=(h->sensitive?5:4)||!(hb4(b+4)&(1u<<8))||(hb4(b+4)&(1u<<11)))return -1;
  h->block=hb4(b+40);h->blocks=hb4(b+44);
  if(h->block<512||(h->block&(h->block-1))||h->block>1024*1024||!h->blocks||(u64)h->blocks*h->block>h->bytes)return -1;
  if(forkplus(h,&h->extents,b+192,3)||forkplus(h,&h->catalogue,b+272,4))return -1;
 }else if(signature==0x4244){
  if(!(hb2(b+10)&(1u<<8))||(hb2(b+10)&(1u<<11)))return -1;
  h->block=hb4(b+20);h->blocks=hb2(b+18);h->alloc=(u64)hb2(b+28)*512;
  if(h->block<512||h->block%512||!h->blocks||h->alloc>h->bytes||(u64)h->blocks*h->block>h->bytes-h->alloc)return -1;
  if(forkclassic(h,&h->extents,b+134,hb4(b+130),3)||forkclassic(h,&h->catalogue,b+150,hb4(b+146),4))return -1;
 }else return -1;
 h->nodes=i->alloc(i->context,65536);if(!h->nodes)return -1;
 if(tree_init(h,&h->ext,&h->extents,h->nodes+32768,1)||tree_init(h,&h->cat,&h->catalogue,h->nodes,0)){i->free(i->context,h->nodes);h->nodes=NULL;return -1;}
 /* HFS+ binary-comparison catalogue attribute (HFSX). We scan leaves but
  * preserve exact UTF8 spelling, using ASCII folding only on HFS/HFS+. */
 if(h->plus){unsigned comparison=h->cat.buffer[14+37];
  if(comparison!=0xbc&&comparison!=0xcf){i->free(i->context,h->nodes);h->nodes=NULL;return -1;}
  h->sensitive=comparison==0xbc;
 }
 return 0;
}
static void close_hfs(Hfs*h){if(h->nodes)h->io->free(h->io->context,h->nodes);}
static int utf8_h(char*out,unsigned*used,u32 c){unsigned n=c<128?1:c<2048?2:c<65536?3:4;if(!c||c=='/'||c==':'||c>0x10ffff||(c>=0xd800&&c<=0xdfff)||*used+n>=1021)return -1;
 if(n==1)out[(*used)++]=c;else{out[(*used)++]=(n==2?0xc0:n==3?0xe0:0xf0)|(c>>(6*(n-1)));for(int j=n-2;j>=0;j--)out[(*used)++]=0x80|((c>>(6*j))&63);}return 0;
}
static const u16 macroman_h[128]={
 0x00c4,0x00c5,0x00c7,0x00c9,0x00d1,0x00d6,0x00dc,0x00e1,0x00e0,0x00e2,0x00e4,0x00e3,0x00e5,0x00e7,0x00e9,0x00e8,
 0x00ea,0x00eb,0x00ed,0x00ec,0x00ee,0x00ef,0x00f1,0x00f3,0x00f2,0x00f4,0x00f6,0x00f5,0x00fa,0x00f9,0x00fb,0x00fc,
 0x2020,0x00b0,0x00a2,0x00a3,0x00a7,0x2022,0x00b6,0x00df,0x00ae,0x00a9,0x2122,0x00b4,0x00a8,0x2260,0x00c6,0x00d8,
 0x221e,0x00b1,0x2264,0x2265,0x00a5,0x00b5,0x2202,0x2211,0x220f,0x03c0,0x222b,0x00aa,0x00ba,0x03a9,0x00e6,0x00f8,
 0x00bf,0x00a1,0x00ac,0x221a,0x0192,0x2248,0x2206,0x00ab,0x00bb,0x2026,0x00a0,0x00c0,0x00c3,0x00d5,0x0152,0x0153,
 0x2013,0x2014,0x201c,0x201d,0x2018,0x2019,0x00f7,0x25ca,0x00ff,0x0178,0x2044,0x20ac,0x2039,0x203a,0xfb01,0xfb02,
 0x2021,0x00b7,0x201a,0x201e,0x2030,0x00c2,0x00ca,0x00c1,0x00cb,0x00c8,0x00cd,0x00ce,0x00cf,0x00cc,0x00d3,0x00d4,
 0xf8ff,0x00d2,0x00da,0x00db,0x00d9,0x0131,0x02c6,0x02dc,0x00af,0x02d8,0x02d9,0x02da,0x00b8,0x02dd,0x02db,0x02c7};
static int catalogue_record(Hfs*h,const u8*p,unsigned n,u32*parent,HFile*f){
 unsigned key=h->plus?(n>=2?hb2(p)+2:0):(n?p[0]+1:0);key=(key+1)&~1u;if(!key||key>n)return -1;
 memset(f,0,sizeof *f);unsigned used=0;
 if(h->plus){if(key<8)return -1;*parent=hb4(p+2);unsigned chars=hb2(p+6);if(chars>255||8+chars*2>key)return -1;
  for(unsigned j=0;j<chars;j++){u32 c=hb2(p+8+j*2);if(c>=0xd800&&c<=0xdbff){if(++j>=chars)return -1;u32 d=hb2(p+8+j*2);if(d<0xdc00||d>0xdfff)return -1;c=0x10000+((c-0xd800)<<10)+(d-0xdc00);}if(utf8_h(f->name,&used,c))return -1;}
 }else{if(key<8)return -1;*parent=hb4(p+2);unsigned chars=p[6];if(chars>31||7+chars>key)return -1;
  for(unsigned j=0;j<chars;j++){unsigned c=p[7+j];if(utf8_h(f->name,&used,c<128?c:macroman_h[c-128]))return -1;}
 }
 f->name[used]=0;p+=key;n-=key;unsigned type=h->plus?(n>=2?hb2(p):0):(n?p[0]:0);
 if(type==3||type==4)return 1; /* reverse/thread record, not a child */
 if(!used)return -1;
 if(type==1){if(n<(h->plus?88:70))return -1;f->dir=1;f->id=h->plus?hb4(p+8):hb4(p+6);}
 else if(type==2){
  if(n<(h->plus?248:102))return -1;f->id=h->plus?hb4(p+8):hb4(p+20);
  if(h->plus){if(forkplus(h,&f->data,p+88,f->id)||forkplus(h,&f->resource,p+168,f->id))return -1;f->resource.type=255;unsigned mode=hb2(p+42)&0170000;
   f->special=(mode&&mode!=0100000)||(p[41]&0x20)||!memcmp(p+48,"hlnkhfs+",8)||!memcmp(p+48,"fdrpMACS",8);
  }else{if(forkclassic(h,&f->data,p+74,hb4(p+26),f->id)||forkclassic(h,&f->resource,p+86,hb4(p+36),f->id))return -1;f->resource.type=255;}
  f->size=f->data.size;
 }else return -1;
 return f->id<2?-1:0;
}
static int same_h(Hfs*h,const char*a,const char*b){while(*a&&*b){unsigned x=(u8)*a++,y=(u8)*b++;if(!h->sensitive){if(x>='A'&&x<='Z')x+=32;if(y>='A'&&y<='Z')y+=32;}if(x!=y)return 0;}return !*a&&!*b;}
typedef struct {u32 parent;const char*name;HFile*file;ForeignEntry*out;int cap,skip,count,found;} HQuery;
static int visit_h(Hfs*h,const u8*p,unsigned n,void*arg){
 HQuery*q=arg;HFile f;u32 parent;int r=catalogue_record(h,p,n,&parent,&f);if(r<0)return -1;if(r||parent!=q->parent)return 0;
 if(q->name){if(!same_h(h,q->name,f.name))return 0;if(q->found)return -1;*q->file=f;q->found=1;return 0;}
 if(q->skip){q->skip--;return 0;}if(q->count>=q->cap)return 1;
 ForeignEntry*e=q->out+q->count++;*e=(ForeignEntry){.dir=f.dir,.size=f.size,.ro=1};strset(e->name,f.name,sizeof e->name);return q->count==q->cap?1:0;
}
static int root_catalogue_h(Hfs*h,const u8*p,unsigned n,void*arg){
 int*found=arg;u32 parent;HFile f;int status=catalogue_record(h,p,n,&parent,&f);if(status<0)return -1;
 if(!status&&f.id==2){if(!f.dir||parent!=1)return -1;*found=1;return 1;}return 0;
}
static int resolve_h(Hfs*h,const char*path,HFile*f){
 if(!path||*path!='/')return -1;int root=0;if(scan_h(h,&h->cat,0,root_catalogue_h,&root)<0||!root)return -1;*f=(HFile){.dir=1,.id=2};unsigned depth=0;
 while(*path){while(*path=='/')path++;if(!*path)break;
  if(!f->dir&&eq(path,"..namedfork/rsrc")){f->data=f->resource;f->size=f->resource.size;return 0;}
  if(!f->dir||++depth>64)return -1;char name[1021];unsigned n=0;
  while(*path&&*path!='/'){if(n>=sizeof name-1)return -1;name[n++]=*path++;}name[n]=0;if(eq(name,".")||eq(name,".."))return -1;
  HQuery q={.parent=f->id,.name=name,.file=f};if(scan_h(h,&h->cat,0,visit_h,&q)||!q.found)return -1;
 }return 0;
}
int hfs_volume_stat(ForeignIo*i,const char*path,ForeignEntry*out){
 if(!out)return -1;Hfs h;if(open_hfs(&h,i))return -1;HFile f;int r=resolve_h(&h,path,&f);
 if(!r){*out=(ForeignEntry){.size=f.size,.dir=f.dir,.ro=1};strset(out->name,f.name,sizeof out->name);}close_hfs(&h);return r;
}
int hfs_volume_list(ForeignIo*i,const char*path,ForeignEntry*out,int cap,int skip){
 if(cap<0||skip<0||(!out&&cap))return -1;Hfs h;if(open_hfs(&h,i))return -1;HFile f;int r=-1;
 if(!resolve_h(&h,path,&f)&&f.dir){HQuery q={.parent=f.id,.out=out,.cap=cap,.skip=skip};int status=cap?scan_h(&h,&h.cat,0,visit_h,&q):0;r=status<0?-1:q.count;}close_hfs(&h);return r;
}
i64 hfs_volume_read(ForeignIo*i,const char*path,u64 off,void*p,size_t n){
 if(n&&!p)return -1;Hfs h;if(open_hfs(&h,i))return -1;HFile f;i64 r=-1;
 if(!resolve_h(&h,path,&f)&&!f.dir&&!f.special){if(off>=f.size)r=0;else{n=MIN((u64)n,f.size-off);r=forkread(&h,&f.data,off,p,n,0)?-1:(i64)n;}}close_hfs(&h);return r;
}
