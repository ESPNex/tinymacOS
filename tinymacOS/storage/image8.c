/* SPDX-License-Identifier: MIT
 * Read-only 64-bit RAW/IMG/ISO view and UDIF v4 multi-blkx mapper.
 * XML and base64 are streamed using the caller's bounded scratch space.
 * No XML entity evaluation, unbounded allocation, or whole-file copies.
 */
#include "image8.h"
static u32 be4i(const u8*p){return (u32)p[0]<<24|(u32)p[1]<<16|(u32)p[2]<<8|p[3];}
static u64 be8i(const u8*p){return (u64)be4i(p)<<32|be4i(p+4);}
static int rangei(u64 a,u64 n,u64 limit){return a<=limit&&n<=limit-a;}
static int srci(Image8*i,u64 at,void*p,size_t n){return !rangei(at,n,i->file_bytes)?-1:i->source(i->context,at,p,n);}
/* Paged radix table: O(log512 runs) access, page-sized allocations only.
 * Memory exhaustion returns an error; disk size/run/XML caps aren't used as
 * substitutes for allocation and checked 64-bit arithmetic. */
#define RUNS_PAGE (4096u/sizeof(Image8Run))
static void*newpage8(Image8*i){void*p=i->alloc?i->alloc(i->context,4096):NULL;if(p)memset(p,0,4096);return p;}
static Image8Run*getrun8(Image8*i,u64 index,int create){
 if(index<IMAGE8_INLINE_RUNS)return i->run+index;
 index-=IMAGE8_INLINE_RUNS;u64 leaf=index/RUNS_PAGE;unsigned slot=index%RUNS_PAGE;
 if(!i->run_pages){if(!create||!i->alloc||!i->free)return NULL;i->run_pages=newpage8(i);if(!i->run_pages)return NULL;i->run_capacity=1;}
 while(leaf>=i->run_capacity){
  if(!create||i->run_levels>=7||i->run_capacity>~(u64)0/512)return NULL;
  void**p=newpage8(i);if(!p)return NULL;p[0]=i->run_pages;i->run_pages=p;i->run_levels++;i->run_capacity*=512;
 }
 void*p=i->run_pages;for(unsigned level=i->run_levels;level;level--){unsigned digit=(leaf>>(9*(level-1)))&511;void**dir=p;
  if(!dir[digit]){if(!create)return NULL;dir[digit]=newpage8(i);if(!dir[digit])return NULL;}p=dir[digit];
 }return (Image8Run*)p+slot;
}
Image8Run*image8_run(Image8*i,u64 index){return !i||index>=i->runs?NULL:getrun8(i,index,0);}
static void freepages8(Image8*i,void*p,unsigned level){if(!p)return;if(level){void**dir=p;for(unsigned j=0;j<512;j++)if(dir[j])freepages8(i,dir[j],level-1);}i->free(i->context,p);}
void image8_release(Image8*i){if(!i)return;if(i->run_pages&&i->free)freepages8(i,i->run_pages,i->run_levels);i->run_pages=NULL;i->run_capacity=i->runs=i->bytes=0;i->run_levels=0;}
/* A bounded XML reader. -1 EOF, -2 source error. */
typedef struct {Image8*i;u8*buf;u64 next,end;unsigned at,n;} Xml8;
static int get8(Xml8*x){if(x->at==x->n){if(x->next==x->end)return -1;x->n=MIN((u64)4096,x->end-x->next);x->at=0;if(srci(x->i,x->next,x->buf,x->n))return -2;x->next+=x->n;}return x->buf[x->at++];}
static int space8(int c){return c==' '||c=='\n'||c=='\r'||c=='\t';}
static int tag8(Xml8*x,char*t,unsigned cap){unsigned n=0;int c;while((c=get8(x))>=0&&c!='>'){if(n+1>=cap||c=='<'||c=='&'||!c)return -1;t[n++]=c;}if(c!='>')return -1;t[n]=0;return 0;}
static int text8(Xml8*x,char*t,unsigned cap){unsigned n=0;int c;while((c=get8(x))>=0&&c!='<'){if(n+1>=cap||c=='&'||!c)return -1;t[n++]=c;}if(c!='<')return -1;t[n]=0;return 0;}
static int b64i(int c){if(c>='A'&&c<='Z')return c-'A';if(c>='a'&&c<='z')return c-'a'+26;if(c>='0'&&c<='9')return c-'0'+52;return c=='+'?62:c=='/'?63:-1;}
typedef struct {Xml8*x;unsigned bits,value,pad,chars;int ended;} Base8;
/* 0 byte, 1 end, -1 malformed. Padding is canonical, no trailing garbage. */
static int byte8(Base8*b,u8*out){
 if(b->ended)return 1;
 for(;;){int c=get8(b->x);if(c<0)return -1;if(space8(c))continue;
  if(c=='<'){char tag[24];if(tag8(b->x,tag,sizeof tag)||!eq(tag,"/data")||(b->chars%4)||b->bits>4||(b->bits&&(b->value&((1u<<b->bits)-1)))||b->pad!=b->bits/2)return -1;b->ended=1;return 1;}
  b->chars++;if(c=='='){if(++b->pad>2)return -1;continue;}if(b->pad)return -1;int v=b64i(c);if(v<0)return -1;
  b->value=(b->value<<6)|v;b->bits+=6;if(b->bits>=8){b->bits-=8;*out=b->value>>b->bits;return 0;}
 }
}
typedef struct {Image8*i;Base8*base;u64 at,end;} Input8;
static int input8(Input8*b,void*out,unsigned n){
 if(b->base){u8*p=out;while(n--)if(byte8(b->base,p++))return -1;return 0;}
 if(!rangei(b->at,n,b->end)||srci(b->i,b->at,out,n))return -1;b->at+=n;return 0;
}
static int input_end8(Input8*b){u8 extra;return b->base?byte8(b->base,&extra)==1:b->at==b->end;}

