/* SPDX-License-Identifier: MIT
 * Mounted foreign/user data volumes: bounded partition-only read/write.
 * Device-relative byte bounds are enforced independently of each filesystem.
 * Startup data is an alias, not another mounted/duplicated native container.
 */
#include "../kernel/k.h"
#include "disklayout.h"
#include "foreignfs.h"
#include "foreign_volume.h"
#include "tmcbfs2.h"
#include "volumes.h"
#define VOLUMES (SETUP_MAX_DISKS*DISK_PARTS)
typedef struct {int disk,number,type;u64 base,sectors;ForeignFs*reader;Tmc2*native;char name[20];} Volume;
static Volume volumes[VOLUMES];static int nvolumes,discovered,inventory_error;static u32 generation=1;
extern int volume_current(void),volume_extent(u64*,u64*);
static DiskMap inventory;
int vol_normalize(const char*p,char out[PATHLEN]){
 if(!p||*p!='/')return -1;unsigned used=0;
 while(*p){while(*p=='/')p++;const char*s=p;while(*p&&*p!='/')p++;size_t n=p-s;if(!n)break;
  if(n==1&&s[0]=='.')continue;if(n==2&&s[0]=='.'&&s[1]=='.'){while(used&&out[used-1]!='/')used--;if(used)used--;continue;}
  if(n>=48||used+1+n>=PATHLEN)return -1;out[used++]='/';memcpy(out+used,s,n);used+=n;
 }if(!used)out[used++]='/';out[used]=0;return 0;
}
static int namespace(const char*p){return strlen(p)>=8&&!memcmp(p,"/Volumes",8)&&(!p[8]||p[8]=='/');}
int vol_reserved(const char*p){char q[PATHLEN];return !vol_normalize(p,q)&&namespace(q);}
static const char*startup(void){return boot.install_len?"tinymacOS Live":"Macintosh HD";}
static int bytes(void*x,u64 off,void*p,size_t len){
 Volume*v=x;u64 limit=v->sectors*512;if(off>limit||len>limit-off)return -1;
 while(len){u8 sector[512];u64 lba=off/512;unsigned part=off%512,n=MIN(len,512-part);
  if(!part&&len>=512){unsigned blocks=MIN(len/512,128);n=blocks*512;if(sata_io(v->disk,v->base+lba,blocks,p,0))return -1;}
  else{if(sata_io(v->disk,v->base+lba,1,sector,0))return -1;memcpy(p,sector+part,n);}
  p=(u8*)p+n;len-=n;off+=n;
 }return 0;
}
static int sectors(void*x,u64 at,u32 n,void*p,int write){Volume*v=x;if(!n||at>v->sectors||n>v->sectors-at)return -1;return sata_io(v->disk,v->base+at,n,p,write);}
static int read_sectors(void*x,u64 at,u32 n,void*p){return sectors(x,at,n,p,0);}
static int write_sectors(void*x,u64 at,u32 n,const void*p){return sectors(x,at,n,(void*)p,1);}
static int flush_volume(void*x){return sata_flush(((Volume*)x)->disk);}
static int page_read(void*x,u64 block,void*p){return block>=((Volume*)x)->sectors/32?-1:bytes(x,block*T2_BLOCK,p,T2_BLOCK);}
static int no_write(void*x,u64 b,const void*p){(void)x;(void)b;(void)p;return -1;}
static int no_flush(void*x){(void)x;return 0;}
static void*allocate(void*x,size_t n){(void)x;return n==4096?(void*)page_get():kmalloc(n);}
static void deallocate(void*x,void*p){(void)x;if(!p)return;if(kmalloc_owns(p))kfree(p);else page_put((u64)p);}
static ForeignIo foreign_io(Volume*v){return (ForeignIo){v,v->sectors,sata_logical_bytes(v->disk),read_sectors,write_sectors,flush_volume,allocate,deallocate,0};}
static int stat_foreign(Volume*v,const char*p,ForeignEntry*e){ForeignIo io=foreign_io(v);return v->type==FS_NTFS?nt_volume_stat(&io,p,e):fat_volume_stat(&io,p,e);}
static i64 data_foreign(Volume*v,const char*p,u64 off,void*b,size_t n,int w){ForeignIo io=foreign_io(v);return v->type==FS_NTFS?(w?nt_volume_write(&io,p,off,b,n):nt_volume_read(&io,p,off,b,n)):(w?fat_volume_write(&io,p,off,b,n):fat_volume_read(&io,p,off,b,n));}
static int foreign_entry(const ForeignEntry*e,Dirent*d){if(strlen(e->name)>=sizeof d->name)return -1;*d=(Dirent){.dir=e->dir,.ro=e->ro,.size=e->size};strset(d->name,e->name,sizeof d->name);return 0;}
void vol_refresh(void){
 for(int i=0;i<nvolumes;i++){Volume*v=volumes+i;if(v->native){tmc2_close(v->native);kfree(v->native);}kfree(v->reader);}
 memset(volumes,0,sizeof volumes);nvolumes=0;inventory_error=0;discovered=0;if(++generation==0)generation=1;
}
static void discover(void){
 if(discovered)return;discovered=1;u64 current_base=0,current_size=0;int current=volume_current();volume_extent(&current_base,&current_size);
 for(int disk=0;disk<sata_count();disk++){
  Medium medium={sata_io,sata_flush,disk,sata_size(disk),0,sata_logical_bytes(disk)};
  if(!partition_scan(&medium,&inventory)||!inventory.safe)continue;
  for(int i=0;i<inventory.count;i++){DiskPartition*p=inventory.part+i;
   if(p->fs!=FS_FAT32&&p->fs!=FS_NTFS&&p->fs!=FS_EXFAT&&p->fs!=FS_TMC)continue;
   if(disk==current&&p->start==current_base&&p->sectors==current_size)continue;
   /* Never expose ESP/Recovery/system payloads as user-native volumes. */
   if(!memcmp(p->type,esp_type,16)||!memcmp(p->type,src_type,16))continue;
   if(nvolumes>=VOLUMES){inventory_error=1;return;}Volume*v=volumes+nvolumes++;v->disk=disk;v->number=p->number;v->base=p->start;v->sectors=p->sectors;v->type=p->fs;
   char num[24];strset(v->name,"disk",sizeof v->name);number(num,disk);append(v->name,num,sizeof v->name);append(v->name,"s",sizeof v->name);number(num,p->number);append(v->name,num,sizeof v->name);
  }
 }
}
static int mount(Volume*v){
 if(v->reader||v->native)return 0;u8 magic[8];if(bytes(v,0,magic,8))return -1;
 if(!memcmp(magic,"TMCBFS2\0",8)){
  Tmc2*t=kmalloc(sizeof *t);if(!t)return -1;
  if(tmc2_mount(t,(T2Io){v,v->sectors/32,page_read,no_write,no_flush,allocate,deallocate})){kfree(t);return -1;}v->native=t;return 0;
 }
 ForeignFs*f=kmalloc(sizeof *f);if(!f)return -1;
 if(fv_mount(f,v,v->sectors*512,bytes)){kfree(f);return -1;}v->reader=f;return 0;
}
/* -2 outside namespace/startup alias; -1 invalid; 0 root; index+1 volume. */
static int path(const char*input,char q[PATHLEN],const char**inner){
 if(vol_normalize(input,q)||!namespace(q))return -2;if(!q[8])return 0;
 char*name=q+9,*end=name;while(*end&&*end!='/')end++;char saved=*end;*end=0;
 if(eq(name,startup())){*end=saved;return -2;}discover();int found=-1;
 for(int i=0;i<nvolumes;i++)if(eq(name,volumes[i].name)){found=i+1;break;}*end=saved;*inner=saved?end:"/";return found;
}
static int resolve(Volume*v,const char*inner,FvNode*out){
 if(mount(v))return -1;FvNode n;
 if(v->native)n=(FvNode){.id=1,.dir=1};else if(fv_root(v->reader,&n))return -1;
 while(*inner){while(*inner=='/')inner++;if(!*inner)break;char name[48];unsigned len=0;while(*inner&&*inner!='/'){if(len>=47)return -1;name[len++]=*inner++;}name[len]=0;
  if(!n.dir)return -1;
  if(v->native){T2Entry e;if(tmc2_lookup(v->native,n.id,name,&e))return -1;n=(FvNode){.id=e.key,.size=e.size,.valid=e.size,.dir=e.kind==T2_DIR};strset(n.name,e.name,48);}
  else{FvNode child;if(fv_lookup(v->reader,&n,name,&child))return -1;n=child;}
 }*out=n;return 0;
}
static void entry(const FvNode*n,Dirent*d){*d=(Dirent){.id=n->id,.dir=n->dir,.ro=1,.size=n->size};strset(d->name,n->name,48);}
int vol_stat(const char*p,Dirent*d){
 char q[PATHLEN];const char*inner=0;int at=path(p,q,&inner);if(at==-2)return -2;if(at<0||!d)return -1;
 if(!at){*d=(Dirent){.dir=1,.ro=1};strset(d->name,"Volumes",48);return 0;}Volume*v=volumes+at-1;if(v->type!=FS_TMC){ForeignEntry e;if(stat_foreign(v,inner,&e)||foreign_entry(&e,d))return -1;if(eq(inner,"/"))strset(d->name,v->name,48);return 0;}FvNode n;if(resolve(v,inner,&n))return -1;entry(&n,d);if(eq(inner,"/"))strset(d->name,v->name,48);return 0;
}
int vol_list(const char*p,Dirent*d,int cap,int skip){
 char q[PATHLEN];const char*inner=0;int at=path(p,q,&inner);if(at==-2)return -2;if(at<0||cap<0||skip<0||(!d&&cap))return -1;
 if(!at){discover();if(inventory_error)return -1;int total=0,n=0;
  if(total++>=skip&&n<cap){d[n]=(Dirent){.dir=1,.ro=1};strset(d[n++].name,startup(),48);}
  for(int i=0;i<nvolumes&&n<cap;i++){
   /* Failed mounts are not silently presented as empty readable folders. */
   if(total++<skip)continue;d[n]=(Dirent){.id=i+1,.dir=1,.ro=volumes[i].type==FS_TMC};strset(d[n++].name,volumes[i].name,48);
  }return n;
 }
 Volume*v=volumes+at-1;if(v->type!=FS_TMC){if(!cap)return 0;ForeignIo io=foreign_io(v);ForeignEntry*page=kmalloc(sizeof *page*MIN(cap,8));if(!page)return -1;int n=0;while(n<cap){int want=MIN(cap-n,8),r=v->type==FS_NTFS?nt_volume_list(&io,inner,page,want,skip+n):fat_volume_list(&io,inner,page,want,skip+n);if(r<0){n=-1;break;}for(int j=0;j<r;j++)if(foreign_entry(page+j,d+n+j)){n=-1;goto listed;}n+=r;if(r<want)break;}listed:kfree(page);return n;}FvNode parent;if(resolve(v,inner,&parent)||!parent.dir)return -1;u64 cookie=0;int n=0,r=0;
 if(v->native){T2Entry e;while(n<cap&&(r=tmc2_readdir(v->native,parent.id,&cookie,&e))==1){if(skip){skip--;continue;}d[n]=(Dirent){.id=e.key,.dir=e.kind==T2_DIR,.ro=1,.size=e.size};strset(d[n++].name,e.name,48);}}
 else{FvNode e;while(n<cap&&(r=fv_next(v->reader,&parent,&cookie,&e))==1){if(skip){skip--;continue;}entry(&e,d+n++);}}
 return r<0?-1:n;
}
int vol_open(Process*p,const char*name,int write){
 char q[PATHLEN];const char*inner=0;int at=path(name,q,&inner);if(at==-2)return -2;if(at<=0)return -1;Volume*v=volumes+at-1;
 if(v->type!=FS_TMC){ForeignEntry e;if(stat_foreign(v,inner,&e)||e.dir||(write&&e.ro))return -1;for(int fd=0;fd<12;fd++)if(!p->fd[fd].id){p->fd[fd]=(FD){.id=-1,.backend=4,.write=write,.snapshot=((u64)generation<<32)|(at-1)};strset(p->fd[fd].foreign_path,inner,PATHLEN);return fd;}return -1;}
 if(write)return -1;FvNode n;if(resolve(v,inner,&n)||n.dir)return -1;
 for(int fd=0;fd<12;fd++)if(!p->fd[fd].id){p->fd[fd]=(FD){.id=-1,.backend=v->native?5:4,.gen=n.start,.write=n.flags,.inode=v->native?n.id:n.size,.snapshot=((u64)generation<<32)|(at-1),.foreign_valid=n.valid};
  /* NTFS uses an MFT ID; all other foreign files use a cluster/slot index. */
  if(v->reader&&v->reader->type==FS_NTFS)p->fd[fd].gen=n.id;
  return fd;
 }return -1;
}
int vol_read(Process*p,int fd,void*out,int count){
 if(fd<0||fd>=12||count<0||(!out&&count))return -1;FD*f=p->fd+fd;unsigned at=(u32)f->snapshot;
 if((f->backend!=4&&f->backend!=5)||!f->id||(f->snapshot>>32)!=generation||at>=(unsigned)nvolumes)return -1;Volume*v=volumes+at;i64 r;
 if(f->backend==5)r=tmc2_read(v->native,f->inode,f->at,out,count);
 else r=data_foreign(v,f->foreign_path,f->at,out,count,0);
 if(r>=0)f->at+=r;return r;
}
int vol_seek(Process*p,int fd,u64 off){if(fd<0||fd>=12||off>T2_MAX_BYTES)return -1;FD*f=p->fd+fd;if(!f->id||(f->backend!=4&&f->backend!=5)||(f->snapshot>>32)!=generation)return -1;f->at=off;return 0;}
int vol_exportable(const char*p){char q[PATHLEN];const char*inner=0;int at=path(p,q,&inner);if(at<=0||!eq(inner,"/"))return 0;Volume*v=volumes+at-1;return v->type==FS_TMC&&!mount(v);}

