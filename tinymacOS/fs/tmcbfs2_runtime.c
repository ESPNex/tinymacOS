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
int fs2_active;
static Tmc2 fs;
static struct{int disk;u64 base,sectors;} medium;
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
 if(!used)out[used++]='/';out[used]=0;return 0;
}
static int resolve(const char*path,T2Entry*out){
 char q[PATHLEN];if(normalize(path,q))return -1;u64 id=1;char*p=q;
 while(*p){while(*p=='/')p++;if(!*p)break;char*s=p;while(*p&&*p!='/')p++;char saved=*p;*p=0;
  T2Entry e;int bad=tmc2_lookup(&fs,id,s,&e);*p=saved;if(bad)return -1;id=e.key;
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
 while(*p){p++;start=p;while(*p&&*p!='/')p++;n=p-start;if(!n||n>=T2_NAME)return -1;
  memcpy(name,start,n);name[n]=0;T2Entry child;if(tmc2_view_lookup(&fs,*snapshot,id,name,&child))return -1;id=child.key;
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
 if(e.reserved&1)return -REMOVE_READ_ONLY;
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
 T2Entry e;if(resolve(q,&e)||e.kind!=T2_FILE||(write&&(protected(q)||(e.reserved&1))))return -1;
 for(int d=0;d<12;d++)if(!p->fd[d].id){p->fd[d]=(FD){.id=-1,.write=!!write,.backend=2,.inode=e.key};return d;}return -1;
}
int fs2_close(Process*p,int fd){if(fd<0||fd>=12||!p->fd[fd].id)return -1;memset(p->fd+fd,0,sizeof(FD));return 0;}
int fs2_read(Process*p,int fd,void*out,int count){
 if(fd<0||fd>=12||count<0||!p->fd[fd].id)return -1;FD*f=p->fd+fd;if(f->backend==3){i64 n=tmc2_view_read(&fs,f->snapshot,f->inode,f->at,out,count);if(n>=0)f->at+=n;return n;}
 if(f->backend!=2)return bootfs_read(f,out,count);
 i64 n=tmc2_read(&fs,f->inode,f->at,out,count);if(n>=0)f->at+=n;return n;
}
int fs2_write(Process*p,int fd,const void*in,int count){
 if(fd<0||fd>=12||count<0)return -1;FD*f=p->fd+fd;if(f->backend!=2||!f->id||!f->write)return -1;
 i64 n=tmc2_write(&fs,f->inode,f->at,in,count);if(n>=0){if(tmc2_truncate(&fs,f->inode,f->at+n))return -1;f->at+=n;fsrev++;}return n;
}
int fs2_seek(Process*p,int fd,u64 offset){
 if(fd<0||fd>=12||!p->fd[fd].id)return -1;FD*f=p->fd+fd;
 if(f->backend==3){T2Entry e;if(offset>T2_MAX_BYTES||tmc2_view_stat(&fs,f->snapshot,f->inode,&e))return -1;}
 else if(f->backend==2){if(offset>T2_MAX_BYTES)return -1;T2Entry e;if(tmc2_stat(&fs,f->inode,&e))return -1;}
 else{Dirent e;if(bootfs_stat(f->id,&e)||offset>e.size)return -1;}
 f->at=offset;return 0;
}
int fs2_account(int slot,void*p,int write){return slot<0?-1:tmc2_account(&fs,slot,p,write);}
static int write_path(const char*path,const void*p,size_t n){T2Entry e;if(resolve(path,&e)||tmc2_write(&fs,e.key,0,p,n)!=(i64)n||tmc2_truncate(&fs,e.key,n))return -1;return 0;}
int fs2_prefs(void){u8 values[6]={theme,wall,dock,reduce_motion,opaque,dark};char xml[1024];int n=prefs_plist_write(xml,sizeof xml,values);return n<0?-1:write_path(HOME "/Library/Preferences/com.tinymacos.appearance.plist",xml,n);}
static void load_prefs(void){T2Entry e;char xml[1024]={0};u8 conf[6];if(!resolve(HOME "/Library/Preferences/com.tinymacos.appearance.plist",&e)&&e.size<sizeof xml&&tmc2_read(&fs,e.key,0,xml,e.size)==(i64)e.size&&prefs_plist_read(xml,conf)){theme=conf[0]%NTHEME;wall=conf[1]%NWALL;dock=conf[2]&1;reduce_motion=conf[3]&1;opaque=conf[4]&1;dark=conf[5]&1;}}
int fs2_snap_create(const char*name){if(!name||strlen(name)>27||tmc2_snapshot(&fs,name,0))return -1;fsrev++;return 0;}
int fs2_snap_page(void*out,int cap,int skip){if(cap<0||skip<0||(!out&&cap))return -1;SnapInfo*s=out;T2Entry e;u64 cookie=0;int n=0,r=0;while(n<cap&&(r=tmc2_snapshots(&fs,&cookie,&e))==1){if(skip){skip--;continue;}strset(s[n].name,e.name,sizeof s[n].name);s[n].date=0;s[n].id=e.key;n++;}return r<0?-1:n;}
int fs2_snap_list(void*out,int cap){return fs2_snap_page(out,cap,0);}
int fs2_local_backup(int op){
 if(op==0){T2Entry e;return !tmc2_snapshot_find(&fs,"Copia locale",&e);}
 if(op!=1)return -1;int r=tmc2_checkpoint(&fs,"Copia locale");if(!r)fsrev++;return r;
}
int fs2_snap_restore(const char*name){if(tmc2_snapshot_restore(&fs,name))return -1;fsrev++;i64 count=tmc2_inode_count(&fs);if(count<0)return -1;usednodes=MIN((u64)count,0xffffffffu);load_prefs();return 0;}
int fs2_snap_delete(const char*name){if(tmc2_snapshot_delete(&fs,name)||tmc2_collect(&fs))return -1;fsrev++;return 0;}
int fs2_snap_verify(const char*name){return tmc2_snapshot_verify(&fs,name);}
int fs2_info(FsInfo*out){if(!out)return -1;u64 cookie=0;T2Entry e;u32 snaps=0;int r;while((r=tmc2_snapshots(&fs,&cookie,&e))==1)snaps++;if(r<0)return -1;*out=(FsInfo){fs.io.blocks*T2_BLOCK,tmc2_used_blocks(&fs)*T2_BLOCK,snaps,(u32)fs.super.generation};return 0;}
int fs2_touch(const char*path){T2Entry e;return resolve(path,&e)?fs2_create(path,0):protected(path)?-1:0;}
int fs2_enc(int op,const char*password,EncInfo*out){(void)password;if(op||!out)return -1;int encrypted=fs2_volume_encrypted(medium.disk,medium.base,medium.sectors);if(encrypted<0)return -1;*out=(EncInfo){.state=encrypted,.locked=encrypted&&fs2_volume_locked(medium.disk)};return 0;}
int fs2_mount(void){
 fs2_active=0;if(boot.recovery||boot.install_len)return 0;Block b;if(!volume_mount(&b)||volume_extent(&medium.base,&medium.sectors))return 0;
 medium.disk=volume_current();u8 h[512];int encrypted=fs2_volume_encrypted(medium.disk,medium.base,medium.sectors);
 if(encrypted<0)panic("TMCBFS2 volume IO needs recovery",0,0);if(encrypted&&fs2_volume_locked(medium.disk))return 0;int recognized=encrypted;
 for(unsigned i=0;i<2;i++)if(!sata_io(medium.disk,medium.base+i*32,1,h,0)&&!memcmp(h,"TMCBFS2",8))recognized=1;
 if(!recognized)return 0;T2Io io={0,fs2_volume_blocks(medium.disk,medium.base,medium.sectors),disk_read,disk_write,disk_flush,memory_alloc,memory_free};
 if(tmc2_mount(&fs,io))panic("TMCBFS2 metadata needs recovery; disk untouched",0,0);
 fs2_active=persistent=1;wall=12;theme=7;dark=darktheme(theme);usednodes=0;fsrev=fs.super.generation;
 i64 count=tmc2_inode_count(&fs);if(count<0)panic("TMCBFS2 inode catalog invalid",0,0);usednodes=MIN((u64)count,0xffffffffu);
 load_prefs();device_add("TMCBFS2 scalable CoW", "block", &fs);boot_logstr("[fs2] mounted full data extent: blocks=");boot_lognum(fs.io.blocks);boot_logstr("\n");return 1;
}

/* Legacy vault dispatch must not inspect fixed v1 offsets in a mounted v2
 * volume; encrypted v3 envelopes are dispatched separately before legacy. */
int fs2_plain_volume(int disk){return fs2_active&&disk==medium.disk;}
