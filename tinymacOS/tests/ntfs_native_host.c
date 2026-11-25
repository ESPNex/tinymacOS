/* Disposable host-image test of the shipping freestanding engine/adapter.
 * Independent ntfsresize + ntfscat validation is performed by the runner. */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include "disklayout.h"
static FILE*f;
static u32 sectors,start=2048,maximum;
static int writes,failat=-1;
static int io(int d,u32 at,u32 n,void*p,int w){
 assert(!d&&n&&at<sectors&&n<=sectors-at);
 /* No upstream request may escape the selected partition, including errors. */
 assert(at>=start&&at+n<=start+maximum);
 assert(!fseek(f,(u64)at*512,SEEK_SET));
 if(w){if(writes++==failat)return -1;return fwrite(p,512,n,f)!=n;}
 return fread(p,512,n,f)!=n;
}
static int flush(int d){assert(!d);return fflush(f);}
extern int ntfs_resize_native(Medium*,DiskEdit*,int);
extern const char*ntfs_resize_log(void);
int main(int argc,char**argv){
 assert(argc==5);f=fopen(argv[1],"r+b");assert(f);
 u32 old=strtoul(argv[2],0,10),target=strtoul(argv[3],0,10);
 maximum=MAX(old,target);sectors=start+maximum+2048;
 Medium m={io,flush,0,sectors};
 DiskEdit q={.op=DISK_EDIT_RESIZE,.start=start,.sectors=old,.size=target};
 int reject=!strcmp(argv[4],"reject"),fault=!strcmp(argv[4],"fault");
 /* Repetition exercises exit/error cleanup, allocator reset and upstream
  * globals without relying on process teardown between operations. */
 for(int pass=0;pass<3;pass++){
  int r=ntfs_resize_native(&m,&q,0);
  printf("Preview %d: %d, writes %d\n%s",pass,r,writes,ntfs_resize_log());assert(!writes);
  if(reject)assert(r<0);else assert(!r);
 }
 if(!reject){
  if(fault)failat=2;
  int r=ntfs_resize_native(&m,&q,1);
  printf("Apply: %d, writes %d\n%s",r,writes,ntfs_resize_log());
  if(fault)assert(r==DE_IO&&writes>failat);
  else{
   assert(!r&&writes);u8 a[512],b[512];
   assert(!io(0,start,1,a,0)&&!io(0,start+target-1,1,b,0));assert(!memcmp(a,b,512));
   writes=0;q.sectors=target;q.size=target-2048;
   r=ntfs_resize_native(&m,&q,0);assert(r<0&&!writes);
   puts("PASS post-resize dirty volume correctly requires Windows check before another resize");
  }
 }
 fclose(f);return 0;
}
