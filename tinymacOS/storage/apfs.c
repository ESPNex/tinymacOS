/* SPDX-License-Identifier: MIT
 * Original native read-only APFS v2 reader. Numerical layout follows Apple's
 * Apple File System Reference. No Apple binaries, FUSE, libc or host syscalls.
 * Container root exposes real volume labels with stable "Volume N" aliases
 * (one per occupied NX slot), never silently just the first filesystem.
 * Supports normal unencrypted, unsealed volumes, object maps, directory/inode
 * records, regular data forks, clone stream IDs and explicit sparse extents.
 * Rejects Fusion, checkpoint-descriptor trees, encrypted/rolled/sealed
 * volumes, snapshots, firmlinks, xattr-compressed and special-file reads.
 * Case/normalization matching is exact UTF8 with ASCII case folding only.
 */
#include "foreign_volume.h"
#include "abi.h"
#define APFS_VOLUMES 100
#define APFS_DEPTH 32
#define APFS_ID_MASK 0x0fffffffffffffffull
static u16 al2(const u8*p){return p[0]|(u16)p[1]<<8;}
static u32 al4(const u8*p){return al2(p)|(u32)al2(p+2)<<16;}
static u64 al8(const u8*p){return al4(p)|(u64)al4(p+4)<<32;}
typedef struct {const u8*k,*v;unsigned kn,vn;} ARecord;
typedef struct {u8*b;unsigned flags,level,n,toc,keybase,valbase,keysize,valsize;} ANode;
typedef struct {ForeignIo*io;u32 block;u64 blocks,xid,container_omap,volume_omap,root;u64 fs_oid[APFS_VOLUMES];u64 features;int physical;} Apfs;
typedef struct {u64 id,stream,size;unsigned mode,bsd;int dir,special;} AFile;
static int arange(u64 at,u64 n,u64 limit){return at<=limit&&n<=limit-at;}
static int abytes(Apfs*a,u64 at,void*p,size_t n){
 u64 limit=a->blocks*a->block;if(!arange(at,n,limit))return -1;
 while(n){unsigned part=at%512,take=MIN((u64)n,512-part);u8 b[512];
  if(!part&&n>=512){unsigned blocks=MIN((u64)128,n/512);take=blocks*512;if(a->io->read(a->io->context,at/512,blocks,p))return -1;}
  else{if(a->io->read(a->io->context,at/512,1,b))return -1;memcpy(p,b+part,take);}
  at+=take;p=(u8*)p+take;n-=take;
 }return 0;
}
static int checksum_a(const u8*b,unsigned n){
 const u64 mod=0xffffffffull;u64 s1=0,s2=0;
 for(unsigned j=8;j<n;j+=4){s1=(s1+al4(b+j))%mod;s2=(s2+s1)%mod;}
 u32 lo=mod-(s1+s2)%mod,hi=mod-(s1+lo)%mod;return al4(b)==lo&&al4(b+4)==hi?0:-1;
}
static int object_a(Apfs*a,u64 physical,u8*b,unsigned type){
 if(!physical||physical>=a->blocks||abytes(a,physical*a->block,b,a->block)||checksum_a(b,a->block))return -1;
 u32 kind=al4(b+24);if((kind&0x38000000u)||(type&&(kind&65535)!=type)||!al8(b+16)||al8(b+16)>a->xid)return -1;return 0;
}
/* Decodes a TOC item using checked offsets into the containing node. */
static int record_a(const ANode*n,unsigned j,ARecord*r){
 if(j>=n->n)return -1;const u8*p=n->b+n->toc+j*((n->flags&4)?4:8);
 unsigned ko=al2(p),vo,kn,vn;
 if(n->flags&4){vo=al2(p+2);kn=n->keysize;vn=(n->flags&2)?n->valsize:8;}
 else{kn=al2(p+2);vo=al2(p+4);vn=al2(p+6);}
 if(!kn||!vn||vo==65535||vo>n->valbase||vn>vo||ko>n->valbase-n->keybase||kn>n->valbase-n->keybase-ko)return -1;
 unsigned key=n->keybase+ko,val=n->valbase-vo;if(key+kn>val)return -1;
 *r=(ARecord){n->b+key,n->b+val,kn,vn};return 0;
}
static int node_a(Apfs*a,u64 physical,u8*b,ANode*n,unsigned keysize,unsigned valsize,int root,int expected){
 if(object_a(a,physical,b,0))return -1;unsigned type=al4(b+24)&65535;
 if(type!=2&&type!=3)return -1;
 *n=(ANode){.b=b,.flags=al2(b+32),.level=al2(b+34),.n=al4(b+36),.toc=56+al2(b+40),.keysize=keysize,.valsize=valsize};
 if((n->flags&~7u)||!!(n->flags&1)!=!!root||n->level>=APFS_DEPTH||(expected>=0&&n->level!=(unsigned)expected)||!!(n->flags&2)!=(n->level==0))return -1;
 unsigned tocbytes=al2(b+42),stride=(n->flags&4)?4:8;n->keybase=n->toc+tocbytes;n->valbase=a->block-((n->flags&1)?40:0);
 if(n->toc>n->valbase||tocbytes>n->valbase-n->toc||n->n>tocbytes/stride||!n->n)return -1;
 unsigned maxkey=n->keybase,minval=n->valbase;
 for(unsigned j=0;j<n->n;j++){ARecord r;if(record_a(n,j,&r))return -1;maxkey=MAX(maxkey,(unsigned)(r.k-b)+r.kn);minval=MIN(minval,(unsigned)(r.v-b));}
 if(maxkey>minval)return -1;return 0;
}
static int omap_tree_a(Apfs*a,u64 object,u64*root){
 u8*b=a->io->alloc(a->io->context,a->block);if(!b)return -1;int result=-1;
 if(!object_a(a,object,b,11)&&!(al4(b+32)&~1u)&&(al4(b+40)&0xc0000000u)==0x40000000u){*root=al8(b+48);if(*root&&*root<a->blocks)result=0;}
 a->io->free(a->io->context,b);return result;
}
/* Physical fixed-key OMAP B-tree lookup for greatest (OID, XID)<=target.
 * Reject deletion/encryption/noheader, and require one complete block. */
