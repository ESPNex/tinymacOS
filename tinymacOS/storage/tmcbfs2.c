/* SPDX-License-Identifier: MIT
 * Development-only TMCBFS2 core. Not wired to fs_init/the installer yet.
 * Immutable data/tree blocks + ordered double-superblock publication. Allocation
 * is reconstructed from both crash-fallback roots and all snapshot roots: no
 * mutable on-disk free bitmap can publish a block as free before its last root.
 */
#include "tmcbfs2.h"
#define INODES 1u
#define MAP 2u
#define SNAPS 3u
#define HEIGHT 8u
#define ITEMS ((T2_BLOCK-64)/sizeof(T2Entry))
typedef struct {u8 magic[8];u32 kind,level,count,crc;u64 reserved[5];T2Entry e[];} Tree;
_Static_assert(sizeof(Tree)==64,"TMCBFS2 tree header");
typedef struct {u64 left,lmax,right,rmax;unsigned level;} Split;
static u32 crc(const void*p,size_t n){const u8*s=p;u32 c=~0u;while(n--){c^=*s++;for(int k=0;k<8;k++)c=(c>>1)^(0xedb88320u&-(c&1));}return ~c;}
static void*alloc(Tmc2*c,size_t n){return c->io.alloc(c->io.context,n);}
static void release(Tmc2*c,void*p){if(p)c->io.free(c->io.context,p);}
/* Sparse page-backed bitmaps: an empty 1TiB volume does not allocate 16MiB
 * from the kernel's 8MiB heap. The kernel adapter supplies independent physical
 * pages for 4096-byte allocations; page tables themselves remain small. */