static int table8(Image8*i,Input8*b,u64 fork,u64 forklen,u64 sectors){
 u8 h[204],r[40];
 if(input8(b,h,sizeof h)||memcmp(h,"mish",4)||be4i(h+4)!=1)return -1;
 u64 first=be8i(h+8),total=be8i(h+16),data=be8i(h+24);unsigned nr=be4i(h+200);
 if(!total||!rangei(first,total,sectors)||data>forklen||!nr)return -1;
 u64 next=0;int terminated=0;
 for(unsigned k=0;k<nr;k++){
  if(input8(b,r,sizeof r))return -1;u32 type=be4i(r);u64 at=be8i(r+8),n=be8i(r+16),off=be8i(r+24),len=be8i(r+32);
  /* Reserved field is a comment value in some producers, not a pointer. */
  if(type==0x7ffffffeu){if(n||len)return -1;continue;}
  if(type==0xffffffffu){if(k+1!=nr||n||len||(at&&at!=total))return -1;terminated=1;break;}
  if(!n||at!=next||!rangei(at,n,total))return -1;
  if(type==IMAGE8_ZERO||type==IMAGE8_IGNORE){if(len)return -1;}
  else{
   if(type!=IMAGE8_RAW&&(!i->codec||type<IMAGE8_ADC||type>IMAGE8_LZMA))return -1;
   if(!len||!rangei(off,len,forklen-data)||(type==IMAGE8_RAW&&len!=n*512))return -1;
  }
  Image8Run*entry=getrun8(i,i->runs,1);if(!entry)return -1;*entry=(Image8Run){first+at,n,fork+data+off,len,type};i->runs++;next+=n;
 }
 if(!terminated||next!=total||!input_end8(b))return -1;return 0;
}
/* Binary plists use positional object references. Follow only the UDIF
 * schema, not arbitrary object graphs: cyclic unrelated objects are never
 * recursively evaluated. Offset/ref widths and all regions are checked. */