static int map_a(Apfs*a,u64 root,u64 oid,u64*physical){
 u8*b=a->io->alloc(a->io->context,a->block);if(!b)return -1;int result=-1,expected=-1;u64 seen[APFS_DEPTH];
 for(unsigned depth=0;depth<APFS_DEPTH;depth++){
  for(unsigned k=0;k<depth;k++)if(seen[k]==root)goto done;seen[depth]=root;ANode n;
  if(node_a(a,root,b,&n,16,16,depth==0,expected)||!(n.flags&4))goto done;
  if(!depth){const u8*f=b+a->block-40;if(al4(f+4)!=a->block||al4(f+8)!=16||al4(f+12)!=16||!(al4(f)&16))goto done;}
  int selected=-1;u64 lastid=0,lastxid=0;
  for(unsigned j=0;j<n.n;j++){
   ARecord r;if(record_a(&n,j,&r)||r.kn!=16)goto done;u64 id=al8(r.k),xid=al8(r.k+8);
   if(j&&(id<lastid||(id==lastid&&xid<=lastxid)))goto done;lastid=id;lastxid=xid;
   if(id<oid||(id==oid&&xid<=a->xid))selected=j;
  }
  if(selected<0)goto done;ARecord r;if(record_a(&n,selected,&r))goto done;
  if(n.flags&2){
   if(al8(r.k)!=oid||r.vn!=16||(al4(r.v)&~2u)||al4(r.v+4)!=a->block)goto done;
   *physical=al8(r.v+8);if(*physical&&*physical<a->blocks)result=0;break;
  }
  if(r.vn!=8)goto done;root=al8(r.v);expected=n.level-1;
 }
done:a->io->free(a->io->context,b);return result;
}
static int container_a(Apfs*a,ForeignIo*i){
 memset(a,0,sizeof *a);if(!i||!i->read||!i->alloc||!i->free||i->sectors>~(u64)0/512)return -1;
 u8 first[512];if(i->sectors<8||i->read(i->context,0,1,first)||memcmp(first+32,"NXSB",4))return -1;
 a->io=i;a->block=al4(first+36);a->blocks=al8(first+40);a->xid=~(u64)0;
 if(a->block<4096||a->block>65536||(a->block&(a->block-1))||!a->blocks||a->blocks>i->sectors/(a->block/512))return -1;
 u8*best=i->alloc(i->context,a->block),*b=i->alloc(i->context,a->block);int result=-1;
 if(!best||!b)goto done;
 if(abytes(a,0,best,a->block)||checksum_a(best,a->block)||(al4(best+24)&65535)!=1||!al8(best+16))goto done;
 u32 count=al4(best+104);u64 base=al8(best+112);
 if((count&0x80000000u)||!count||count>1048576||!arange(base,count,a->blocks))goto done;
 u8 uuid[16];memcpy(uuid,best+72,16);
 for(u32 k=0;k<count;k++){
  if(abytes(a,(base+k)*a->block,b,a->block))goto done;
  if(memcmp(b+32,"NXSB",4)||(al4(b+24)&65535)!=1||checksum_a(b,a->block))continue;
  if(al4(b+36)!=a->block||al8(b+40)!=a->blocks||memcmp(b+72,uuid,16))goto done;
  if(al8(b+16)>al8(best+16))memcpy(best,b,a->block);
 }
 if(memcmp(best+32,"NXSB",4)||al8(best+56)||(al8(best+64)&~2ull)||!(al8(best+64)&2)||al4(best+180)>APFS_VOLUMES)goto done;
 a->xid=al8(best+16);for(unsigned j=0;j<APFS_VOLUMES;j++)a->fs_oid[j]=al8(best+184+j*8);
 if(omap_tree_a(a,al8(best+160),&a->container_omap))goto done;
 result=0;
done:if(best)i->free(i->context,best);if(b)i->free(i->context,b);return result;
}
static int volume_a(Apfs*a,unsigned slot){
 if(slot>=APFS_VOLUMES||!a->fs_oid[slot])return -1;u64 p;
 if(map_a(a,a->container_omap,a->fs_oid[slot],&p))return -1;u8*b=a->io->alloc(a->io->context,a->block);if(!b)return -1;int result=-1;
 if(object_a(a,p,b,13)||al8(b+8)!=a->fs_oid[slot]||memcmp(b+32,"APSB",4)||!(al8(b+264)&1)||(al8(b+56)&~9ull)||al8(b+48))goto done;
 a->features=al8(b+56);u64 root_oid=al8(b+136),omap_oid=al8(b+128);u32 treetype=al4(b+116);
 if((treetype&65535)!=14||(treetype&0xb8000000u))goto done;
 a->physical=!!(treetype&0x40000000u);
 if(omap_tree_a(a,omap_oid,&a->volume_omap))goto done;
 if(a->physical)a->root=root_oid;else if(map_a(a,a->volume_omap,root_oid,&a->root))goto done;
 ANode node;if(node_a(a,a->root,b,&node,0,0,1,-1)||node.flags&4)goto done;
 const u8*info=b+a->block-40;if(al4(info+4)!=a->block||!!(al4(info)&16)!=a->physical||!al8(info+32)||al8(info+32)>a->blocks)goto done;
 result=0;
done:a->io->free(a->io->context,b);return result;
}
/* Walk one virtual filesystem tree. Budgets are corruption guards, not a
 * file/image byte limit. Only the current recursion path keeps node buffers. */
