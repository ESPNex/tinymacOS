/* Exercise the actual kernel-facing backend without privileged IRQ ops. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../fs/tmcbfs2_runtime.c"
u32 persistent,fsrev,usednodes,theme,wall,dock,reduce_motion,opaque,dark;
struct Boot boot;
static u8*disk;
static int read_page(void*x,u64 b,void*p){(void)x;assert(b<4096);memcpy(p,disk+b*T2_BLOCK,T2_BLOCK);return 0;}
static int write_page(void*x,u64 b,const void*p){(void)x;assert(b<4096);memcpy(disk+b*T2_BLOCK,p,T2_BLOCK);return 0;}
static int sync_page(void*x){(void)x;return 0;}
static void*allocate(void*x,size_t n){(void)x;return malloc(n);}
static void deallocate(void*x,void*p){(void)x;free(p);}
int bootfs_lookup(const char*p){return !strncmp(p,"/System",7)?1200:-2;}
int bootfs_stat(int v,Dirent*d){if(v!=1200)return -1;*d=(Dirent){.id=1200,.size=6,.ro=1};return 0;}
int bootfs_list(int v,Dirent*d,int cap,int skip){(void)v;(void)d;(void)cap;(void)skip;return 0;}
int bootfs_open(Process*p,int id,int write){if(id!=1200||write)return -1;for(int j=0;j<12;j++)if(!p->fd[j].id){p->fd[j]=(FD){.id=id};return j;}return -1;}
int bootfs_read(FD*f,void*p,int count){if(f->id!=1200)return -1;int n=MIN((u64)count,6-f->at);memcpy(p,"SYSTEM"+f->at,n);f->at+=n;return n;}
static u64 node(u64 parent,const char*name,int kind){u64 id;assert(!tmc2_create(&fs,parent,name,kind,&id));return id;}
int main(void){
 disk=calloc(4096,T2_BLOCK);assert(disk);assert(!tmc2_format(&fs,(T2Io){0,4096,read_page,write_page,sync_page,allocate,deallocate}));fs2_active=1;
 u64 users=node(1,"Users",T2_DIR),home=node(users,"User",T2_DIR);node(home,"Documents",T2_DIR);u64 library=node(home,"Library",T2_DIR);node(library,"Preferences",T2_DIR);
 Process p={0};static u8 in[65536],out[65536];for(unsigned i=0;i<sizeof in;i++)in[i]=(i*19+23)%251;
 assert(!fs2_create(DOCS "/large",0));int fd=fs2_open(&p,DOCS "/large",1);assert(fd>=0&&p.fd[fd].backend==2);assert(fs2_write(&p,fd,in,sizeof in)==sizeof in);assert(!fs2_close(&p,fd));
 Dirent e;assert(!fs2_stat(DOCS "/large",&e)&&e.size==sizeof in);fd=fs2_open(&p,DOCS "/large",0);assert(fs2_read(&p,fd,out,sizeof out)==sizeof out&&!memcmp(in,out,sizeof in));assert(!fs2_close(&p,fd));
 for(unsigned i=0;i<1100;i++){char path[PATHLEN];snprintf(path,sizeof path,DOCS "/item-%04u",i);assert(!fs2_create(path,0));}
 /* Independent enumeration, not just direct lookup: readdir must retain the
  * directory ID while its result structure is overwritten at every entry. */
 static Dirent page[37];int total=0,got;u64 previous=0;
 while((got=fs2_list(DOCS,page,37,total))>0){for(int j=0;j<got;j++){assert(page[j].id>previous);previous=page[j].id;}total+=got;assert(total<=1102);}
 assert(got==0&&total==1101);assert(fs2_list(DOCS,page,37,total)==0);
 assert(!fs2_stat(DOCS "/item-1099",&e)&&e.id>1024);assert(tmc2_inode_count(&fs)>1100);fd=fs2_open(&p,DOCS "/item-1099",1);assert(fd>=0&&p.fd[fd].id==-1);assert(fs2_write(&p,fd,"INODE",5)==5);assert(!fs2_close(&p,fd));
 fd=fs2_open(&p,DOCS "/item-1099",0);assert(fs2_read(&p,fd,out,5)==5&&!memcmp(out,"INODE",5));assert(!fs2_close(&p,fd));
 fd=fs2_open(&p,"/System/proof",0);assert(fd>=0);assert(fs2_read(&p,fd,out,6)==6&&!memcmp(out,"SYSTEM",6));assert(!fs2_close(&p,fd));
 assert(!fs2_snap_create("before"));fd=fs2_open(&p,DOCS "/large",1);assert(fs2_write(&p,fd,"short",5)==5);assert(!fs2_close(&p,fd));assert(!fs2_stat(DOCS "/large",&e)&&e.size==5);
 /* View reads must use the snapshot root, not live inode contents or bootfs. */
 const char*view="/.snapshots/before/Users/User/Documents/large";
 assert(!fs2_stat(view,&e)&&e.ro&&e.size==sizeof in);int frozen=fs2_open(&p,view,0);assert(frozen>=0&&p.fd[frozen].backend==3);
 assert(fs2_read(&p,frozen,out,sizeof out)==sizeof out&&!memcmp(in,out,sizeof in));assert(!fs2_seek(&p,frozen,32768));assert(fs2_read(&p,frozen,out,512)==512&&!memcmp(in+32768,out,512));assert(!fs2_close(&p,frozen));
 assert(fs2_open(&p,view,1)<0&&fs2_create("/.snapshots/before/injected",0)<0&&fs2_remove(view,0)==-REMOVE_READ_ONLY&&fs2_move(view,DOCS "/stolen")<0);
 assert(fs2_stat("/.snapshots/before/System/proof",&e)<0);assert(fs2_list("/.snapshots/before/Users/User/Documents",page,37,1090)==11);for(int j=0;j<11;j++)assert(page[j].ro);
 assert(!fs2_snap_verify("before"));assert(!fs2_snap_restore("before"));assert(!fs2_stat(DOCS "/large",&e)&&e.size==sizeof in);
 assert(!fs2_move(DOCS "/item-1099",DOCS "/renamed"));assert(!fs2_stat(DOCS "/renamed",&e)&&e.id>1024);assert(fs2_stat(DOCS "/item-1099",&e)<0);
 fd=fs2_open(&p,DOCS "/large",0);assert(!fs2_seek(&p,fd,1ull<<34));assert(!fs2_read(&p,fd,out,64));assert(!fs2_close(&p,fd));
 memset(in,0x93,512);assert(!fs2_account(0,in,1));assert(!fs2_account(0,out,0)&&!memcmp(in,out,512));
 FsInfo info;assert(!fs2_info(&info)&&info.total==4096ull*T2_BLOCK&&info.snaps==1);assert(fs2_remove(DOCS,0)==-REMOVE_NOT_EMPTY);
 T2Entry parent;assert(!resolve(HOME "/Library/Preferences",&parent));node(parent.key,"com.tinymacos.appearance.plist",T2_FILE);assert(fs2_remove(HOME "/Library",REMOVE_RECURSIVE)==-REMOVE_READ_ONLY);assert(!fs2_stat(HOME "/Library/Preferences/com.tinymacos.appearance.plist",&e));
 /* More than 16 snapshots through both the paged ABI and browsable catalogue. */
 for(int j=0;j<24;j++){char name[28];snprintf(name,sizeof name,"many-%02d",j);assert(!fs2_snap_create(name));}
 SnapInfo snaps[7];int count=0;while((got=fs2_snap_page(snaps,7,count))>0)count+=got;assert(got==0&&count==25);assert(fs2_list("/.snapshots",page,37,0)==25);
 assert(fs2_local_backup(0)==0&&!fs2_local_backup(1)&&fs2_local_backup(0)==1);T2Entry checkpoint;assert(!tmc2_snapshot_find(&fs,"Copia locale",&checkpoint));u64 old_checkpoint=checkpoint.key;
 assert(!fs2_local_backup(1)&&!tmc2_snapshot_find(&fs,"Copia locale",&checkpoint)&&checkpoint.key!=old_checkpoint);assert(tmc2_view_stat(&fs,old_checkpoint,1,&checkpoint)<0);
 frozen=fs2_open(&p,view,0);assert(frozen>=0);
 assert(!fs2_remove(DOCS "/renamed",0));assert(!fs2_snap_delete("before"));assert(fs2_read(&p,frozen,out,1)<0);assert(!fs2_close(&p,frozen));assert(fs2_open(&p,"/System/proof",1)<0);
 /* The shared listing API never suppresses dot-prefixed installed/user names. */
 assert(!fs2_create(DOCS "/.build-visible",0));int found_dot=0,skip=0;while((got=fs2_list(DOCS,page,37,skip))>0){for(int j=0;j<got;j++)if(!strcmp(page[j].name,".build-visible"))found_dot++;skip+=got;}assert(got==0&&found_dot==1);
 tmc2_close(&fs);free(disk);
 puts("PASS actual FS2 kernel backend: 64KiB file, 1100 files/inode IDs beyond bootfs 1024, separate bootfs handles, 64-bit seek, overwrite truncation, rename, CoW snapshot restore/delete, private account slots, paged snapshot catalogue, read-only snapshot namespace/FD isolation, atomic local checkpoints and volume capacity");return 0;
}