typedef struct {Image8*i;u64 base,size,table,objects,top;unsigned offsets,refs;} Plist8;
typedef struct {unsigned kind;u64 at,count;} Object8;
static int uint8p(Image8*i,u64 at,unsigned n,u64*out){
 u8 b[8];if(!n||n>8||srci(i,at,b,n))return -1;u64 value=0;for(unsigned j=0;j<n;j++)value=value<<8|b[j];*out=value;return 0;
}
static int object8p(Plist8*p,u64 id,Object8*out){
 u64 off;if(id>=p->objects||uint8p(p->i,p->base+p->table+id*p->offsets,p->offsets,&off)||off<8||off>=p->table)return -1;
 u8 marker;if(srci(p->i,p->base+off++,&marker,1))return -1;unsigned kind=marker>>4;u64 count=marker&15;
 if(count==15){u8 integer;if(off>=p->table||srci(p->i,p->base+off++,&integer,1)||(integer>>4)!=1||(integer&15)>3)return -1;unsigned n=1u<<(integer&15);
  if(!rangei(off,n,p->table)||uint8p(p->i,p->base+off,n,&count))return -1;off+=n;
 }
 unsigned unit=kind==6?2:kind==10?p->refs:kind==13?2*p->refs:1;
 if(kind!=4&&kind!=5&&kind!=6&&kind!=10&&kind!=13)return -1;
 if(count>(p->table-off)/unit)return -1;*out=(Object8){kind,p->base+off,count};return 0;
}
static int ref8p(Plist8*p,const Object8*o,u64 j,u64*out){
 u64 count=o->count*(o->kind==13?2:1);return j>=count?-1:uint8p(p->i,o->at+j*p->refs,p->refs,out);
}
static int key8p(Plist8*p,u64 id,const char*key){
 Object8 o;if(object8p(p,id,&o))return -1;size_t n=strlen(key);if((o.kind!=5&&o.kind!=6)||o.count!=n)return 0;
 for(unsigned j=0;j<n;j++){u64 c;if(uint8p(p->i,o.at+j*(o.kind==6?2:1),o.kind==6?2:1,&c))return -1;if(c!=(u8)key[j])return 0;}return 1;
}
/* 0 found, 1 absent, -1 malformed. Duplicate schema keys are rejected. */
static int dict8p(Plist8*p,u64 id,const char*key,u64*out){
 Object8 o;if(object8p(p,id,&o)||o.kind!=13)return -1;int found=0;
 for(u64 j=0;j<o.count;j++){u64 k;if(ref8p(p,&o,j,&k))return -1;int match=key8p(p,k,key);if(match<0)return -1;if(match){if(found++||ref8p(p,&o,o.count+j,out))return -1;}}
 return found?0:1;
}
static int binary8(Image8*i,u64 base,u64 size,u64 fork,u64 forklen,u64 sectors){
 if(size<40)return -1;u8 trailer[32];if(srci(i,base+size-32,trailer,32))return -1;
 Plist8 p={.i=i,.base=base,.size=size,.offsets=trailer[6],.refs=trailer[7],.objects=be8i(trailer+8),.top=be8i(trailer+16),.table=be8i(trailer+24)};
 if(!p.offsets||p.offsets>8||!p.refs||p.refs>8||!p.objects||p.top>=p.objects||p.table<8||p.table>size-32||p.objects>(size-32-p.table)/p.offsets)return -1;
 u64 resource,blkx;int status=dict8p(&p,p.top,"resource-fork",&resource);if(status<0)return -1;
 if(dict8p(&p,status?p.top:resource,"blkx",&blkx))return -1;Object8 array;
 if(object8p(&p,blkx,&array)||array.kind!=10||!array.count)return -1;
 for(u64 j=0;j<array.count;j++){u64 item,data;if(ref8p(&p,&array,j,&item)||dict8p(&p,item,"Data",&data))return -1;Object8 o;
  if(object8p(&p,data,&o)||o.kind!=4)return -1;Input8 input={.i=i,.at=o.at,.end=o.at+o.count};if(table8(i,&input,fork,forklen,sectors))return -1;
 }return 0;
}
/* Classic resource fork: the map contains 24-bit resource-data offsets,
 * each pointing to a length-prefixed binary mish payload. No XML required. */