typedef struct {Apfs*a;u64 path[APFS_DEPTH],remaining;int(*visit)(Apfs*,const ARecord*,void*);void*arg;} AWalk;
static int walk_node_a(AWalk*w,u64 physical,unsigned depth,int expected){
 Apfs*a=w->a;if(depth>=APFS_DEPTH||!w->remaining--)return -1;
 for(unsigned k=0;k<depth;k++)if(w->path[k]==physical)return -1;w->path[depth]=physical;
 u8*b=a->io->alloc(a->io->context,a->block);if(!b)return -1;ANode n;int result=-1;
 if(node_a(a,physical,b,&n,0,0,!depth,expected)||n.flags&4)goto done;
 if(!depth){u64 nodes=al8(b+a->block-8);if(!nodes||nodes>a->blocks)goto done;w->remaining=nodes-1;}
 for(unsigned j=0;j<n.n;j++){
  result=-1;ARecord r;if(record_a(&n,j,&r))goto done;
  if(n.flags&2){result=w->visit(a,&r,w->arg);if(result)goto done;}
  else{if(r.vn!=8)goto done;u64 next=al8(r.v);if(!a->physical&&map_a(a,a->volume_omap,next,&next))goto done;
   result=walk_node_a(w,next,depth+1,n.level-1);if(result)goto done;
  }
 }result=0;
done:a->io->free(a->io->context,b);return result;
}
static int walk_a(Apfs*a,int(*visit)(Apfs*,const ARecord*,void*),void*arg){AWalk w={.a=a,.remaining=a->blocks,.visit=visit,.arg=arg};return walk_node_a(&w,a->root,0,-1);}
static int key_a(const ARecord*r,u64 id,unsigned type){return r->kn>=8&&(al8(r->k)&APFS_ID_MASK)==id&&(al8(r->k)>>60)==type;}
typedef struct {u64 id;AFile*f;int found;} InodeQ;
static int inode_visit_a(Apfs*a,const ARecord*r,void*arg){
 (void)a;InodeQ*q=arg;if(!key_a(r,q->id,3))return 0;
 if(q->found||r->kn!=8||r->vn<92)return -1;const u8*v=r->v;
 AFile*f=q->f;*f=(AFile){.id=q->id,.stream=al8(v+8),.mode=al2(v+80),.bsd=al4(v+68)};f->dir=(f->mode&0170000)==0040000;
 f->special=(!f->dir&&(f->mode&0170000)!=0100000)||(f->bsd&0x20)||(al8(v+48)&0x80);
 if(r->vn>92){
  const u8*x=v+92;unsigned bytes=r->vn-92;if(bytes<4)return -1;unsigned count=al2(x),used=al2(x+2),at=4+count*4;if(at>bytes||used>bytes-at)return -1;unsigned end=at+used;int has_stream=0;
  for(unsigned j=0;j<count;j++){unsigned size=al2(x+4+j*4+2);if(at>end||size>end-at)return -1;
   if(x[4+j*4]==8){if(has_stream++||size!=40||al8(x+at+16))return -1;f->size=al8(x+at);}
   at+=(size+7)&~7u;
  }
 }
 q->found=1;return 0;
}
static int inode_a(Apfs*a,u64 id,AFile*f){InodeQ q={.id=id,.f=f};return walk_a(a,inode_visit_a,&q)||!q.found?-1:0;}
static int utf8_a(const u8*s,unsigned n){
 if(!n||s[n-1]||n>1021)return -1;for(unsigned j=0;j+1<n;){u8 c=s[j++];if(!c||c=='/'||c<32)return -1;if(c<128)continue;unsigned more;u32 v,min;
  if(c>=0xc2&&c<=0xdf){more=1;v=c&31;min=128;}else if(c>=0xe0&&c<=0xef){more=2;v=c&15;min=2048;}else if(c>=0xf0&&c<=0xf4){more=3;v=c&7;min=65536;}else return -1;
  if(more>n-1-j)return -1;while(more--){c=s[j++];if((c&0xc0)!=0x80)return -1;v=v*64+(c&63);}if(v<min||v>0x10ffff||(v>=0xd800&&v<=0xdfff))return -1;
 }return 0;
}
static int same_a(Apfs*a,const char*x,const char*y){while(*x&&*y){unsigned c=(u8)*x++,d=(u8)*y++;if(a->features&1){if(c>='A'&&c<='Z')c+=32;if(d>='A'&&d<='Z')d+=32;}if(c!=d)return 0;}return !*x&&!*y;}
typedef struct {u64 parent,id;const char*name;ForeignEntry*out;int cap,skip,count,found;} DirQ;
static int directory_a(Apfs*a,const ARecord*r,void*arg){
 DirQ*q=arg;if(!key_a(r,q->parent,9))return 0;unsigned prefix=(a->features&9)?12:10;if(r->kn<prefix||r->vn<18)return -1;
 unsigned n=prefix==12?(al4(r->k+8)&1023):al2(r->k+8);if(n>r->kn-prefix||utf8_a(r->k+prefix,n))return -1;
 const char*name=(const char*)r->k+prefix;u64 id=al8(r->v);if(!id||id>APFS_ID_MASK)return -1;
 if(q->name){if(!same_a(a,name,q->name))return 0;if(q->found)return -1;q->id=id;q->found=1;return 0;}
 if(q->skip){q->skip--;return 0;}if(q->count>=q->cap)return 1;
 /* Do not nest inode walks while a large directory walk retains its path.
  * Caller fills sizes with separate bounded lookups after collecting names. */
 ForeignEntry*e=q->out+q->count++;*e=(ForeignEntry){.size=id,.dir=(al2(r->v+16)&15)==4,.ro=1};strset(e->name,name,sizeof e->name);return q->count==q->cap?1:0;
}
/* Real volume names with unambiguous stable aliases. The public directory
 * ABI still has 47-byte names: longer names use Volume N, never truncation.
 * Old /Volume N paths stay valid even when a short actual name is displayed. */
