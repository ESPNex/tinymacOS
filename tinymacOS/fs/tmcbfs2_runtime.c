/* SPDX-License-Identifier: MIT
 * TMCBFS2 mounted user-volume backend. Never formats unknown/corrupt media.
 * Separate inode handles prevent collision with the bootfs >=1024 namespace.
 */
#include "../kernel/k.h"
#include "disklayout.h"
#include "tmcbfs2.h"
#include "prefs_plist.h"
#include "fs2_runtime.h"
#include "fs2_volume.h"
#include "home_name.h"
#include "tmcbfs2_seed.h"
int fs2_active;
static Tmc2 fs;
/* The image manager pins source inodes, including handles opened before
 * attach. Weak linkage keeps standalone filesystem fixtures independent. */
extern int image_native_busy(u64) __attribute__((weak));
extern int image_native_attached(void) __attribute__((weak));
extern int image_source_snapshot_busy(u64) __attribute__((weak));
static int image_pinned(u64 id){return image_native_busy&&image_native_busy(id);}

static char home_name[48]="User";
/* No migration at mount. Bind only to a verified account's existing inode. */
static void home_refresh(void){
 strset(home_name,"User",48);T2Entry users,old,named;u8 record[512];char name[48];
 if(tmc2_lookup(&fs,1,"Users",&users)||users.kind!=T2_DIR||!tmc2_lookup(&fs,users.key,"User",&old))return;
 for(int slot=0;slot<2;slot++)if(!tmc2_account(&fs,slot,record,0)&&home_account_name(record,name)){
  if(!tmc2_lookup(&fs,users.key,name,&named)&&named.kind==T2_DIR)strset(home_name,name,48);
  break;
 }
 erase(record,sizeof record);
}
int fs2_home_bind(const char*name){
 if(!home_name_valid(name))return -3;T2Entry users,old,named;
 if(tmc2_lookup(&fs,1,"Users",&users)||users.kind!=T2_DIR)return -1;
 int have_old=!tmc2_lookup(&fs,users.key,"User",&old),have_named=!tmc2_lookup(&fs,users.key,name,&named);
 if(have_named){if(named.kind!=T2_DIR||(have_old&&named.key!=old.key))return -2;strset(home_name,name,48);return 0;}
 if(!have_old||old.kind!=T2_DIR||old.reserved&1||users.reserved&1)return -1;
 /* One COW metadata publication; file inodes/extents, open handles, account
  * records and historical snapshot roots are untouched. No copy or overwrite. */
 if(tmc2_move(&fs,old.key,users.key,name))return -1;
 strset(home_name,name,48);fsrev++;return 0;
}
static struct{int disk;u64 base,sectors;} medium={.disk=-1};
extern int volume_extent(u64*,u64*);
static int disk_read(void*x,u64 block,void*p){(void)x;return fs2_volume_read(medium.disk,medium.base,medium.sectors,block,p);}
static int disk_write(void*x,u64 block,const void*p){(void)x;return fs2_volume_write(medium.disk,medium.base,medium.sectors,block,p);}
static int disk_flush(void*x){(void)x;return sata_flush(medium.disk);}
/* Bitmap pages come from physical memory, not the fixed-size small-object heap.
 * Every other allocation is <= one tree page + a single temporary entry. */