static int resource8(Image8*i,u64 base,u64 size,u64 fork,u64 forklen,u64 sectors){
 u8 h[28];if(size<16||srci(i,base,h,16))return -1;
 u64 data=be4i(h),map=be4i(h+4),datalen=be4i(h+8),maplen=be4i(h+12);
 if(!rangei(data,datalen,size)||!rangei(map,maplen,size)||maplen<28||srci(i,base+map,h,28))return -1;
 u64 types=(u64)h[24]*256+h[25];if(!rangei(types,2,maplen)||srci(i,base+map+types,h,2))return -1;
 unsigned count=((unsigned)h[0]<<8|h[1])+1;int found=0;
 if(count>(maplen-types-2)/8)return -1;
 for(unsigned j=0;j<count;j++){u8 t[8];if(srci(i,base+map+types+2+(u64)j*8,t,8))return -1;if(memcmp(t,"blkx",4))continue;
  unsigned resources=((unsigned)t[4]<<8|t[5])+1;u64 refs=types+((unsigned)t[6]<<8|t[7]);
  if(!rangei(refs,(u64)resources*12,maplen))return -1;
  for(unsigned k=0;k<resources;k++){u8 r[12],len[4];if(srci(i,base+map+refs+(u64)k*12,r,12))return -1;
   u64 off=(u32)r[5]<<16|(u32)r[6]<<8|r[7];if(!rangei(off,4,datalen)||srci(i,base+data+off,len,4))return -1;
   u64 n=be4i(len);if(!rangei(off+4,n,datalen))return -1;Input8 input={.i=i,.at=base+data+off+4,.end=base+data+off+4+n};
   if(table8(i,&input,fork,forklen,sectors))return -1;found=1;
  }
 }return found?0:-1;
}
static void swap8(Image8Run*a,Image8Run*b){Image8Run t=*a;*a=*b;*b=t;}
static void down8(Image8*i,u64 root,u64 n){for(;;){if(root>(n-1)/2)return;u64 c=root*2+1;if(c>=n)return;if(c+1<n&&image8_run(i,c)->sector<image8_run(i,c+1)->sector)c++;if(image8_run(i,root)->sector>=image8_run(i,c)->sector)return;swap8(image8_run(i,root),image8_run(i,c));root=c;}}
static void sort8(Image8*i){for(u64 k=i->runs/2;k;k--)down8(i,k-1,i->runs);for(u64 n=i->runs;n>1;n--){swap8(image8_run(i,0),image8_run(i,n-1));down8(i,0,n-1);}}
int image8_read(void*ctx,u64 at,void*p,size_t n){
 Image8*i=ctx;if(!i||!p||!rangei(at,n,i->bytes))return -1;if(!i->runs)return srci(i,at,p,n);
 while(n){
  /* Upper bound lookup: metadata sorted and disjoint at attach time. */
  u64 lo=0,hi=i->runs;while(lo<hi){u64 m=lo+(hi-lo)/2;if(image8_run(i,m)->sector<=at/512)lo=m+1;else hi=m;}
  Image8Run*r=lo?image8_run(i,lo-1):NULL;
  if(!r||at-r->sector*512>=r->count*512){
   u64 end=lo<i->runs?image8_run(i,lo)->sector*512:i->bytes;size_t take=MIN((u64)n,end-at);if(!take)return -1;memset(p,0,take);at+=take;p=(u8*)p+take;n-=take;continue;
  }
  u64 off=at-r->sector*512;size_t take=MIN((u64)n,r->count*512-off);int error=0;
  if(r->type==IMAGE8_RAW)error=srci(i,r->offset+off,p,take);
  else if(r->type==IMAGE8_ZERO||r->type==IMAGE8_IGNORE)memset(p,0,take);
  else error=!i->codec?-1:i->codec(i,r,off,p,take);
  if(error)return -1;at+=take;p=(u8*)p+take;n-=take;
 }return 0;
}
int image8_decode_alloc(Image8*i,void*context,u64 bytes,int(*read)(void*,u64,void*,size_t),u8*scratch,size_t scratch_bytes,int(*codec)(Image8*,const Image8Run*,u64,void*,size_t),void*(*alloc)(void*,size_t),void(*free)(void*,void*)){
 if(!i||!read||!scratch||scratch_bytes<4096||bytes<512||bytes>IMAGE8_ADDRESS_LIMIT)return -1;
 memset(i,0,sizeof *i);i->context=context;i->source=read;i->codec=codec;i->alloc=alloc;i->free=free;i->file_bytes=bytes;
 u8 footer[512];if(srci(i,bytes-512,footer,512))return -1;
 if(memcmp(footer,"koly",4)){if(bytes%512)return -1;i->bytes=bytes;return 0;}
 /* Single-segment UDIF v4; encryption, NDIF and segmented containers are
  * explicitly unsupported, never interpreted as a partially decoded disk. */
 if(be4i(footer+4)!=4||be4i(footer+8)!=512||be8i(footer+16)||be4i(footer+56)!=1||be4i(footer+60)!=1)return -1;
 u64 fork=be8i(footer+24),forklen=be8i(footer+32),xml=be8i(footer+216),xmlsize=be8i(footer+224),sectors=be8i(footer+492);
 if(!sectors||sectors>IMAGE8_ADDRESS_LIMIT/512||!rangei(fork,forklen,bytes-512))return -1;
 if(!xmlsize){u64 resource=be8i(footer+40),size=be8i(footer+48);
  if(!size||!rangei(resource,size,bytes-512)||resource8(i,resource,size,fork,forklen,sectors))return -1;goto mapped;
 }
 if(!rangei(xml,xmlsize,bytes-512)||xml<fork+forklen)return -1;
 u8 magic[8];if(xmlsize<8||srci(i,xml,magic,8))return -1;
 if(!memcmp(magic,"bplist00",8)){if(binary8(i,xml,xmlsize,fork,forklen,sectors))return -1;goto mapped;}
 Xml8 x={.i=i,.buf=scratch,.next=xml,.end=xml+xmlsize};
 char tag[128],text[128];int c,found=0,wait_array=0,depth=0,tables=0;
 while((c=get8(&x))>=0){
  if(c!='<'){if(!c||c=='&')return -1;continue;}
  if(tag8(&x,tag,sizeof tag))return -1;
  /* Only a non-internal DOCTYPE is tolerated. Never entities/subsets. */
  if(tag[0]=='!'){if(strchr(tag,'[')||!strchr(tag,'D')||memcmp(tag,"!DOCTYPE ",9))return -1;continue;}
  if(eq(tag,"key")){
   if(text8(&x,text,sizeof text)||tag8(&x,tag,sizeof tag)||!eq(tag,"/key"))return -1;
   if(eq(text,"blkx")){if(found||depth)return -1;found=1;wait_array=1;}continue;
  }
  if(wait_array){if(!eq(tag,"array"))return -1;wait_array=0;depth=1;continue;}
  if(depth){
   if(eq(tag,"array")){depth++;if(depth>16)return -1;}
   else if(eq(tag,"/array"))depth--;
   else if(eq(tag,"data")){Base8 base={.x=&x};Input8 input={.base=&base};if(table8(i,&input,fork,forklen,sectors))return -1;tables++;}
  }
 }
 if(c==-2||!found||wait_array||depth||!tables||!i->runs)return -1;
mapped:
 if(!i->runs)return -1;sort8(i);u64 end=0;for(u64 k=0;k<i->runs;k++){Image8Run*r=image8_run(i,k);if(r->sector<end)return -1;end=r->sector+r->count;}
 i->bytes=sectors*512;return 0;
}
int image8_decode(Image8*i,void*c,u64 bytes,int(*r)(void*,u64,void*,size_t),u8*s,size_t n){return image8_decode_codecs(i,c,bytes,r,s,n,NULL);}

int image8_decode_codecs(Image8*i,void*c,u64 bytes,int(*r)(void*,u64,void*,size_t),u8*s,size_t n,int(*codec)(Image8*,const Image8Run*,u64,void*,size_t)){return image8_decode_alloc(i,c,bytes,r,s,n,codec,NULL,NULL);}