static int alias_slot_a(const char*name){
 const char*prefix="Volume ";for(unsigned j=0;prefix[j];j++)if(*name++!=prefix[j])return -1;
 unsigned n=0,digits=0;while(*name>='0'&&*name<='9'){if(++digits>3||(!n&&*name=='0'))return -1;n=n*10+(*name++-'0');}
 return !*name&&digits&&n&&n<=APFS_VOLUMES?(int)n-1:-1;
}
static int raw_label_a(Apfs*a,unsigned slot,char label[48]){
 u64 physical;if(slot>=APFS_VOLUMES||!a->fs_oid[slot]||map_a(a,a->container_omap,a->fs_oid[slot],&physical))return -1;
 u8*b=a->io->alloc(a->io->context,a->block);if(!b)return -1;int result=-1;
 if(!object_a(a,physical,b,13)&&al8(b+8)==a->fs_oid[slot]&&!memcmp(b+32,"APSB",4)){
  unsigned n=0;while(n<256&&b[704+n])n++;
  if(n&&n<48&&!utf8_a(b+704,n+1)){memcpy(label,b+704,n+1);result=0;}
 }a->io->free(a->io->context,b);return result;
}
static void volume_label_a(Apfs*a,unsigned slot,char label[48]){
 char num[24],actual[48];number(num,slot+1);strset(label,"Volume ",48);append(label,num,48);
 if(raw_label_a(a,slot,actual)||alias_slot_a(actual)>=0)return;
 for(unsigned j=0;j<APFS_VOLUMES;j++)if(j!=slot&&a->fs_oid[j]){char other[48];if(!raw_label_a(a,j,other)&&eq(actual,other))return;}
 strset(label,actual,48);
}
static int path_a(Apfs*a,const char*path,const char**inner,int*slot){
 if(!path||*path!='/')return -1;while(*path=='/')path++;if(!*path){*slot=-1;*inner="/";return 0;}
 char name[48];unsigned n=0;while(*path&&*path!='/'){if(n>=sizeof name-1)return -1;name[n++]=*path++;}name[n]=0;
 int index=alias_slot_a(name);
 if(index<0){for(unsigned j=0;j<APFS_VOLUMES;j++)if(a->fs_oid[j]){char label[48];volume_label_a(a,j,label);if(eq(name,label)){index=j;break;}}}
 if(index<0||!a->fs_oid[index])return -1;*slot=index;*inner=*path?path:"/";return volume_a(a,index);
}
static int resolve_a(Apfs*a,const char*path,AFile*f){
 if(inode_a(a,2,f)||!f->dir)return -1;unsigned depth=0;
 while(*path){while(*path=='/')path++;if(!*path)break;if(!f->dir||++depth>64)return -1;char name[1021];unsigned n=0;
  while(*path&&*path!='/'){if(n>=sizeof name-1)return -1;name[n++]=*path++;}name[n]=0;if(eq(name,".")||eq(name,".."))return -1;
  DirQ q={.parent=f->id,.name=name};if(walk_a(a,directory_a,&q)||!q.found||inode_a(a,q.id,f))return -1;
 }return 0;
}
int apfs_volume_stat(ForeignIo*i,const char*path,ForeignEntry*out){
 if(!out)return -1;Apfs a;if(container_a(&a,i))return -1;const char*inner;int slot;
 if(path_a(&a,path,&inner,&slot))return -1;if(slot<0){*out=(ForeignEntry){.dir=1,.ro=1};return 0;}
 AFile f;if(resolve_a(&a,inner,&f))return -1;*out=(ForeignEntry){.size=f.size,.dir=f.dir,.ro=1};return 0;
}
int apfs_volume_list(ForeignIo*i,const char*path,ForeignEntry*out,int cap,int skip){
 if(cap<0||skip<0||(!out&&cap))return -1;Apfs a;if(container_a(&a,i))return -1;const char*inner;int slot;
 if(path_a(&a,path,&inner,&slot))return -1;
 if(slot<0){int n=0;for(unsigned j=0;j<APFS_VOLUMES&&n<cap;j++)if(a.fs_oid[j]){if(skip){skip--;continue;}out[n]=(ForeignEntry){.dir=1,.ro=1};char label[48];volume_label_a(&a,j,label);strset(out[n++].name,label,sizeof out[0].name);}return n;}
 AFile f;if(resolve_a(&a,inner,&f)||!f.dir)return -1;DirQ q={.parent=f.id,.out=out,.cap=cap,.skip=skip};
 if(cap&&walk_a(&a,directory_a,&q)<0)return -1;
 for(int j=0;j<q.count;j++){u64 id=out[j].size;if(inode_a(&a,id,&f))return -1;out[j].size=f.size;out[j].dir=f.dir;}return q.count;
}
typedef struct {u64 stream,at,end,physical;int found;} ExtentQ;
static int extent_a(Apfs*a,const ARecord*r,void*arg){
 ExtentQ*q=arg;if(!key_a(r,q->stream,8))return 0;if(r->kn!=16||r->vn!=24)return -1;
 u64 start=al8(r->k+8),length=al8(r->v)&0x00ffffffffffffffull,physical=al8(r->v+8);unsigned flags=al8(r->v)>>56;
 if(flags||al8(r->v+16)||!length||start%a->block||length%a->block||!arange(start,length,~(u64)0)||physical>=a->blocks||(physical&&length/a->block>a->blocks-physical))return -1;
 if(q->at<start||q->at-start>=length)return 0;if(q->found)return -1;q->found=1;q->end=start+length;q->physical=physical?physical*a->block+(q->at-start):0;return 0;
}
i64 apfs_volume_read(ForeignIo*i,const char*path,u64 off,void*p,size_t n){
 if(n&&!p)return -1;Apfs a;if(container_a(&a,i))return -1;const char*inner;int slot;
 if(path_a(&a,path,&inner,&slot)||slot<0)return -1;AFile f;if(resolve_a(&a,inner,&f)||f.dir||f.special)return -1;
 if(off>=f.size)return 0;n=MIN((u64)n,f.size-off);size_t done=0;
 while(done<n){ExtentQ q={.stream=f.stream,.at=off+done};if(walk_a(&a,extent_a,&q)||!q.found)return -1;size_t take=MIN((u64)(n-done),q.end-q.at);
  if(!q.physical)memset((u8*)p+done,0,take);else if(abytes(&a,q.physical,(u8*)p+done,take))return -1;done+=take;
 }return done;
}

/* Attach preflight: a container signature alone isn't a browsable volume. */
int apfs_volume_validate(ForeignIo*i){
 Apfs a;if(container_a(&a,i))return -1;
 for(unsigned j=0;j<APFS_VOLUMES;j++)if(a.fs_oid[j]&&!volume_a(&a,j)){
  AFile root;if(!inode_a(&a,2,&root)&&root.dir)return 0;
 }return -1;
}
