/* Real native FAT32/exFAT driver on independent disposable filesystems.
 * The partition view has explicit outside canaries; no host mount is used.
 */
#define _FILE_OFFSET_BITS 64
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "foreign_volume.h"
static u8*media;static size_t length;static unsigned writes,flushes;static int fail_read,fail_write;
static int rd(void*x,u64 at,u32 n,void*p){(void)x;assert(n&&n<=128&&at<=length/512&&n<=length/512-at);if(fail_read)return -1;memcpy(p,media+4096+at*512,n*512);return 0;}
static int wr(void*x,u64 at,u32 n,const void*p){(void)x;assert(n&&n<=128&&at<=length/512&&n<=length/512-at);if(fail_write)return -1;writes++;memcpy(media+4096+at*512,p,n*512);assert(!memcmp(media+4096+at*512,p,n*512));return 0;}
static int fl(void*x){(void)x;flushes++;return 0;}static void*al(void*x,size_t n){(void)x;return malloc(n);}static void fr(void*x,void*p){(void)x;free(p);}
int main(int argc,char**argv){
 assert(argc==3&&(!strcmp(argv[1],"fat32")||!strcmp(argv[1],"exfat")||!strcmp(argv[1],"ntfs"))&&!strncmp(argv[2],"/var/tmp/",9));
 int fd=open(argv[2],O_RDWR);assert(fd>=0);struct stat st;assert(!fstat(fd,&st));length=st.st_size;media=malloc(length+8192);assert(media);memset(media,0xa7,length+8192);assert(pread(fd,media+4096,length,0)==length);
 ForeignIo io={0,length/512,512,rd,wr,fl,al,fr,0};

 ForeignEntry entry;assert(!nt_volume_stat(&io,"/",&entry)&&entry.dir&&!entry.ro);
 unsigned initial=writes;ForeignIo ro=io;ro.write=0;assert(nt_volume_create(&ro,"/denied",1)==FOREIGN_READ_ONLY&&writes==initial);
 assert(!nt_volume_create(&io,"/Documenti",1));assert(!nt_volume_create(&io,"/Documenti/caffè.txt",0));
 u8*proof=malloc(65537),*readback=malloc(65537);assert(proof&&readback);for(unsigned i=0;i<65537;i++)proof[i]=(i*17+23)&255;
 assert(nt_volume_write(&io,"/Documenti/caffè.txt",0,proof,65537)==65537);
 assert(!nt_volume_stat(&io,"/Documenti/caffè.txt",&entry)&&!entry.dir&&entry.size==65537);
 assert(nt_volume_read(&io,"/Documenti/caffè.txt",0,readback,65537)==65537&&!memcmp(proof,readback,65537));
 assert(nt_volume_read(&io,"/Documenti/caffè.txt",32765,readback,123)==123&&!memcmp(proof+32765,readback,123));assert(nt_volume_read(&io,"/Documenti/caffè.txt",65537,readback,1)==0);
 assert(nt_volume_create(&io,"/Documenti/caffè.txt",0)==FOREIGN_EXISTS);assert(!nt_volume_create(&io,"/occupied.txt",0));assert(nt_volume_write(&io,"/occupied.txt",0,"PRESERVE",8)==8);
 assert(nt_volume_move(&io,"/Documenti/caffè.txt","/occupied.txt")==FOREIGN_EXISTS);assert(nt_volume_read(&io,"/occupied.txt",0,readback,8)==8&&!memcmp(readback,"PRESERVE",8));
 assert(!nt_volume_move(&io,"/Documenti/caffè.txt","/Documenti/rinominato.txt"));assert(nt_volume_stat(&io,"/Documenti/caffè.txt",&entry)==FOREIGN_MISSING);
 assert(nt_volume_write(&io,"/Documenti/rinominato.txt",0,"tiny",4)==4);assert(!nt_volume_stat(&io,"/Documenti/rinominato.txt",&entry)&&entry.size==4);
 assert(nt_volume_write(&io,"/Documenti/rinominato.txt",4,"tail",4)==4);assert(nt_volume_read(&io,"/Documenti/rinominato.txt",0,readback,100)==8&&!memcmp(readback,"tinytail",8));
 if(!strcmp(argv[1],"fat32")){initial=writes;assert(nt_volume_write(&io,"/occupied.txt",0xffffffffull,"xx",2)==FOREIGN_TOO_LARGE&&writes==initial);assert(!nt_volume_stat(&io,"/occupied.txt",&entry)&&entry.size==8);puts("PASS FAT32 4GiB-1 per-file ceiling refuses overflow BEFORE writes; existing file unchanged");}
 for(unsigned i=0;i<25;i++){char name[40];sprintf(name,"/Documenti/file-%02u.txt",i);assert(!nt_volume_create(&io,name,0));}
 ForeignEntry page[3];int count=0,n;while((n=nt_volume_list(&io,"/Documenti",page,3,count))>0){count+=n;assert(count<40);}assert(n==0&&count==26);
 assert(!nt_volume_create(&io,"/Documenti/Sub",1));
 assert(nt_volume_move(&io,"/Documenti","/Documenti/Sub/cycle")<0);
 assert(!nt_volume_stat(&io,"/Documenti",&entry)&&entry.dir);
 assert(!nt_volume_move(&io,"/Documenti/Sub","/Documenti/Spostata"));
 assert(!nt_volume_remove(&io,"/Documenti/Spostata"));
 assert(nt_volume_remove(&io,"/Documenti")<0);assert(!nt_volume_remove(&io,"/Documenti/file-03.txt"));assert(nt_volume_stat(&io,"/Documenti/file-03.txt",&entry)==FOREIGN_MISSING);
 initial=writes;fail_read=1;assert(nt_volume_create(&io,"/must-not-exist",0)<0&&writes==initial);fail_read=0;
 fail_write=1;assert(nt_volume_create(&io,"/failed-write",0)<0);fail_write=0;
 assert(!nt_volume_stat(&io,"/occupied.txt",&entry)&&entry.size==8);
 for(unsigned i=0;i<4096;i++)assert(media[i]==0xa7&&media[4096+length+i]==0xa7);
 assert(writes&&flushes);assert(pwrite(fd,media+4096,length,0)==length&&!fsync(fd)&&!close(fd));free(media);free(proof);free(readback);
 puts("PASS actual driver: Unicode create, read/write 65537-byte multi-cluster stream, unaligned seek/EOF, truncate/append, rename/no-overwrite, delete/nonempty refusal, bounded paged listing, readonly and I/O errors; outside canaries unchanged");
}