#define BIT_PAGE 4096u
#define BIT_BITS (BIT_PAGE*8u)
static int fail(Tmc2*c){c->faulted=1;return -1;}
static int bit(u8*const*p,u64 n){u8*q=p[n/BIT_BITS];n%=BIT_BITS;return q?!!(q[n>>3]&(1u<<(n&7))):0;}
static int mark(Tmc2*c,u8**p,u64 n){
 u64 page=n/BIT_BITS;n%=BIT_BITS;
 if(!p[page]){p[page]=alloc(c,BIT_PAGE);if(!p[page])return fail(c);memset(p[page],0,BIT_PAGE);}
 p[page][n>>3]|=1u<<(n&7);return 0;
}
static void bitmap_clear(Tmc2*c,u8**p){if(p)for(size_t i=0;i<c->bitmap_pages;i++)if(p[i]){release(c,p[i]);p[i]=0;}}
static int readblock(Tmc2*c,u64 n,void*p){if(n>=c->io.blocks||c->io.read(c->io.context,n,p))return fail(c);return 0;}
static int writeblock(Tmc2*c,u64 n,const void*p){if(n>=c->io.blocks||c->io.write(c->io.context,n,p))return fail(c);return 0;}
static int flush(Tmc2*c){return c->io.flush(c->io.context)?fail(c):0;}
static int name_ok(const char*s){if(!s||!*s||eq(s,".")||eq(s,".."))return 0;unsigned n=0;for(;s[n];n++)if(n>=T2_NAME-1||s[n]=='/'||(u8)s[n]<32)return 0;return 1;}
static Tree*tree_read(Tmc2*c,u64 at,unsigned kind){
 if(at<2||at>=c->io.blocks){fail(c);return 0;}
 Tree*t=alloc(c,T2_BLOCK+sizeof(T2Entry));if(!t){fail(c);return 0;}
 if(readblock(c,at,t))goto bad;
 u32 saved=t->crc;t->crc=0;u32 actual=crc(t,T2_BLOCK);t->crc=saved;
 if(memcmp(t->magic,"TM2TREE",8)||saved!=actual||t->kind!=kind||t->level>HEIGHT||!t->count||t->count>ITEMS)goto bad;
 for(unsigned i=0;i<t->count;i++){
  T2Entry*e=t->e+i;
  if((i&&t->e[i-1].key>=e->key)||(t->level&&(e->parent<2||e->parent>=c->io.blocks)))goto bad;
  if(!t->level){
   if(kind==INODES&&(e->key<2||e->kind>T2_FILE||e->size>T2_MAX_BYTES||e->name[T2_NAME-1]))goto bad;
   if(kind==SNAPS&&(!e->key||(e->kind&&e->kind!=SNAPS)||e->name[T2_NAME-1]))goto bad;
   if(kind==MAP&&(e->key>=T2_MAX_BYTES/T2_BLOCK||e->parent<2||e->parent>=c->io.blocks||e->size>0xffffffffu))goto bad;
  }
 }
 return t;
 bad:release(c,t);fail(c);return 0;
}
static int get(Tmc2*c,u64 root,unsigned kind,u64 key,T2Entry*out){
 for(unsigned depth=0;root&&depth<=HEIGHT;depth++){
  Tree*t=tree_read(c,root,kind);if(!t)return -1;unsigned i=0;while(i<t->count&&t->e[i].key<key)i++;
  if(t->level){if(i==t->count){release(c,t);return 0;}root=t->e[i].parent;release(c,t);}
  else{int found=i<t->count&&t->e[i].key==key;if(found)*out=t->e[i];release(c,t);return found;}
 }
 return root?fail(c):0;
}
static u64 newblock(Tmc2*c){
 if(!c->transaction||c->nfresh==64){fail(c);return 0;}
 for(u64 i=0;i<c->io.blocks-2;i++){
  u64 at=c->cursor++;if(c->cursor>=c->io.blocks)c->cursor=2;
  if(!bit(c->used,at)){if(mark(c,c->used,at))return 0;c->allocated++;c->fresh[c->nfresh++]=at;return at;}
 }
 return 0;
}
static u64 tree_write(Tmc2*c,Tree*t){
 u64 at=newblock(c);if(!at)return 0;
 memcpy(t->magic,"TM2TREE",8);t->crc=0;t->reserved[0]=c->super.generation+1;t->crc=crc(t,T2_BLOCK);
 return writeblock(c,at,t)?0:at;
}
static int insert(Tmc2*c,u64 root,unsigned kind,const T2Entry*entry,unsigned depth,Split*out){
 if(depth>HEIGHT)return fail(c);
 Tree*t;
 if(root)t=tree_read(c,root,kind);
 else{t=alloc(c,T2_BLOCK+sizeof(T2Entry));if(t){memset(t,0,T2_BLOCK+sizeof(T2Entry));t->kind=kind;}}
 if(!t)return -1;
 unsigned i=0;while(i<t->count&&t->e[i].key<entry->key)i++;
 if(t->level){
  if(i==t->count)i--;Split child;
  if(insert(c,t->e[i].parent,kind,entry,depth+1,&child)){release(c,t);return -1;}
  if(child.level+1!=t->level){release(c,t);return fail(c);}
  t->e[i]=(T2Entry){.key=child.lmax,.parent=child.left};
  if(child.right){memmove(t->e+i+2,t->e+i+1,(t->count-i-1)*sizeof(T2Entry));t->e[i+1]=(T2Entry){.key=child.rmax,.parent=child.right};t->count++;}
 }else{
  if(i<t->count&&t->e[i].key==entry->key)t->e[i]=*entry;
  else{memmove(t->e+i+1,t->e+i,(t->count-i)*sizeof(T2Entry));t->e[i]=*entry;t->count++;}
 }
 memset(out,0,sizeof *out);out->level=t->level;
 if(t->count<=ITEMS){out->lmax=t->e[t->count-1].key;out->left=tree_write(c,t);release(c,t);return out->left?0:-1;}
 unsigned left=t->count/2,right=t->count-left;Tree*r=alloc(c,T2_BLOCK+sizeof(T2Entry));if(!r){release(c,t);return -1;}
 memset(r,0,T2_BLOCK+sizeof(T2Entry));r->kind=kind;r->level=t->level;r->count=right;memcpy(r->e,t->e+left,right*sizeof(T2Entry));t->count=left;
 /* Unused bytes must be deterministic and cannot retain split-out records. */
 memset(t->e+left,0,T2_BLOCK-64-left*sizeof(T2Entry));
 out->lmax=t->e[left-1].key;out->rmax=r->e[right-1].key;out->left=tree_write(c,t);out->right=out->left?tree_write(c,r):0;
 release(c,t);release(c,r);return out->left&&out->right?0:-1;
}
static int put(Tmc2*c,u64 root,unsigned kind,const T2Entry*e,u64*out){
 Split s;if(insert(c,root,kind,e,0,&s))return -1;
 if(!s.right){*out=s.left;return 0;}
 if(s.level==HEIGHT)return fail(c);
 Tree*t=alloc(c,T2_BLOCK+sizeof(T2Entry));if(!t)return -1;memset(t,0,T2_BLOCK+sizeof(T2Entry));t->kind=kind;t->level=s.level+1;t->count=2;
 t->e[0]=(T2Entry){.key=s.lmax,.parent=s.left};t->e[1]=(T2Entry){.key=s.rmax,.parent=s.right};*out=tree_write(c,t);release(c,t);return *out?0:-1;
}
static int publish(Tmc2*c,T2Super s){
 if(c->faulted||c->super.generation==~0ull)return -1;
 u8*p=alloc(c,T2_BLOCK);if(!p)return -1;memset(p,0,T2_BLOCK);
 s.generation=c->super.generation+1;s.crc=0;memcpy(p,&s,sizeof s);((T2Super*)p)->crc=crc(p,T2_BLOCK);
 int slot=c->active^1;
 if(flush(c)||writeblock(c,slot,p)||flush(c)){release(c,p);return fail(c);}
 s.crc=((T2Super*)p)->crc;c->super=s;c->slots[slot]=s;c->valid[slot]=1;c->active=slot;c->transaction=0;c->nfresh=0;
 release(c,p);return 0;
}
/* Iteration uses immutable pages; callback never mutates the current tree. */
typedef int(*Visit)(const T2Entry*,void*);
static int visit(Tmc2*c,u64 root,unsigned kind,unsigned depth,Visit fn,void*x){
 if(!root)return 0;if(depth>HEIGHT)return fail(c);Tree*t=tree_read(c,root,kind);if(!t)return -1;int r=0;
 for(unsigned i=0;i<t->count&&!r;i++)r=t->level?visit(c,t->e[i].parent,kind,depth+1,fn,x):fn(t->e+i,x);
 release(c,t);return r;
}
static int marktree(Tmc2*c,u64 at,unsigned kind,int level,u64 stack[],unsigned depth,u64*max){
 if(!at){*max=0;return 0;}if(depth>3*HEIGHT+3)return fail(c);
 for(unsigned i=0;i<depth;i++)if(stack[i]==at)return fail(c);stack[depth]=at;
 Tree*t=tree_read(c,at,kind);if(!t)return -1;
 if(level>=0&&t->level!=(unsigned)level){release(c,t);return fail(c);}*max=t->e[t->count-1].key;
 if(bit(c->used,at)){int okay=bit(c->tree,at);release(c,t);return okay?0:fail(c);}
 if(mark(c,c->used,at)||mark(c,c->tree,at)){release(c,t);return -1;}c->allocated++;
 for(unsigned i=0;i<t->count;i++){
  T2Entry*e=t->e+i;u64 child=0,m=0;unsigned next=kind;int expected=-1;
  if(t->level){child=e->parent;expected=t->level-1;}
  else if(kind==INODES&&e->kind==T2_FILE){child=e->root;next=MAP;}
  else if(kind==SNAPS&&e->kind==SNAPS){child=e->parent;next=INODES;}
  else if(kind==MAP){
   if(e->parent<2||e->parent>=c->io.blocks||bit(c->tree,e->parent)){release(c,t);return fail(c);}
   if(!bit(c->used,e->parent)){if(mark(c,c->used,e->parent)){release(c,t);return -1;}c->allocated++;}
  }
  if(child&&marktree(c,child,next,expected,stack,depth+1,&m)){release(c,t);return -1;}
  if(!t->level&&kind==SNAPS&&e->kind==SNAPS&&e->root&&marktree(c,e->root,MAP,-1,stack,depth+1,&m)){release(c,t);return -1;}
  if(t->level&&m!=e->key){release(c,t);return fail(c);}
 }
 release(c,t);return 0;
}
static int rebuild(Tmc2*c){
 bitmap_clear(c,c->used);bitmap_clear(c,c->tree);if(mark(c,c->used,0)||mark(c,c->used,1))return -1;c->allocated=2;
 u64 stack[3*HEIGHT+5],m;
 for(int i=0;i<2;i++)if(c->valid[i]){
  if(marktree(c,c->slots[i].live,INODES,-1,stack,0,&m)||marktree(c,c->slots[i].snapshots,SNAPS,-1,stack,0,&m)||marktree(c,c->slots[i].control,MAP,-1,stack,0,&m))return -1;
 }
 c->cursor=2;return 0;
}
static int setup(Tmc2*c,T2Io io){
 if(!c||!io.read||!io.write||!io.flush||!io.alloc||!io.free||io.blocks<128||io.blocks>T2_MAX_BYTES/T2_BLOCK)return -1;
 memset(c,0,sizeof *c);c->io=io;c->bitmap_pages=(io.blocks+BIT_BITS-1)/BIT_BITS;
 c->used=alloc(c,c->bitmap_pages*sizeof(u8*));if(c->used)memset(c->used,0,c->bitmap_pages*sizeof(u8*));
 c->tree=alloc(c,c->bitmap_pages*sizeof(u8*));if(c->tree)memset(c->tree,0,c->bitmap_pages*sizeof(u8*));
 if(!c->used||!c->tree){tmc2_close(c);return -1;}return 0;
}
void tmc2_close(Tmc2*c){if(!c)return;bitmap_clear(c,c->used);bitmap_clear(c,c->tree);release(c,c->used);release(c,c->tree);memset(c,0,sizeof *c);}
int tmc2_format(Tmc2*c,T2Io io){
 if(setup(c,io))return -1;c->active=1;c->super=(T2Super){.version=2,.block_bytes=T2_BLOCK,.blocks=io.blocks,.next_inode=2,.next_snapshot=1};memcpy(c->super.magic,"TMCBFS2",8);
 if(publish(c,c->super)||publish(c,c->super)||rebuild(c)){tmc2_close(c);return -1;}return 0;
}
int tmc2_mount(Tmc2*c,T2Io io){
 if(setup(c,io))return -1;u8*p=alloc(c,T2_BLOCK);if(!p){tmc2_close(c);return -1;}
 for(int i=0;i<2;i++){
  if(io.read(io.context,i,p))continue;T2Super*s=(void*)p;u32 saved=s->crc;s->crc=0;u32 actual=crc(p,T2_BLOCK);s->crc=saved;
  if(memcmp(s->magic,"TMCBFS2",8)||s->version!=2||s->block_bytes!=T2_BLOCK||s->blocks!=io.blocks||!s->generation||s->next_inode<2||!s->next_snapshot||saved!=actual)continue;
  c->slots[i]=*s;c->valid[i]=1;
 }
 release(c,p);if(!c->valid[0]&&!c->valid[1])goto bad;
 c->active=c->valid[1]&&(!c->valid[0]||c->slots[1].generation>c->slots[0].generation);c->super=c->slots[c->active];
 if(rebuild(c))goto bad;return 0;
 bad:tmc2_close(c);return -1;
}
int tmc2_collect(Tmc2*c){if(!c||c->faulted||c->transaction)return -1;if(publish(c,c->super))return -1;return rebuild(c);}
u64 tmc2_used_blocks(const Tmc2*c){return c?c->allocated:0;}
static int begin(Tmc2*c){
 if(!c||c->faulted||c->transaction)return -1;
 if(c->allocated+64>=c->io.blocks&&tmc2_collect(c))return -1;
 c->transaction=1;c->nfresh=0;return 0;
}
static int statroot(Tmc2*c,u64 root,u64 id,T2Entry*out){
 if(c->faulted||!out)return -1;
 if(id==1){*out=(T2Entry){.key=1,.parent=1,.kind=T2_DIR};return 0;}
 return get(c,root,INODES,id,out)==1&&(out->kind==T2_FILE||out->kind==T2_DIR)?0:-1;
}
int tmc2_stat(Tmc2*c,u64 id,T2Entry*out){return c?statroot(c,c->super.live,id,out):-1;}
typedef struct{u64 parent;const char*name;T2Entry*out;} Search;
static int named(const T2Entry*e,void*p){Search*s=p;if(e->kind&&e->parent==s->parent&&eq(e->name,s->name)){*s->out=*e;return 1;}return 0;}
int tmc2_lookup(Tmc2*c,u64 parent,const char*name,T2Entry*out){
 if(!c||c->faulted||!out||!name_ok(name))return -1;Search s={parent,name,out};return visit(c,c->super.live,INODES,0,named,&s)==1?0:-1;
}
typedef struct{u64 parent,cookie;T2Entry*out;int snapshots;} Next;
static int next_entry(const T2Entry*e,void*p){
 Next*n=p;if(e->key>n->cookie&&e->kind&&(n->snapshots?e->kind==SNAPS:e->parent==n->parent)){*n->out=*e;return 1;}return 0;
}
/* Interior keys are inclusive subtree maxima. A continuation cookie can skip
 * entire immutable subtrees, rather than rereading every preceding leaf for
 * every entry. Keep the same monotonically increasing public cookie/ABI. */