int vol_write(Process*p,int fd,const void*data,int count){
 if(fd<0||fd>=12||count<0||(!data&&count))return -1;FD*f=p->fd+fd;unsigned at=(u32)f->snapshot;
 if(f->backend!=4||!f->id||!f->write||(f->snapshot>>32)!=generation||at>=(unsigned)nvolumes)return -1;
 i64 r=data_foreign(volumes+at,f->foreign_path,f->at,(void*)data,count,1);if(r>=0)f->at+=r;return r;
}
int vol_create(const char*p,int dir){char q[PATHLEN];const char*inner=0;int at=path(p,q,&inner);if(at==-2)return -2;if(at<=0||eq(inner,"/"))return -1;Volume*v=volumes+at-1;if(v->type==FS_TMC)return -1;ForeignIo io=foreign_io(v);return (v->type==FS_NTFS?nt_volume_create(&io,inner,dir):fat_volume_create(&io,inner,dir))? -1:0;}
static int remove_foreign(Volume*v,const char*p,unsigned flags,unsigned depth){
 ForeignEntry e;if(depth>48||stat_foreign(v,p,&e))return -REMOVE_BAD_PATH;if(e.ro)return -REMOVE_READ_ONLY;
 if(((flags&REMOVE_FILE_ONLY)&&e.dir)||((flags&REMOVE_DIR_ONLY)&&!e.dir))return -REMOVE_WRONG_TYPE;
 ForeignIo io=foreign_io(v);
 if(e.dir&&(flags&REMOVE_RECURSIVE)){for(;;){ForeignEntry child;int r=v->type==FS_NTFS?nt_volume_list(&io,p,&child,1,0):fat_volume_list(&io,p,&child,1,0);if(r<0)return -REMOVE_IO;if(!r)break;char q[PATHLEN];if(strlen(p)+strlen(child.name)+2>PATHLEN)return -REMOVE_BAD_PATH;strset(q,p,PATHLEN);append(q,"/",PATHLEN);append(q,child.name,PATHLEN);r=remove_foreign(v,q,REMOVE_RECURSIVE,depth+1);if(r)return r;}}
 int r=v->type==FS_NTFS?nt_volume_remove(&io,p):fat_volume_remove(&io,p);return !r?0:r==FOREIGN_NOT_EMPTY?-REMOVE_NOT_EMPTY:r==FOREIGN_READ_ONLY?-REMOVE_READ_ONLY:-REMOVE_IO;
}
int vol_remove(const char*p,unsigned flags){char q[PATHLEN];const char*inner=0;int at=path(p,q,&inner);if(at==-2)return -2;if(at<=0||eq(inner,"/")||volumes[at-1].type==FS_TMC)return -REMOVE_READ_ONLY;if(++generation==0)generation=1;return remove_foreign(volumes+at-1,inner,flags,0);}
/* -3 = a cross-filesystem move; copy/verify/remove is handled by fs_move. */
int vol_move(const char*a,const char*b){char qa[PATHLEN],qb[PATHLEN];const char*pa=0,*pb=0;int ia=path(a,qa,&pa),ib=path(b,qb,&pb);if(ia==-2&&ib==-2)return -2;if(ia<=0||ib<=0)return (ia==-2||ib==-2)?-3:-1;if(ia!=ib)return -3;Volume*v=volumes+ia-1;if(v->type==FS_TMC||eq(pa,"/")||eq(pb,"/"))return -1;if(++generation==0)generation=1;ForeignIo io=foreign_io(v);return (v->type==FS_NTFS?nt_volume_move(&io,pa,pb):fat_volume_move(&io,pa,pb))?-1:0;}