static void*memory_alloc(void*x,size_t n){(void)x;return n==4096?(void*)page_get():kmalloc(n);}
static void memory_free(void*x,void*p){(void)x;if(!p)return;if(kmalloc_owns(p))kfree(p);else page_put((u64)p);}
static const char*alias(const char*p){const char*k="/Volumes/Macintosh HD";size_t n=strlen(k);return strlen(p)>=n&&!memcmp(p,k,n)&&(!p[n]||p[n]=='/')?(p[n]?p+n:"/"):p;}
static int normalize(const char*path,char out[PATHLEN]){
 if(!path)return -1;path=alias(path);if(*path!='/')return -1;size_t used=0;
 while(*path){while(*path=='/')path++;if(!*path)break;const char*start=path;while(*path&&*path!='/')path++;size_t n=path-start;
  if(n==1&&*start=='.')continue;
  if(n==2&&start[0]=='.'&&start[1]=='.'){while(used&&out[used-1]!='/')used--;if(used)used--;continue;}
  if(n>=T2_NAME||used+1+n>=PATHLEN)return -1;out[used++]='/';memcpy(out+used,start,n);used+=n;
 }
 if(!used)out[used++]='/';out[used]=0;
 /* Keep a canonical compatibility path for protection checks, without adding
  * a second directory to /Users. Snapshot paths deliberately bypass this. */
 if(!eq(home_name,"User")){
  char actual[64]="/Users/";append(actual,home_name,sizeof actual);size_t n=strlen(actual);
  if(used>=n&&!memcmp(out,actual,n)&&(!out[n]||out[n]=='/')){
   char suffix[PATHLEN];strset(suffix,out+n,PATHLEN);strset(out,HOME,PATHLEN);append(out,suffix,PATHLEN);
  }
 }
 return 0;
}
static int resolve(const char*path,T2Entry*out){
 char q[PATHLEN];if(normalize(path,q))return -1;u64 id=1;char*p=q;
 while(*p){while(*p=='/')p++;if(!*p)break;char*s=p;while(*p&&*p!='/')p++;char saved=*p;*p=0;
  T2Entry e;const char*component=s;
  if(s==q+7&&!memcmp(q,"/Users/",7)&&eq(s,"User"))component=home_name;
  int bad=tmc2_lookup(&fs,id,component,&e);*p=saved;if(bad)return -1;id=e.key;
 }
 return tmc2_stat(&fs,id,out);
}
/* Snapshot views never fall through to the LIVE bootfs or namespace. FD reads
 * retain a catalogue ID, not just an inode (which may exist in both versions).
 * Deleting a snapshot invalidates its handles instead of exposing live bytes. */