static int visit_after(Tmc2*c,u64 root,unsigned kind,unsigned depth,Next*n){
 if(!root)return 0;if(depth>HEIGHT)return fail(c);
 Tree*t=tree_read(c,root,kind);if(!t)return -1;int r=0;
 for(unsigned i=0;i<t->count&&!r;i++){
  if(t->e[i].key<=n->cookie)continue;
  r=t->level?visit_after(c,t->e[i].parent,kind,depth+1,n):next_entry(t->e+i,n);
 }
 release(c,t);return r;
}
int tmc2_readdir(Tmc2*c,u64 parent,u64*cookie,T2Entry*out){
 T2Entry dir;if(!c||!cookie||!out||tmc2_stat(c,parent,&dir)||dir.kind!=T2_DIR)return -1;
 Next n={parent,*cookie,out,0};int r=visit_after(c,c->super.live,INODES,0,&n);if(r==1)*cookie=out->key;return r;
}
int tmc2_snapshots(Tmc2*c,u64*cookie,T2Entry*out){
 if(!c||c->faulted||!cookie||!out)return -1;Next n={0,*cookie,out,1};int r=visit_after(c,c->super.snapshots,SNAPS,0,&n);if(r==1)*cookie=out->key;return r;
}
int tmc2_create(Tmc2*c,u64 parent,const char*name,unsigned kind,u64*out){
 T2Entry e,exists;if(!c||!out||!name_ok(name)||(kind!=T2_FILE&&kind!=T2_DIR)||tmc2_stat(c,parent,&e)||e.kind!=T2_DIR||!tmc2_lookup(c,parent,name,&exists)||c->super.next_inode==~0ull)return -1;
 e=(T2Entry){.key=c->super.next_inode,.parent=parent,.kind=kind};strset(e.name,name,sizeof e.name);if(begin(c))return -1;T2Super s=c->super;
 if(put(c,s.live,INODES,&e,&s.live))return fail(c);s.next_inode++;
 if(publish(c,s))return -1;*out=e.key;return 0;
}
int tmc2_reset(Tmc2*c,const T2Seed*seed,unsigned count){
 /* A bounded factory plan fits in one transaction. Never reset counters: stale
  * inode and snapshot IDs must not alias newly enrolled users' data. */
 if(!c||c->faulted||c->transaction||count>32||(!seed&&count)||c->super.next_inode>~0ull-count)return -1;
 for(unsigned i=0;i<count;i++){
  if(!name_ok(seed[i].name)||(seed[i].kind!=T2_DIR&&seed[i].kind!=T2_FILE)||seed[i].parent>i||(seed[i].parent&&seed[seed[i].parent-1].kind!=T2_DIR))return -1;
  for(unsigned j=0;j<i;j++)if(seed[j].parent==seed[i].parent&&eq(seed[j].name,seed[i].name))return -1;
 }
 if(begin(c))return -1;T2Super s=c->super;s.live=s.control=s.snapshots=0;
 for(unsigned i=0;i<count;i++){
  T2Entry e={.key=s.next_inode+i,.parent=seed[i].parent?s.next_inode+seed[i].parent-1:1,.kind=seed[i].kind};strset(e.name,seed[i].name,sizeof e.name);
  if(put(c,s.live,INODES,&e,&s.live))return fail(c);
 }
 s.next_inode+=count;return publish(c,s);
}
int tmc2_readonly(Tmc2*c,u64 id,int ro){
 T2Entry e;if((ro!=0&&ro!=1)||id==1||tmc2_stat(c,id,&e)||begin(c))return -1;
 T2Super s=c->super;e.reserved=ro;if(put(c,s.live,INODES,&e,&s.live))return fail(c);return publish(c,s);
}
static i64 readroot(Tmc2*c,u64 root,u64 id,u64 offset,void*buffer,size_t count){
 T2Entry e;if(!buffer||statroot(c,root,id,&e)||e.kind!=T2_FILE)return -1;if(offset>=e.size||!count)return 0;
 count=MIN((u64)count,e.size-offset);u8*p=buffer;size_t total=count;u8*data=alloc(c,T2_BLOCK);if(!data)return -1;
 while(count){u64 block=offset/T2_BLOCK;unsigned at=offset%T2_BLOCK,take=MIN(count,T2_BLOCK-at);T2Entry m;int r=get(c,e.root,MAP,block,&m);
  if(r<0){release(c,data);return -1;}
  if(r){if(readblock(c,m.parent,data)||crc(data,T2_BLOCK)!=m.size){release(c,data);return fail(c);}memcpy(p,data+at,take);}
  else memset(p,0,take);
  p+=take;offset+=take;count-=take;
 }
 release(c,data);return total;
}
i64 tmc2_read(Tmc2*c,u64 id,u64 off,void*p,size_t n){return c?readroot(c,c->super.live,id,off,p,n):-1;}
i64 tmc2_write(Tmc2*c,u64 id,u64 offset,const void*buffer,size_t count){
 if(!c||!buffer||offset>T2_MAX_BYTES||count>T2_MAX_BYTES-offset)return -1;
 T2Entry e;if(tmc2_stat(c,id,&e)||e.kind!=T2_FILE)return -1;
 const u8*p=buffer;size_t total=0;u8*data=alloc(c,T2_BLOCK);if(!data)return -1;
 while(count){
  unsigned at=offset%T2_BLOCK,take=MIN(count,T2_BLOCK-at);T2Entry m={.key=offset/T2_BLOCK};
  if(at||take<T2_BLOCK){int r=get(c,e.root,MAP,m.key,&m);if(r<0)goto bad;if(r){if(readblock(c,m.parent,data)||crc(data,T2_BLOCK)!=m.size)goto bad;}else memset(data,0,T2_BLOCK);}
  memcpy(data+at,p,take);if(begin(c))goto bad;
  m=(T2Entry){.key=offset/T2_BLOCK,.size=crc(data,T2_BLOCK)};m.parent=newblock(c);if(!m.parent||writeblock(c,m.parent,data))goto bad;
  if(put(c,e.root,MAP,&m,&e.root))goto bad;e.size=MAX(e.size,offset+take);T2Super s=c->super;
  if(put(c,s.live,INODES,&e,&s.live)||publish(c,s))goto bad;
  p+=take;offset+=take;count-=take;total+=take;
 }
 release(c,data);return total;
 bad:release(c,data);fail(c);return total?(i64)total:-1;
}
static int child(const T2Entry*e,void*p){return e->kind&&e->parent==*(u64*)p;}
int tmc2_remove(Tmc2*c,u64 id){
 T2Entry e;if(!c||id==1||tmc2_stat(c,id,&e))return -1;
 if(e.kind==T2_DIR&&visit(c,c->super.live,INODES,0,child,&id))return -1;
 e.kind=0;e.root=0;e.size=0;memset(e.name,0,sizeof e.name);if(begin(c))return -1;T2Super s=c->super;
 if(put(c,s.live,INODES,&e,&s.live))return fail(c);return publish(c,s);
}
static int snapshot_named(const T2Entry*e,void*p){Search*s=p;if(e->kind==SNAPS&&eq(e->name,s->name)){*s->out=*e;return 1;}return 0;}
static int snapshot_get(Tmc2*c,const char*name,T2Entry*out){Search s={0,name,out};return !c||c->faulted||!name_ok(name)?-1:visit(c,c->super.snapshots,SNAPS,0,snapshot_named,&s)==1?0:-1;}
int tmc2_snapshot(Tmc2*c,const char*name,u64*out){
 T2Entry e;if(!c||!name_ok(name)||!snapshot_get(c,name,&e)||c->super.next_snapshot==~0ull)return -1;
 e=(T2Entry){.key=c->super.next_snapshot,.parent=c->super.live,.size=c->super.next_inode,.kind=SNAPS,.root=c->super.control};strset(e.name,name,sizeof e.name);
 if(begin(c))return -1;T2Super s=c->super;if(put(c,s.snapshots,SNAPS,&e,&s.snapshots))return fail(c);s.next_snapshot++;if(publish(c,s))return -1;if(out)*out=e.key;return 0;
}
int tmc2_checkpoint(Tmc2*c,const char*name){
 T2Entry e;if(!c||!name_ok(name)||c->faulted)return -1;
 if(snapshot_get(c,name,&e))return c->faulted?-1:tmc2_snapshot(c,name,0);
 if(c->super.next_snapshot==~0ull||begin(c))return -1;T2Super s=c->super;
 e.kind=0;e.parent=0;e.root=0;e.name[0]=0;
 if(put(c,s.snapshots,SNAPS,&e,&s.snapshots))return fail(c);
 e=(T2Entry){.key=s.next_snapshot++,.parent=s.live,.root=s.control,.size=s.next_inode,.kind=SNAPS};strset(e.name,name,sizeof e.name);
 if(put(c,s.snapshots,SNAPS,&e,&s.snapshots))return fail(c);return publish(c,s);
}
int tmc2_snapshot_find(Tmc2*c,const char*name,T2Entry*out){return out?snapshot_get(c,name,out):-1;}
static int viewroot(Tmc2*c,u64 id,u64*root){
 T2Entry e;if(!c||c->faulted||!id||get(c,c->super.snapshots,SNAPS,id,&e)!=1||e.kind!=SNAPS)return -1;*root=e.parent;return 0;
}
int tmc2_view_stat(Tmc2*c,u64 snap,u64 id,T2Entry*out){u64 root;return viewroot(c,snap,&root)?-1:statroot(c,root,id,out);}
int tmc2_view_lookup(Tmc2*c,u64 snap,u64 parent,const char*name,T2Entry*out){
 u64 root;T2Entry dir;if(!out||!name_ok(name)||viewroot(c,snap,&root)||statroot(c,root,parent,&dir)||dir.kind!=T2_DIR)return -1;
 Search s={parent,name,out};return visit(c,root,INODES,0,named,&s)==1?0:-1;
}
int tmc2_view_readdir(Tmc2*c,u64 snap,u64 parent,u64*cookie,T2Entry*out){
 u64 root;T2Entry dir;if(!cookie||!out||viewroot(c,snap,&root)||statroot(c,root,parent,&dir)||dir.kind!=T2_DIR)return -1;
 Next n={parent,*cookie,out,0};int r=visit_after(c,root,INODES,0,&n);if(r==1)*cookie=out->key;return r;
}
i64 tmc2_view_read(Tmc2*c,u64 snap,u64 id,u64 offset,void*p,size_t n){u64 root;return viewroot(c,snap,&root)?-1:readroot(c,root,id,offset,p,n);}
int tmc2_snapshot_delete(Tmc2*c,const char*name){
 T2Entry e;if(snapshot_get(c,name,&e)||begin(c))return -1;e.kind=0;e.parent=0;memset(e.name,0,sizeof e.name);T2Super s=c->super;
 if(put(c,s.snapshots,SNAPS,&e,&s.snapshots))return fail(c);return publish(c,s);
}
int tmc2_snapshot_restore(Tmc2*c,const char*name){T2Entry e;if(snapshot_get(c,name,&e))return -1;T2Super s=c->super;s.live=e.parent;s.control=e.root;s.next_inode=MAX(s.next_inode,e.size);return publish(c,s);}
int tmc2_snapshot_stat(Tmc2*c,const char*name,u64 id,T2Entry*out){T2Entry e;return snapshot_get(c,name,&e)?-1:statroot(c,e.parent,id,out);}
i64 tmc2_snapshot_read(Tmc2*c,const char*name,u64 id,u64 offset,void*p,size_t n){T2Entry e;return snapshot_get(c,name,&e)?-1:readroot(c,e.parent,id,offset,p,n);}

