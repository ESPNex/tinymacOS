/* Actual mounted-volume and preserving-export adapters, with disposable files
 * instead of SATA controllers. Backend routing below mirrors the kernel's
 * gate-free dispatch; actual syscall/compositor routing is covered in guests. */
#define _FILE_OFFSET_BITS 64
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "../fs/tmcbfs2_runtime.c"
#include "../fs/exfat_export.c"
u32 persistent,fsrev,usednodes,theme,wall,dock,reduce_motion,opaque,dark;
struct Boot boot;u64 ticks=12345;
static int media[6];static u64 sizes[6];static int foreign_writes,source_data_error,readback_corruption;
static int allocation_failure=-1;
void*kmalloc(size_t n){if(allocation_failure==0)return 0;if(allocation_failure>0)allocation_failure--;return malloc(n);}void kfree(void*p){free(p);}int kmalloc_owns(const void*p){(void)p;return 1;}
u64 page_get(void){return (u64)calloc(1,4096);}void page_put(u64 p){free((void*)p);}
int volume_current(void){return 0;}int volume_extent(u64*b,u64*s){*b=0;*s=sizes[0]/512;return 0;}
int sata_count(void){return 6;}u64 sata_size(int d){return sizes[d]/512+(d&&d<5?2048:0);}u32 sata_logical_bytes(int d){(void)d;return 512;}int sata_flush(int d){return fsync(media[d]);}
static void put32(u8*p,u32 n){memcpy(p,&n,4);}
int sata_io(int d,u64 l,u32 n,void*p,int write){
 assert(d>=0&&d<6&&n&&n<=256&&l+n<=sata_size(d));
 if(d==5){if(write){foreign_writes++;return pwrite(media[d],p,n*512,l*512)!=(ssize_t)n*512;}return pread(media[d],p,n*512,l*512)!=(ssize_t)n*512;}
 if(write)return pwrite(media[d],p,n*512,(l-(d?2048:0))*512)!=(ssize_t)n*512;
 if(d&&d<5&&l<2048){memset(p,0,n*512);assert(l+n<=2048);if(!l){u8*b=p;b[510]=85;b[511]=170;b[450]=d==1?0xc:7;put32(b+454,2048);put32(b+458,sizes[d]/512);}return 0;}
 int r=pread(media[d],p,n*512,(l-(d?2048:0))*512)==(ssize_t)n*512?0:-1;
 if(d==4&&source_data_error==1)for(unsigned at=0;at+15<=n*512;at++)if(!memcmp((u8*)p+at,"EXTERNAL_NATIVE",15)){source_data_error=2;return -1;}return r;
}
int bootfs_lookup(const char*p){(void)p;return -2;}int bootfs_stat(int i,Dirent*d){(void)i;(void)d;return -1;}int bootfs_list(int i,Dirent*d,int n,int s){(void)i;(void)d;(void)n;(void)s;return 0;}
int bootfs_open(Process*p,int i,int w){(void)p;(void)i;(void)w;return -1;}int bootfs_read(FD*f,void*p,int n){(void)f;(void)p;(void)n;return -1;}
int fs_stat(const char*p,Dirent*d){int r=vol_stat(p,d);return r==-2?fs2_stat(p,d):r;}
int fs_list(const char*p,Dirent*d,int n,int s){int r=vol_list(p,d,n,s);return r==-2?fs2_list(p,d,n,s):r;}
int fs_open(Process*p,const char*name,int w){int r=vol_open(p,name,w);return r==-2?fs2_open(p,name,w):r;}
int fs_read(Process*p,int fd,void*b,int n){int r=p->fd[fd].backend>=4?vol_read(p,fd,b,n):fs2_read(p,fd,b,n);
 if(readback_corruption&&p->fd[fd].backend==2&&r>0){((u8*)b)[0]^=1;readback_corruption++;}return r;}