static int snapshot_path(const char*q){return !memcmp(q,"/.snapshots",11)&&(!q[11]||q[11]=='/');}
static int view_resolve(const char*q,u64*snapshot,T2Entry*out){
 if(!snapshot_path(q))return 0;if(!q[11])return -1;
 const char*p=q+12,*start=p;while(*p&&*p!='/')p++;size_t n=p-start;
 if(!n||n>=T2_NAME)return -1;char name[T2_NAME];memcpy(name,start,n);name[n]=0;
 T2Entry snap;if(tmc2_snapshot_find(&fs,name,&snap))return -1;*snapshot=snap.key;u64 id=1;
 int at_users=0;
 while(*p){p++;start=p;while(*p&&*p!='/')p++;n=p-start;if(!n||n>=T2_NAME)return -1;
  memcpy(name,start,n);name[n]=0;T2Entry child;int bad=tmc2_view_lookup(&fs,*snapshot,id,name,&child);
  /* Old HOME aliases in a snapshot bind ONLY to that snapshot's own account,
   * never to the live account/name or an arbitrary directory. */
  if(bad&&at_users&&eq(name,"User")){
   u8 record[512];char actual[48];
   for(unsigned slot=0;slot<2;slot++)if(!tmc2_view_account(&fs,*snapshot,slot,record)&&home_account_name(record,actual)){
    bad=tmc2_view_lookup(&fs,*snapshot,id,actual,&child);break;
   }
   erase(record,sizeof record);
  }
  if(bad)return -1;at_users=id==1&&eq(name,"Users");id=child.key;
 }
 return tmc2_view_stat(&fs,*snapshot,id,out)?-1:1;
}
static int split(const char*path,char name[T2_NAME],T2Entry*parent){
 char q[PATHLEN];if(normalize(path,q)||eq(q,"/"))return -1;size_t n=strlen(q),at=n;while(at&&q[at-1]!='/')at--;
 strset(name,q+at,T2_NAME);q[at?at-1:0]=0;return resolve(*q?q:"/",parent)||parent->kind!=T2_DIR?-1:0;
}
static int protected(const char*path){
 char q[PATHLEN];if(normalize(path,q))return 1;
 if(snapshot_path(q)||bootfs_lookup(q)!=-2||eq(q,"/")||eq(q,"/Users")||eq(q,HOME)||eq(q,"/Library")||eq(q,"/Volumes"))return 1;
 return eq(q,HOME "/Library/Preferences/com.tinymacos.appearance.plist")||eq(q,"/Library/Preferences/com.tinymacos.system.plist");
}
static void entry(const T2Entry*e,Dirent*d){*d=(Dirent){e->key,e->parent,e->kind==T2_DIR,!!(e->reserved&1),e->size,{0}};strset(d->name,e->name,sizeof d->name);}
int fs2_stat(const char*path,Dirent*out){
 char q[PATHLEN];if(!out||normalize(path,q))return -1;
 if(eq(q,"/.snapshots")){*out=(Dirent){.dir=1,.ro=1};strset(out->name,".snapshots",48);return 0;}
 u64 snapshot;T2Entry frozen;int view=view_resolve(q,&snapshot,&frozen);if(view){if(view<0)return -1;entry(&frozen,out);out->ro=1;return 0;}
 int v=bootfs_lookup(q);if(v!=-2)return bootfs_stat(v,out);
 T2Entry e;if(resolve(q,&e))return -1;entry(&e,out);out->ro=protected(q)||!!(e.reserved&1);return 0;
}
int fs2_list(const char*path,Dirent*out,int cap,int skip){
 char q[PATHLEN];if(cap<0||skip<0||(!out&&cap)||normalize(path,q))return -1;
 if(eq(q,"/.snapshots")){u64 cookie=0;T2Entry e;int n=0,r=0;while(n<cap&&(r=tmc2_snapshots(&fs,&cookie,&e))==1){if(skip){skip--;continue;}out[n]=(Dirent){.id=e.key,.dir=1,.ro=1};strset(out[n++].name,e.name,48);}return r<0?-1:n;}
 u64 snapshot;T2Entry frozen;int view=view_resolve(q,&snapshot,&frozen);
 if(view){if(view<0||frozen.kind!=T2_DIR)return -1;u64 parent=frozen.key,cookie=0;int n=0,r=0;while(n<cap&&(r=tmc2_view_readdir(&fs,snapshot,parent,&cookie,&frozen))==1){if(skip){skip--;continue;}entry(&frozen,out+n);out[n++].ro=1;}return r<0?-1:n;}
 int v=bootfs_lookup(q);if(v!=-2)return bootfs_list(v,out,cap,skip);
 T2Entry e;if(resolve(q,&e)||e.kind!=T2_DIR)return -1;int n=0;
 if(e.key==1){if(skip)skip--;else if(n<cap){out[n]=(Dirent){.dir=1,.ro=1};strset(out[n++].name,".snapshots",48);}Dirent built[12];int k=bootfs_list(1024,built,12,0);for(int i=0;i<k&&n<cap;i++){if(skip)skip--;else out[n++]=built[i];}}
 u64 parent=e.key,cookie=0;int r;
 while(n<cap&&(r=tmc2_readdir(&fs,parent,&cookie,&e))==1){
  if(e.parent==1){char full[64]="/";append(full,e.name,sizeof full);if(bootfs_lookup(full)!=-2)continue;}
  if(skip)skip--;else entry(&e,out+n++);
 }
 return fs.faulted?-1:n;
}
int fs2_create(const char*path,int dir){
 char name[T2_NAME];T2Entry p;u64 id;char q[PATHLEN];if(normalize(path,q)||protected(q)||split(q,name,&p)||(p.reserved&1)||tmc2_create(&fs,p.key,name,dir?T2_DIR:T2_FILE,&id))return -1;
 usednodes++;fsrev++;return 0;
}
static int validate_remove(u64 id,int recursive,unsigned depth){
 if(depth>=PATHLEN/2)return -REMOVE_BAD_PATH;T2Entry e;
 if(tmc2_stat(&fs,id,&e))return -REMOVE_IO;
 const char*ro[]={"/","/Users",HOME,"/Library","/Volumes",HOME "/Library/Preferences/com.tinymacos.appearance.plist","/Library/Preferences/com.tinymacos.system.plist"};
 for(unsigned i=0;i<COUNT(ro);i++){T2Entry p;if(!resolve(ro[i],&p)&&p.key==id)return -REMOVE_READ_ONLY;}
 if((e.reserved&1)||image_pinned(id))return -REMOVE_READ_ONLY;
 if(e.kind==T2_DIR){u64 cookie=0;int r;T2Entry child;while((r=tmc2_readdir(&fs,id,&cookie,&child))==1){if(!recursive)return -REMOVE_NOT_EMPTY;int bad=validate_remove(child.key,recursive,depth+1);if(bad)return bad;}if(r<0)return -REMOVE_IO;}
 return 0;
}
static int remove_inode(u64 id,int recursive,unsigned depth){
 if(depth>=PATHLEN/2)return -REMOVE_BAD_PATH;T2Entry e;if(tmc2_stat(&fs,id,&e))return -REMOVE_IO;
 if(e.kind==T2_DIR){u64 cookie=0;T2Entry c;int r;while((r=tmc2_readdir(&fs,id,&cookie,&c))==1){if(!recursive)return -REMOVE_NOT_EMPTY;int bad=remove_inode(c.key,recursive,depth+1);if(bad)return bad;}if(r<0)return -REMOVE_IO;}
 if(tmc2_remove(&fs,id))return -REMOVE_IO;if(usednodes)usednodes--;return 0;
}
int fs2_remove(const char*path,unsigned flags){
 if(flags&~7u)return -REMOVE_BAD_PATH;if(protected(path))return -REMOVE_READ_ONLY;T2Entry e;if(resolve(path,&e))return -REMOVE_BAD_PATH;
 if(((flags&REMOVE_DIR_ONLY)&&e.kind!=T2_DIR)||((flags&REMOVE_FILE_ONLY)&&e.kind!=T2_FILE))return -REMOVE_WRONG_TYPE;
 int r=validate_remove(e.key,flags&REMOVE_RECURSIVE,0);if(r)return r;
 r=remove_inode(e.key,flags&REMOVE_RECURSIVE,0);if(!r)fsrev++;return r;
}
int fs2_move(const char*from,const char*to){
 if(protected(from)||protected(to))return -1;T2Entry e,p;char name[T2_NAME];if(resolve(from,&e)||split(to,name,&p)||(e.reserved&1)||(p.reserved&1)||tmc2_move(&fs,e.key,p.key,name))return -1;fsrev++;return 0;
}
int fs2_open(Process*p,const char*path,int write){
 char q[PATHLEN];if(normalize(path,q))return -1;
 u64 snapshot;T2Entry frozen;int view=view_resolve(q,&snapshot,&frozen);
 if(view){if(view<0||write||frozen.kind!=T2_FILE)return -1;for(int d=0;d<12;d++)if(!p->fd[d].id){p->fd[d]=(FD){.id=-1,.backend=3,.inode=frozen.key,.snapshot=snapshot};return d;}return -1;}
 int v=bootfs_lookup(q);if(v!=-2)return bootfs_open(p,v,write);
 T2Entry e;if(resolve(q,&e)||e.kind!=T2_FILE||(write&&(protected(q)||(e.reserved&1)||image_pinned(e.key))))return -1;
 for(int d=0;d<12;d++)if(!p->fd[d].id){p->fd[d]=(FD){.id=-1,.write=!!write,.backend=2,.inode=e.key};return d;}return -1;
}
int fs2_close(Process*p,int fd){if(fd<0||fd>=12||!p->fd[fd].id)return -1;memset(p->fd+fd,0,sizeof(FD));return 0;}
int fs2_read(Process*p,int fd,void*out,int count){
 if(fd<0||fd>=12||count<0||!p->fd[fd].id)return -1;FD*f=p->fd+fd;if(f->backend==3){i64 n=tmc2_view_read(&fs,f->snapshot,f->inode,f->at,out,count);if(n>=0)f->at+=n;return n;}
 if(f->backend!=2)return bootfs_read(f,out,count);
 i64 n=tmc2_read(&fs,f->inode,f->at,out,count);if(n>=0)f->at+=n;return n;
}
int fs2_write(Process*p,int fd,const void*in,int count){
 if(fd<0||fd>=12||count<0)return -1;FD*f=p->fd+fd;if(f->backend!=2||!f->id||!f->write||image_pinned(f->inode))return -1;
 i64 n=tmc2_write(&fs,f->inode,f->at,in,count);if(n>=0){if(tmc2_truncate(&fs,f->inode,f->at+n))return -1;f->at+=n;fsrev++;}return n;
}
int fs2_seek(Process*p,int fd,u64 offset){
 if(fd<0||fd>=12||!p->fd[fd].id)return -1;FD*f=p->fd+fd;
 if(f->backend==3){T2Entry e;if(offset>T2_MAX_BYTES||tmc2_view_stat(&fs,f->snapshot,f->inode,&e))return -1;}
 else if(f->backend==2){if(offset>T2_MAX_BYTES)return -1;T2Entry e;if(tmc2_stat(&fs,f->inode,&e))return -1;}
 else{Dirent e;if(bootfs_stat(f->id,&e)||offset>e.size)return -1;}
 f->at=offset;return 0;
}
int fs2_account(int slot,void*p,int write){if(slot<0)return -1;int r=tmc2_account(&fs,slot,p,write);if(!r&&write)home_refresh();return r;}
static int write_path(const char*path,const void*p,size_t n){T2Entry e;if(resolve(path,&e)||image_pinned(e.key)||tmc2_write(&fs,e.key,0,p,n)!=(i64)n||tmc2_truncate(&fs,e.key,n))return -1;return 0;}
int fs2_prefs(void){u8 values[6]={theme,wall,dock,reduce_motion,opaque,dark};char xml[1024];int n=prefs_plist_write(xml,sizeof xml,values);return n<0?-1:write_path(HOME "/Library/Preferences/com.tinymacos.appearance.plist",xml,n);}
static void load_prefs(void){T2Entry e;char xml[1024]={0};u8 conf[6];if(!resolve(HOME "/Library/Preferences/com.tinymacos.appearance.plist",&e)&&e.size<sizeof xml&&tmc2_read(&fs,e.key,0,xml,e.size)==(i64)e.size&&prefs_plist_read(xml,conf)){theme=conf[0]%NTHEME;wall=conf[1]%NWALL;dock=conf[2]&1;reduce_motion=conf[3]&1;opaque=conf[4]&1;dark=conf[5]&1;}}
int fs2_snap_create(const char*name){if(!name||strlen(name)>27||tmc2_snapshot(&fs,name,0))return -1;fsrev++;return 0;}
int fs2_snap_page(void*out,int cap,int skip){if(cap<0||skip<0||(!out&&cap))return -1;SnapInfo*s=out;T2Entry e;u64 cookie=0;int n=0,r=0;while(n<cap&&(r=tmc2_snapshots(&fs,&cookie,&e))==1){if(skip){skip--;continue;}strset(s[n].name,e.name,sizeof s[n].name);s[n].date=0;s[n].id=e.key;n++;}return r<0?-1:n;}
int fs2_snap_list(void*out,int cap){return fs2_snap_page(out,cap,0);}
int fs2_local_backup(int op){
 if(op==0){T2Entry e;return !tmc2_snapshot_find(&fs,"Copia locale",&e);}
 if(op!=1)return -1;T2Entry held;if(image_source_snapshot_busy&&!tmc2_snapshot_find(&fs,"Copia locale",&held)&&image_source_snapshot_busy(held.key))return -1;int r=tmc2_checkpoint(&fs,"Copia locale");if(!r)fsrev++;return r;
}
int fs2_snap_restore(const char*name){if(image_native_attached&&image_native_attached())return -1;if(tmc2_snapshot_restore(&fs,name))return -1;fsrev++;i64 count=tmc2_inode_count(&fs);if(count<0)return -1;usednodes=MIN((u64)count,0xffffffffu);home_refresh();load_prefs();return 0;}
int fs2_snap_delete(const char*name){T2Entry held;if(image_source_snapshot_busy&&!tmc2_snapshot_find(&fs,name,&held)&&image_source_snapshot_busy(held.key))return -1;if(tmc2_snapshot_delete(&fs,name)||tmc2_collect(&fs))return -1;fsrev++;return 0;}
int fs2_snap_verify(const char*name){return tmc2_snapshot_verify(&fs,name);}
int fs2_info(FsInfo*out){if(!out)return -1;u64 cookie=0;T2Entry e;u32 snaps=0;int r;while((r=tmc2_snapshots(&fs,&cookie,&e))==1)snaps++;if(r<0)return -1;*out=(FsInfo){fs.io.blocks*T2_BLOCK,tmc2_used_blocks(&fs)*T2_BLOCK,snaps,(u32)fs.super.generation};return 0;}
int fs2_touch(const char*path){T2Entry e;return resolve(path,&e)?fs2_create(path,0):protected(path)?-1:0;}
int fs2_enc(int op,const char*password,EncInfo*out){(void)password;if(op||!out)return -1;int encrypted=medium.disk<0?0:fs2_volume_encrypted(medium.disk,medium.base,medium.sectors);if(encrypted<0)return -1;*out=(EncInfo){.state=encrypted,.locked=encrypted&&fs2_volume_locked(medium.disk)};return 0;}
/* Sparse RAM TMCFS2 for live, Recovery and the locked-volume login shell.
 * Explicit volatile creation, NEVER a fallback that initializes a disk. */