/* Prune an extent tree by logical block key, sharing entire retained subtrees.
 * At most one boundary path is copied; large sparse truncation is O(height),
 * not proportional to the old file size. Caller is inside one transaction. */
static int prune(Tmc2*c,u64 root,u64 limit,u64*out,u64*last,unsigned depth){
 *out=root;if(!root){*last=0;return 0;}if(depth>HEIGHT)return fail(c);
 if(!limit){*out=0;*last=0;return 0;}
 Tree*t=tree_read(c,root,MAP);if(!t)return -1;
 if(t->e[t->count-1].key<limit){*last=t->e[t->count-1].key;release(c,t);return 0;}
 unsigned keep=0;
 if(!t->level){while(keep<t->count&&t->e[keep].key<limit)keep++;}
 else{
  for(;keep<t->count;keep++){
   if(t->e[keep].key<limit)continue;
   u64 child,max;if(prune(c,t->e[keep].parent,limit,&child,&max,depth+1)){release(c,t);return -1;}
   if(child){t->e[keep]=(T2Entry){.key=max,.parent=child};keep++;}break;
  }
 }
 if(!keep){*out=0;*last=0;release(c,t);return 0;}
 t->count=keep;*last=t->e[keep-1].key;memset(t->e+keep,0,T2_BLOCK-64-keep*sizeof(T2Entry));
 *out=tree_write(c,t);release(c,t);return *out?0:-1;
}
int tmc2_truncate(Tmc2*c,u64 id,u64 size){
 T2Entry e;if(!c||size>T2_MAX_BYTES||tmc2_stat(c,id,&e)||e.kind!=T2_FILE)return -1;
 if(e.size==size)return 0;if(begin(c))return -1;
 if(size<e.size){
  if(size%T2_BLOCK){
   T2Entry m;int r=get(c,e.root,MAP,size/T2_BLOCK,&m);if(r<0)return fail(c);
   if(r){
    u8*p=alloc(c,T2_BLOCK);if(!p)return fail(c);
    if(readblock(c,m.parent,p)||crc(p,T2_BLOCK)!=m.size){release(c,p);return fail(c);}
    memset(p+size%T2_BLOCK,0,T2_BLOCK-size%T2_BLOCK);m.size=crc(p,T2_BLOCK);m.parent=newblock(c);
    int bad=!m.parent||writeblock(c,m.parent,p);release(c,p);
    if(bad||put(c,e.root,MAP,&m,&e.root))return fail(c);
   }
  }
  u64 last;if(prune(c,e.root,(size+T2_BLOCK-1)/T2_BLOCK,&e.root,&last,0))return fail(c);
 }
 e.size=size;T2Super s=c->super;
 if(put(c,s.live,INODES,&e,&s.live))return fail(c);return publish(c,s);
}
int tmc2_move(Tmc2*c,u64 id,u64 parent,const char*name){
 T2Entry e,p,exists;
 if(!c||id==1||!name_ok(name)||tmc2_stat(c,id,&e)||tmc2_stat(c,parent,&p)||p.kind!=T2_DIR)return -1;
 if(!tmc2_lookup(c,parent,name,&exists))return exists.key==id?0:-1;
 if(c->faulted)return -1;
 if(e.kind==T2_DIR){
  u64 at=parent,walked=0;
  while(at!=1){if(at==id||++walked>=c->super.next_inode||tmc2_stat(c,at,&p)||p.kind!=T2_DIR)return -1;at=p.parent;}
 }
 e.parent=parent;strset(e.name,name,sizeof e.name);if(begin(c))return -1;T2Super s=c->super;
 if(put(c,s.live,INODES,&e,&s.live))return fail(c);return publish(c,s);
}