int fs_seek(Process*p,int fd,u64 at){return p->fd[fd].backend>=4?vol_seek(p,fd,at):fs2_seek(p,fd,at);}
int fs_close(Process*p,int fd){if(p->fd[fd].backend>=4){memset(p->fd+fd,0,sizeof(FD));return 0;}return fs2_close(p,fd);}int fs_snap_page(void*out,int cap,int skip){return fs2_snap_page(out,cap,skip);}
static int rd(void*x,u64 b,void*p){int fd=*(int*)x;return pread(fd,p,T2_BLOCK,b*T2_BLOCK)==T2_BLOCK?0:-1;}
static int wr(void*x,u64 b,const void*p){int fd=*(int*)x;return pwrite(fd,p,T2_BLOCK,b*T2_BLOCK)==T2_BLOCK?0:-1;}
static int fl(void*x){return fsync(*(int*)x);}static void*al(void*x,size_t n){(void)x;return malloc(n);}static void fr(void*x,void*p){(void)x;free(p);}
static u64 node(Tmc2*t,u64 parent,const char*name,int kind){u64 id;assert(!tmc2_create(t,parent,name,kind,&id));return id;}
static void save(const char*path,const char*contents){Process p={0};Dirent e;if(fs2_stat(path,&e))assert(!fs2_create(path,0));int fd=fs2_open(&p,path,1);assert(fd>=0);assert(fs2_write(&p,fd,contents,strlen(contents))==(int)strlen(contents));assert(!fs2_close(&p,fd));}
static void dump(const char*path,const char*dest){Process p={0};int fd=fs2_open(&p,path,0),out=open(dest,O_WRONLY|O_CREAT|O_EXCL,0600);assert(fd>=0&&out>=0);u8 b[16384];int n;while((n=fs2_read(&p,fd,b,sizeof b))>0)assert(write(out,b,n)==n);assert(n==0);close(out);fs2_close(&p,fd);}
int main(int argc,char**argv){
 assert(argc==8);for(int i=0;i<6;i++){media[i]=open(argv[i+1],O_RDWR);assert(media[i]>=0);struct stat st;assert(!fstat(media[i],&st));sizes[i]=st.st_size;}
 assert(!tmc2_format(&fs,(T2Io){media,sizes[0]/T2_BLOCK,rd,wr,fl,al,fr}));fs2_active=1;
 u64 users=node(&fs,1,"Users",T2_DIR),home=node(&fs,users,"User",T2_DIR);node(&fs,home,"Documents",T2_DIR);node(&fs,1,"Volumes",T2_DIR);
 save(DOCS "/marker.txt","ORIGINAL_FROZEN");assert(!fs2_snap_create("utente-copy"));save(DOCS "/marker.txt","LIVE_CHANGED");
 Tmc2 external;assert(!tmc2_format(&external,(T2Io){media+4,sizes[4]/T2_BLOCK,rd,wr,fl,al,fr}));u64 file=node(&external,1,"external.txt",T2_FILE);assert(tmc2_write(&external,file,0,"EXTERNAL_NATIVE",15)==15);assert(!tmc2_collect(&external));tmc2_close(&external);
 u8 table[512]={0};table[510]=85;table[511]=170;table[450]=7;put32(table+454,2048);put32(table+458,2048);assert(pwrite(media[5],table,512,0)==512);
 Dirent entries[12];assert(vol_list("/Volumes",entries,12,0)>=5);Process p={0};
 for(int disk=1;disk<=3;disk++){
  char path[100],target[100];sprintf(path,"/Volumes/disk%ds1/proof.bin",disk);Dirent e;assert(!vol_stat(path,&e)&&e.size==65537&&!e.ro);int fd=vol_open(&p,path,0);assert(fd>=0&&p.fd[fd].backend==4);u8 b[10];assert(!vol_seek(&p,fd,32765));assert(vol_read(&p,fd,b,7)==7);for(int j=0;j<7;j++)assert(b[j]==(u8)((32765+j)*17+23));assert(!fs_close(&p,fd));
  sprintf(path,"/Volumes/disk%ds1/new",disk);assert(!vol_create(path,1));strcat(path,"/Unicode-caffè.txt");assert(!vol_create(path,0));fd=vol_open(&p,path,1);assert(fd>=0);assert(vol_write(&p,fd,"NAMESPACE_RW",12)==12);assert(!vol_seek(&p,fd,0));assert(vol_read(&p,fd,b,4)==4&&!memcmp(b,"NAME",4));fs_close(&p,fd);sprintf(target,"/Volumes/disk%ds1/renamed.txt",disk);assert(!vol_move(path,target));assert(!vol_remove(target,REMOVE_FILE_ONLY));sprintf(path,"/Volumes/disk%ds1/new",disk);assert(!vol_remove(path,REMOVE_DIR_ONLY));
 }
 puts("PASS real namespace adapters: FAT32/NTFS/exFAT create/write/seek/read/rename/remove through ForeignIo and FD, no readonly shim");
 VolumeExport q={0};strset(q.source,"/.snapshots/utente-copy",PATHLEN);strset(q.destination,"disk5",PATHLEN);assert(run(&q)==1&&q.bytes>33554432&&!foreign_writes);q.part.confirm=DISK_EDIT_CONFIRM;
 table[440]=1;assert(pwrite(media[5],table,512,0)==512);assert(run(&q)<0&&!foreign_writes);table[440]=0;assert(pwrite(media[5],table,512,0)==512);
 q.part.confirm=0;assert(run(&q)==1&&!foreign_writes);q.part.confirm=DISK_EDIT_CONFIRM;assert(!run(&q)&&q.part.number==2&&foreign_writes);assert(!strcmp(q.destination,"/Volumes/disk5s2"));
 char b[20]={0};int fd=fs2_open(&p,DOCS "/marker.txt",0);assert(fs2_read(&p,fd,b,12)==12&&!strcmp(b,"LIVE_CHANGED"));fs2_close(&p,fd);assert(!fs2_snap_verify("utente-copy"));
 char raw[512];assert(pread(media[5],raw,512,0)==512&&!memcmp(raw+446,table+446,16));int out=open(argv[7],O_WRONLY|O_CREAT|O_EXCL,0600);assert(out>=0);u8 block[4096];for(u64 at=0;at<q.bytes;at+=4096){assert(pread(media[5],block,4096,q.part.start*512+at)==4096);assert(write(out,block,4096)==4096);}close(out);
 assert(!tmc2_verify(&fs));vol_refresh();tmc2_close(&fs);for(int i=0;i<6;i++)close(media[i]);
 puts("PASS actual new REAL exFAT partition export: zero-write preview, stale-table refusal before writes, existing partition entry preserved, publish last after verified copy; native live bytes and snapshot unchanged. NO VHD.");
}
