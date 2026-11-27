#define _FILE_OFFSET_BITS 64
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "foreignfs.h"
#include "exfat_copy.h"
static int input,output,writes,fail_io,sabotage,prepared;
static u64 patch_at=~0ull;static unsigned patch_len;static u8 patch[16];
static int readfile(void*x,u64 off,void*p,size_t n){int fd=*(int*)x;if(fail_io)return -1;int r=pread(fd,p,n,off)==(ssize_t)n?0:-1;
 for(unsigned i=0;i<patch_len;i++)if(patch_at+i>=off&&patch_at+i-off<n)((u8*)p)[patch_at+i-off]=patch[i];return r;}
static int writefile(void*x,u64 off,const void*p,size_t n){(void)x;writes++;return pwrite(output,p,n,off)==(ssize_t)n?0:-1;}
static void*al(void*x,size_t n){(void)x;return malloc(n);}
static void fr(void*x,void*p){(void)x;free(p);}
static int next(void*x,u64 dir,u64*cookie,ExEntry*e){(void)x;
 if(dir==1){if(*cookie==0)*e=(ExEntry){2,0,1,"Documenti"};else if(*cookie==1)*e=(ExEntry){3,0,0,"vuoto.txt"};else if(*cookie==2)*e=(ExEntry){6,65537,0,"proof.bin"};else if(*cookie==3)*e=(ExEntry){7,4,0,"mini.txt"};else return 0;}
 else if(dir==2){if(*cookie==0)*e=(ExEntry){4,65537,0,"caffè.txt"};else if(*cookie==1)*e=(ExEntry){5,4,0,"piccolo.txt"};else return 0;}else return -1;
 if(sabotage==1&&dir==1&&!*cookie)strcpy(e->name,"CON.txt");
 if(sabotage==2&&dir==2)strcpy(e->name,*cookie?"SAME":"same");
 if(sabotage==3&&dir==1&&!*cookie)e->id=dir;
 if(sabotage==4&&dir==1&&!*cookie)strcpy(e->name,"\xc0\xaf");
 (*cookie)++;return 1;
}
static int prepare(void*x,u64 logical,u64 stored){(void)x;assert(logical>stored);prepared++;return sabotage==5?-1:ftruncate(output,logical);}
static i64 source(void*x,u64 id,u64 off,void*p,size_t n){(void)x;for(size_t i=0;i<n;i++)((u8*)p)[i]=(id==5||id==7)?"CIAO"[off+i]:(u8)((off+i)*17+23);return n;}
static void readproof(ForeignFs*f,FvNode*n,u64 size,int small){
 assert(!n->dir&&n->size==size);u8 b[9000];u64 at=0;
 while(at<size){size_t k=MIN(sizeof b,size-at);assert(fv_read(f,n,at,b,k)==(i64)k);for(size_t j=0;j<k;j++)assert(b[j]==(small?"CIAO"[at+j]:(u8)((at+j)*17+23)));at+=k;}
 assert(fv_read(f,n,size,b,1)==0);
 if(size>32768){assert(fv_read(f,n,32765,b,7)==7);for(int j=0;j<7;j++)assert(b[j]==(u8)((32765+j)*17+23));}
}
static int memory_read(void*x,u64 at,void*p,size_t n){assert(at<=1048576&&n<=1048576-at);memcpy(p,(u8*)x+at,n);return 0;}
static void w32(u8*p,u32 v){memcpy(p,&v,4);}
static u32 crc32test(const u8*p,unsigned n){u32 v=~0u;while(n--){v^=*p++;for(int i=0;i<8;i++)v=(v>>1)^(0xedb88320u&-(v&1));}return ~v;}
static void legacy_mount_cases(void){
 u8*m=calloc(1,1048576);assert(m);memcpy(m,"TMCBFS1\0",8);w32(m+8,2);w32(m+12,64);w32(m+16,144);w32(m+20,1);
 m[64*512]=1;u8*e=m+64*512+64;e[0]=2;w32(e+4,4);strcpy((char*)e+12,"legacy.txt");memcpy(m+(144+16)*512,"CIAO",4);
 ForeignFs f;FvNode root,n;char text[4];assert(!fv_mount(&f,m,1048576,memory_read)&&!fv_root(&f,&root)&&!fv_lookup(&f,&root,"legacy.txt",&n));assert(fv_read(&f,&n,0,text,4)==4&&!memcmp(text,"CIAO",4));
 u8*c=m+137*512;memcpy(c,"TMCOW03",8);w32(c+8,1);unsigned frame=144+64*16,stride=(frame*8+511)/512,start=frame+8*stride;
 w32(c+12,frame);w32(c+16,frame);w32(c+20,stride);w32(c+24,start);w32(c+28,2048-start);w32(c+32,2048);w32(c+36,crc32test(c,512));
 assert(!fv_mount(&f,m,1048576,memory_read)&&!fv_root(&f,&root)&&!fv_lookup(&f,&root,"legacy.txt",&n));assert(fv_read(&f,&n,0,text,4)==4&&!memcmp(text,"CIAO",4));
 m[138*512]=1;assert(fv_mount(&f,m,1048576,memory_read)<0);m[138*512]=0;c[36]^=1;assert(fv_mount(&f,m,1048576,memory_read)<0);memset(c,0,512);m[24]=1;assert(fv_mount(&f,m,1048576,memory_read)<0);
 free(m);puts("PASS read-only legacy native and clean CoW live-file mount; pending replay, bad CoW checksum and encryption refuse without writes");
}
int main(int argc,char**argv){assert(argc>=3);
 if(!strcmp(argv[1],"export")){
  legacy_mount_cases();
  u64 rejected;for(sabotage=1;sabotage<=5;sabotage++){writes=prepared=0;assert(exfat_copy((ExCopyIo){0,next,source,writefile,al,fr,0,prepare},1,&rejected)<0);assert(!writes&&prepared==(sabotage==5));}
  sabotage=0;puts("PASS export preflight rejects Windows reserved names/case collisions, recursive source IDs, invalid UTF8 and denied preparation before first output byte");
  output=open(argv[2],O_RDWR|O_CREAT|O_EXCL,0600);assert(output>=0);u64 bytes;assert(!exfat_copy((ExCopyIo){0,next,source,writefile,al,fr,0x544d4332,prepare},1,&bytes));struct stat s;assert(!fstat(output,&s)&&(u64)s.st_size==bytes);close(output);puts("PASS new raw real-partition exFAT, Unicode directory tree, empty/resident-sized/65537-byte files, no source writes");return 0;
 }
 input=open(argv[2],O_RDONLY);assert(input>=0);struct stat s;assert(!fstat(input,&s));ForeignFs f;assert(!fv_mount(&f,&input,s.st_size,readfile));FvNode root,n,dir;assert(!fv_root(&f,&root));u64 cookie=0;int r,total=0;
 while((r=fv_next(&f,&root,&cookie,&n))==1){printf("entry %s %llu dir %d\n",n.name,(unsigned long long)n.size,n.dir);assert(++total<1000);}assert(r==0);
 if(!strcmp(argv[1],"export-read")){assert(f.type==FS_EXFAT);assert(!fv_lookup(&f,&root,"Documenti",&dir)&&dir.dir);assert(!fv_lookup(&f,&dir,"caffè.txt",&n));readproof(&f,&n,65537,0);assert(!fv_lookup(&f,&dir,"piccolo.txt",&n));readproof(&f,&n,4,1);assert(!fv_lookup(&f,&root,"vuoto.txt",&n));readproof(&f,&n,0,0);}
 else{assert(!fv_lookup(&f,&root,"proof.bin",&n));readproof(&f,&n,65537,0);assert(!fv_lookup(&f,&root,"mini.txt",&n));readproof(&f,&n,4,1);}
 /* Read-callback overlays: never alter the independently formatted fixture. */
 ForeignFs bad;patch_at=f.type==FS_EXFAT?108:12;patch_len=1;patch[0]=0;
 assert(fv_mount(&bad,&input,s.st_size,readfile)<0);patch_len=0;
 if(f.type==FS_EXFAT){
  patch_at=f.heap+(u64)(f.root-2)*f.cluster+98;patch_len=0;
  assert(!readfile(&input,patch_at,patch,1));patch[0]^=1;patch_len=1;cookie=0;
  assert(fv_next(&f,&root,&cookie,&n)<0);patch_len=0;
 }
 if(f.type==FS_FAT32){
  patch_at=f.fat+(u64)f.root*4;patch_len=4;for(int i=0;i<4;i++)patch[i]=f.root>>(i*8);
  FvNode loop={.start=f.root,.size=f.cluster+1,.valid=f.cluster+1};u8 byte;
  assert(fv_read(&f,&loop,f.cluster,&byte,1)<0);patch_len=0;
 }
 fail_io=1;assert(fv_mount(&bad,&input,s.st_size,readfile)<0);fail_io=0;
 puts("PASS malformed boot geometry, filesystem metadata checksum/chain cycle and injected I/O refusal; no fixture writes");
 assert(!writes);close(input);puts("PASS read-only real filesystem: root/readdir/lookup, Unicode, empty and 65537-byte streams, cross-cluster read, unaligned seek and EOF; no write API");
}