int tmc2_account(Tmc2*c,unsigned slot,void*buffer,int write){
 if(!c||c->faulted||slot>1||!buffer||(write!=0&&write!=1))return -1;
 T2Entry m;int found=get(c,c->super.control,MAP,slot,&m);if(found<0)return -1;
 u8*p=alloc(c,T2_BLOCK);if(!p)return -1;
 if(found){if(readblock(c,m.parent,p)||crc(p,T2_BLOCK)!=m.size){release(c,p);return fail(c);}}
 else memset(p,0,T2_BLOCK);
 if(!write){memcpy(buffer,p,512);release(c,p);return 0;}
 memcpy(p,buffer,512);if(begin(c)){release(c,p);return -1;}
 m=(T2Entry){.key=slot,.size=crc(p,T2_BLOCK)};m.parent=newblock(c);
 int bad=!m.parent||writeblock(c,m.parent,p);release(c,p);if(bad)return fail(c);
 T2Super s=c->super;if(put(c,s.control,MAP,&m,&s.control))return fail(c);return publish(c,s);
}

static int verify_data(const T2Entry*e,void*x){
 Tmc2*c=x;u8*p=alloc(c,T2_BLOCK);if(!p)return -1;
 int r=readblock(c,e->parent,p);if(!r&&crc(p,T2_BLOCK)!=e->size)r=fail(c);release(c,p);return r;
}
static int verify_inode(const T2Entry*e,void*x){
 Tmc2*c=x;return e->kind==T2_FILE?visit(c,e->root,MAP,0,verify_data,c):0;
}
int tmc2_snapshot_verify(Tmc2*c,const char*name){
 T2Entry s;if(snapshot_get(c,name,&s))return -1;
 return visit(c,s.parent,INODES,0,verify_inode,c)||visit(c,s.root,MAP,0,verify_data,c)?-1:0;
}