#define LIVE_BLOCKS 2048u
static u64 live_pages[LIVE_BLOCKS][4];
static int live_read(void*x,u64 b,void*p){(void)x;if(b>=LIVE_BLOCKS)return -1;for(unsigned i=0;i<4;i++){if(live_pages[b][i])memcpy((u8*)p+i*4096,(void*)live_pages[b][i],4096);else memset((u8*)p+i*4096,0,4096);}return 0;}
static int live_write(void*x,u64 b,const void*p){(void)x;if(b>=LIVE_BLOCKS)return -1;for(unsigned i=0;i<4;i++)if(!live_pages[b][i]){live_pages[b][i]=page_get();if(!live_pages[b][i])return -1;memset((void*)live_pages[b][i],0,4096);}for(unsigned i=0;i<4;i++)memcpy((void*)live_pages[b][i],(const u8*)p+i*4096,4096);return 0;}
static int live_flush(void*x){(void)x;return 0;}
static int live_init(void){
 T2Io io={0,LIVE_BLOCKS,live_read,live_write,live_flush,memory_alloc,memory_free};if(tmc2_format(&fs,io))return 0;
 static const T2Seed seed[]={
 {0,T2_DIR,"Users"},{1,T2_DIR,"User"},{2,T2_DIR,"Desktop"},{2,T2_DIR,"Documents"},
 {2,T2_DIR,"Downloads"},{2,T2_DIR,"Pictures"},{0,T2_DIR,"Volumes"},
 {0,T2_DIR,"Library"},{8,T2_DIR,"Application Support"},{8,T2_DIR,"Preferences"},
 {2,T2_DIR,"Library"},{11,T2_DIR,"Preferences"},{11,T2_DIR,"Application Support"},
 {12,T2_FILE,"com.tinymacos.appearance.plist"},{10,T2_FILE,"com.tinymacos.system.plist"},
 {2,T2_FILE,"Welcome.txt"}};
 if(tmc2_reset(&fs,seed,COUNT(seed)))return 0;
 strset(home_name,"User",48);fs2_active=1;persistent=0;theme=7;wall=12;dark=darktheme(theme);fsrev=fs.super.generation;
 const char*welcome="Benvenuti in tinymacOS.\n\nQuesta sessione usa TMCFS2 in RAM. Il formato legacy non è supportato.\n";
 const char*system="<?xml version=\"1.0\"?><plist version=\"1.0\"><dict><key>ComputerName</key><string>Tramontana</string></dict></plist>\n";
 if(write_path(HOME "/Welcome.txt",welcome,strlen(welcome))||write_path("/Library/Preferences/com.tinymacos.system.plist",system,strlen(system))||fs2_prefs())return 0;
 i64 count=tmc2_inode_count(&fs);if(count<0)return 0;usednodes=count;
 device_add("TMCFS2 RAM", "block", &fs);logstr("[fs2] volatile TMCFS2; legacy support removed\n");return 1;
}
int fs2_snapshot(void*p,u32 bytes){return fs2_active?tmc2_seed_export(&fs,p,bytes):-1;}
int fs2_mount(void){
 if(fs2_active)tmc2_close(&fs);fs2_active=0;persistent=0;
 for(unsigned b=0;b<LIVE_BLOCKS;b++)for(unsigned i=0;i<4;i++)if(live_pages[b][i]){page_put(live_pages[b][i]);live_pages[b][i]=0;}
 if(boot.recovery||boot.install_len){medium.disk=-1;return live_init();}
 Block block;if(!volume_mount(&block)||volume_extent(&medium.base,&medium.sectors))panic("TMCFS2 data volume missing; disks untouched",0,0);
 medium.disk=volume_current();u8 h[512];int encrypted=fs2_volume_encrypted(medium.disk,medium.base,medium.sectors);
 if(encrypted<0)panic("TMCFS2 volume IO needs recovery",0,0);
 if(encrypted&&fs2_volume_locked(medium.disk))return live_init();
 int recognized=encrypted;
 for(unsigned i=0;i<2;i++)if(!sata_io(medium.disk,medium.base+i*32,1,h,0)&&!memcmp(h,"TMCBFS2",8))recognized=1;
 if(!recognized)panic("Native filesystem unsupported; TMCFS2 required; disk untouched",0,0);
 T2Io io={0,fs2_volume_blocks(medium.disk,medium.base,medium.sectors),disk_read,disk_write,disk_flush,memory_alloc,memory_free};
 if(tmc2_mount(&fs,io))panic("TMCFS2 metadata needs recovery; disk untouched",0,0);
 fs2_active=persistent=1;wall=12;theme=7;dark=darktheme(theme);fsrev=fs.super.generation;
 i64 count=tmc2_inode_count(&fs);if(count<0)panic("TMCFS2 inode catalog invalid",0,0);usednodes=MIN((u64)count,0xffffffffu);
 home_refresh();load_prefs();device_add("TMCFS2 CoW", "block", &fs);boot_logstr("[fs2] mounted full data extent: blocks=");boot_lognum(fs.io.blocks);boot_logstr("\n");return 1;
}

i64 fs2_write_at(Process*p,int fd,u64 off,const void*data,size_t bytes){
 if(!fs2_active||fd<0||fd>=12||!p->fd[fd].id||p->fd[fd].backend!=2||!p->fd[fd].write||image_pinned(p->fd[fd].inode))return -1;
 i64 n=tmc2_write(&fs,p->fd[fd].inode,off,data,bytes);if(n>=0)fsrev++;return n;
}