static int count_inode(const T2Entry*e,void*x){if(e->kind==T2_FILE||e->kind==T2_DIR)(*(u64*)x)++;return 0;}
i64 tmc2_inode_count(Tmc2*c){if(!c||c->faulted)return -1;u64 n=1;return visit(c,c->super.live,INODES,0,count_inode,&n)?-1:(i64)n;}

int tmc2_verify(Tmc2*c){
 if(!c||c->faulted||visit(c,c->super.live,INODES,0,verify_inode,c)||visit(c,c->super.control,MAP,0,verify_data,c))return -1;
 u64 cookie=0;T2Entry e;int r;
 while((r=tmc2_snapshots(c,&cookie,&e))==1)if(tmc2_snapshot_verify(c,e.name))return -1;
 return r<0?-1:0;
}

/* External checkpoint import. Only a selected immutable snapshot is imported;
 * destination snapshots and envelope identity/credentials remain its own.
 * Recursion is bounded by the inode-tree + map-tree heights, never file count.
 * No source writes, whole-volume buffers, inode-sized arrays or fixed file cap.
 * All new pages remain unreachable until ONE superblock publication. */
static int transfer_tree(Tmc2*source,Tmc2*dest,u64 at,unsigned kind,unsigned depth,u64*out,u64*needed,int copy){
 *out=0;if(!at)return 0;
 if(depth>2*HEIGHT+2||(!copy&&*needed>=dest->io.blocks-dest->allocated))return -1;
 (*needed)++;
 Tree*t=tree_read(source,at,kind);if(!t)return -1;int r=0;
 for(unsigned i=0;i<t->count&&!r;i++){
  T2Entry*e=t->e+i;u64 child=0;
  if(t->level){r=transfer_tree(source,dest,e->parent,kind,depth+1,&child,needed,copy);if(copy)e->parent=child;}
  else if(kind==INODES&&e->kind==T2_FILE){r=transfer_tree(source,dest,e->root,MAP,depth+1,&child,needed,copy);if(copy)e->root=child;}
  else if(kind==MAP){
   if(!copy&&*needed>=dest->io.blocks-dest->allocated){r=-1;break;}(*needed)++;
   if(copy){
    u8*p=alloc(dest,T2_BLOCK);if(!p){r=-1;break;}
    r=readblock(source,e->parent,p);if(!r&&crc(p,T2_BLOCK)!=e->size)r=fail(source);
    if(!r){dest->nfresh=0;child=newblock(dest);r=!child||writeblock(dest,child,p);}
    release(dest,p);if(!r)e->parent=child;
   }
  }
 }
 if(!r&&copy){dest->nfresh=0;*out=tree_write(dest,t);if(!*out)r=-1;}
 release(source,t);return r?-1:0;
}
int tmc2_import_check(Tmc2*dest,Tmc2*source,const char*name,u64*blocks){
 if(!dest||!source||dest==source||dest->faulted||source->faulted||dest->transaction||source->transaction)return -1;
 T2Entry snap;if(snapshot_get(source,name,&snap)||tmc2_snapshot_verify(source,name)||tmc2_verify(dest))return -1;
 u64 ignored,needed=0;
 if(transfer_tree(source,dest,snap.parent,INODES,0,&ignored,&needed,0)||transfer_tree(source,dest,snap.root,MAP,0,&ignored,&needed,0))return -1;
 if(blocks)*blocks=needed;return 0;
}
int tmc2_import_snapshot(Tmc2*dest,Tmc2*source,const char*name){
 T2Entry snap;if(tmc2_import_check(dest,source,name,0)||snapshot_get(source,name,&snap)||begin(dest))return -1;
 T2Super next=dest->super;u64 copied=0;
 /* nfresh is a per-operation allocation guard, NOT a rollback/free list. The
  * sparse used bitmap retains every staged block across the bounded batches.
  * Any failure poisons this mount; remount reconstructs only durable roots. */
 if(transfer_tree(source,dest,snap.parent,INODES,0,&next.live,&copied,1)||transfer_tree(source,dest,snap.root,MAP,0,&next.control,&copied,1))return fail(dest);
 next.next_inode=MAX(next.next_inode,snap.size);
 return publish(dest,next);
}
