/* Actual experimental filesystem against disposable sparse files/RAM media.
 * No mkfs wrapper or huge RAM image masquerades as capacity support. */
#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "tmcbfs2.h"
static struct {int fd;u8*ram;u64 blocks,reads,writes,flushes;int cut,event,torn;} media;
static int rd(void*x,u64 b,void*p){(void)x;assert(b<media.blocks);media.reads++;if(media.ram){memcpy(p,media.ram+b*T2_BLOCK,T2_BLOCK);return 0;}return pread(media.fd,p,T2_BLOCK,b*T2_BLOCK)==T2_BLOCK?0:-1;}
static int wr(void*x,u64 b,const void*p){(void)x;assert(b<media.blocks);media.writes++;if(media.cut>=0&&media.event++==media.cut){if(media.torn){assert(media.ram);memcpy(media.ram+b*T2_BLOCK,p,37);}return -1;}if(media.ram){memcpy(media.ram+b*T2_BLOCK,p,T2_BLOCK);return 0;}return pwrite(media.fd,p,T2_BLOCK,b*T2_BLOCK)==T2_BLOCK?0:-1;}
static int fl(void*x){(void)x;media.flushes++;if(media.cut>=0&&media.event++==media.cut)return -1;return media.ram?0:fsync(media.fd);}
static void*al(void*x,size_t n){(void)x;assert(n<=T2_BLOCK+sizeof(T2Entry));return malloc(n);}
static void fr(void*x,void*p){(void)x;free(p);}
static T2Io io(void){return (T2Io){0,media.blocks,rd,wr,fl,al,fr};}
static void large(void){
 char path[]="/var/tmp/tmcbfs2-1TiB-XXXXXXXX";media.fd=mkstemp(path);assert(media.fd>=0);assert(!unlink(path));media.blocks=T2_MAX_BYTES/T2_BLOCK;media.cut=-1;assert(!ftruncate(media.fd,T2_MAX_BYTES));
 Tmc2 c;assert(!tmc2_format(&c,io()));assert(media.writes==2);u64 id;assert(!tmc2_create(&c,1,"large.bin",T2_FILE,&id));
 static u8 a[65536],b[65536];for(unsigned i=0;i<sizeof a;i++)a[i]=(i*17+29)%251;
 assert(tmc2_write(&c,id,0,a,sizeof a)==sizeof a);assert(tmc2_read(&c,id,0,b,sizeof b)==sizeof b&&!memcmp(a,b,sizeof a));
 /* 192 materialized data blocks force the file's extent/map tree to split.
  * Writing a huge logical size alone would not exercise scalable indexing. */
 for(unsigned part=0;part<48;part++){memset(a,part+37,sizeof a);assert(tmc2_write(&c,id,(1ull<<20)+(u64)part*sizeof a,a,sizeof a)==sizeof a);}
 for(unsigned part=0;part<48;part++){assert(tmc2_read(&c,id,(1ull<<20)+(u64)part*sizeof b,b,sizeof b)==sizeof b);for(unsigned j=0;j<sizeof b;j++)assert(b[j]==part+37);}
 for(unsigned j=0;j<sizeof a;j++)a[j]=(j*17+29)%251;
 c.cursor=media.blocks-32;assert(tmc2_write(&c,id,(1ull<<34)+31,a,512)==512);assert(tmc2_write(&c,id,T2_MAX_BYTES-1,"Z",1)==1);
 T2Entry e;assert(!tmc2_stat(&c,id,&e)&&e.size==T2_MAX_BYTES);memset(b,0x77,4096);assert(tmc2_read(&c,id,1ull<<33,b,4096)==4096);for(int i=0;i<4096;i++)assert(!b[i]);
 for(unsigned i=0;i<1000;i++){char s[48];snprintf(s,sizeof s,"file-%04u",i);u64 other;assert(!tmc2_create(&c,1,s,T2_FILE,&other));}
 assert(!tmc2_lookup(&c,1,"file-0999",&e)&&e.key>384);
 u64 cookie=0;unsigned listed=0;int result;u64 enum_reads=media.reads;while((result=tmc2_readdir(&c,1,&cookie,&e))==1)listed++;assert(result==0&&listed==1001);
 assert(media.reads-enum_reads<=2100); /* root+one leaf per entry, no prefix rescan */
 u64 last=cookie;enum_reads=media.reads;assert(!tmc2_readdir(&c,1,&cookie,&e)&&cookie==last&&media.reads-enum_reads<=2);
 cookie=~0ull;assert(!tmc2_readdir(&c,1,&cookie,&e)&&cookie==~0ull);
 puts("PASS directory continuation prunes preceding subtrees: 1001 entries in <=2100 tree reads, stable EOF and UINT64_MAX cookie without wrap");
 u64 before=tmc2_used_blocks(&c);assert(!tmc2_collect(&c));assert(tmc2_used_blocks(&c)<before);
 for(unsigned i=0;i<192;i++){
  char s[48];snprintf(s,sizeof s,"history-%02u",i);memset(a,i,512);assert(tmc2_write(&c,id,0,a,512)==512);assert(!tmc2_snapshot(&c,s,0));
 }
 for(unsigned i=0;i<192;i++){char s[48];snprintf(s,sizeof s,"history-%02u",i);assert(tmc2_snapshot_read(&c,s,id,0,b,512)==512);for(int j=0;j<512;j++)assert(b[j]==i);}
 cookie=0;listed=0;enum_reads=media.reads;while((result=tmc2_snapshots(&c,&cookie,&e))==1)listed++;assert(result==0&&listed==192&&media.reads-enum_reads<=390);
 T2Entry frozen;assert(!tmc2_snapshot_find(&c,"history-03",&frozen));cookie=0;listed=0;enum_reads=media.reads;
 while((result=tmc2_view_readdir(&c,frozen.key,1,&cookie,&e))==1)listed++;assert(result==0&&listed==1001&&media.reads-enum_reads<=4100);
 assert(!tmc2_collect(&c));u64 shared=tmc2_used_blocks(&c);assert(shared<2000); /* no 1TiB clones, no 64KiB file clones per snapshot */
 tmc2_close(&c);u64 reads=media.reads;assert(!tmc2_mount(&c,io()));assert(media.reads-reads<5000);assert(!tmc2_stat(&c,id,&e)&&e.size==T2_MAX_BYTES);
 assert(tmc2_read(&c,id,(1ull<<34)+31,b,512)==512);for(int j=0;j<512;j++)assert(b[j]==(j*17+29)%251);
 assert(tmc2_read(&c,id,T2_MAX_BYTES-1,b,1)==1&&b[0]=='Z');assert(!tmc2_snapshot_restore(&c,"history-03"));assert(tmc2_read(&c,id,0,b,512)==512&&b[0]==3);
 assert(!tmc2_truncate(&c,id,17001));assert(!tmc2_truncate(&c,id,5ull<<20));
 memset(b,0xff,sizeof b);assert(tmc2_read(&c,id,17001,b,sizeof b)==sizeof b);for(unsigned j=0;j<sizeof b;j++)assert(!b[j]);
 assert(tmc2_snapshot_read(&c,"history-03",id,(1ull<<20)+8192,b,512)==512&&b[0]==37);
 u64 dir,nested;assert(!tmc2_create(&c,1,"new-parent",T2_DIR,&dir));assert(!tmc2_create(&c,dir,"nested",T2_DIR,&nested));
 assert(tmc2_move(&c,dir,nested,"cycle")<0);assert(!tmc2_move(&c,id,dir,"renamed.bin"));assert(!tmc2_lookup(&c,dir,"renamed.bin",&e)&&e.key==id);
 assert(tmc2_remove(&c,dir)<0);assert(!tmc2_remove(&c,nested));assert(!tmc2_move(&c,id,1,"large.bin"));assert(!tmc2_remove(&c,dir));
 puts("PASS scalable truncate/extend zeroes hidden tails, prunes split map trees without copying retained data, preserves snapshot bytes; atomic rename/move refuses directory cycles and nonempty removal");
 for(unsigned i=0;i<192;i++){char s[48];snprintf(s,sizeof s,"history-%02u",i);assert(!tmc2_snapshot_delete(&c,s));}
 assert(!tmc2_collect(&c));assert(tmc2_used_blocks(&c)<shared);assert(!tmc2_remove(&c,id));assert(!tmc2_collect(&c));
 assert(tmc2_stat(&c,id,&e)<0);
 memset(a,'A',512);assert(!tmc2_account(&c,0,a,1));assert(!tmc2_snapshot(&c,"credentials-before",0));
 memset(a,'B',512);assert(!tmc2_account(&c,0,a,1));assert(!tmc2_collect(&c));tmc2_close(&c);assert(!tmc2_mount(&c,io()));
 assert(!tmc2_account(&c,0,b,0));for(unsigned j=0;j<512;j++)assert(b[j]=='B');
 assert(!tmc2_snapshot_restore(&c,"credentials-before"));assert(!tmc2_account(&c,0,b,0));for(unsigned j=0;j<512;j++)assert(b[j]=='A');
 assert(!tmc2_snapshot_delete(&c,"credentials-before"));assert(!tmc2_collect(&c));
 puts("PASS private account roots survive remount/GC, are snapshot-shared and restored without public namespace entries; no allocation larger than one tree page on the 1TiB volume");tmc2_close(&c);struct stat st;assert(!fstat(media.fd,&st));close(media.fd);
 printf("PASS actual TMCBFS2 core: 1TiB sparse volume, multi-MiB materialized file and split extent-tree I/O, file offsets above 16GiB and at 1TiB-1, high physical blocks, 1000 files, 192 real shared-root CoW snapshots, reboot/restore/delete and last-reference collection; fixture allocated %llu bytes\n",(unsigned long long)st.st_blocks*512);
}
static void faults(void){
 memset(&media,0,sizeof media);media.fd=-1;media.cut=-1;media.blocks=256;size_t bytes=media.blocks*T2_BLOCK;media.ram=calloc(1,bytes);assert(media.ram);Tmc2 c;assert(!tmc2_format(&c,io()));u64 id;assert(!tmc2_create(&c,1,"stable",T2_FILE,&id));u8 a[512],b[512];memset(a,'A',512);assert(tmc2_write(&c,id,0,a,512)==512);assert(!tmc2_account(&c,0,a,1));assert(!tmc2_snapshot(&c,"protected",0));assert(!tmc2_collect(&c));tmc2_close(&c);
 u8*original=malloc(bytes);assert(original);memcpy(original,media.ram,bytes);
 for(int torn=0;torn<2;torn++)for(int cut=0;cut<10;cut++){
  memcpy(media.ram,original,bytes);media.cut=-1;assert(!tmc2_mount(&c,io()));media.cut=cut;media.event=0;media.torn=torn;memset(a,'B',512);i64 r=tmc2_write(&c,id,0,a,512);
  if(r<0){assert(c.faulted);assert(tmc2_write(&c,id,0,a,512)<0);}tmc2_close(&c);media.cut=-1;
  assert(!tmc2_mount(&c,io()));assert(tmc2_read(&c,id,0,b,512)==512);u8 which=b[0];assert(which=='A'||which=='B');for(int j=0;j<512;j++)assert(b[j]==which);
  assert(tmc2_snapshot_read(&c,"protected",id,0,b,512)==512);for(int j=0;j<512;j++)assert(b[j]=='A');assert(!tmc2_collect(&c));tmc2_close(&c);
 }
 /* Interrupted atomic replacement must retain exactly one old/new checkpoint,
  * never a missing backup caused by delete-then-create. */
 for(int torn=0;torn<2;torn++)for(int cut=0;cut<16;cut++){
  memcpy(media.ram,original,bytes);media.cut=-1;assert(!tmc2_mount(&c,io()));memset(a,'B',512);assert(tmc2_write(&c,id,0,a,512)==512);
  media.cut=cut;media.event=0;media.torn=torn;int r=tmc2_checkpoint(&c,"protected");if(r<0)assert(c.faulted);tmc2_close(&c);media.cut=-1;
  assert(!tmc2_mount(&c,io()));assert(tmc2_snapshot_read(&c,"protected",id,0,b,512)==512);u8 which=b[0];assert(which=='A'||which=='B');for(int j=0;j<512;j++)assert(b[j]==which);
  u64 cookie=0;T2Entry snap;int n=0,step;while((step=tmc2_snapshots(&c,&cookie,&snap))==1)n++;assert(!step&&n==1);assert(!tmc2_verify(&c));tmc2_close(&c);
 }
 puts("PASS atomic checkpoint replacement: 32 write/flush/torn-publication faults retain exactly one readable old/new backup, including data integrity and catalogue uniqueness after reboot");
 /* Reset must never expose a new blank account with old user files, nor the
  * inverse, even if publication is interrupted. Both fallback roots retired
  * only after a successful second publication (collection). */
 const T2Seed seed[]={{0,T2_DIR,"Users"},{1,T2_DIR,"User"},{2,T2_DIR,"Documents"}};
 for(int torn=0;torn<2;torn++)for(int cut=0;cut<14;cut++){
  memcpy(media.ram,original,bytes);media.cut=-1;assert(!tmc2_mount(&c,io()));
  u64 next=c.super.next_inode,ns=c.super.next_snapshot,writes=media.writes;
  const T2Seed invalid[]={{0,T2_DIR,"bad"},{1,T2_FILE,"../invalid"}};assert(tmc2_reset(&c,invalid,2)<0&&writes==media.writes);
  const T2Seed duplicate[]={{0,T2_DIR,"same"},{0,T2_FILE,"same"}};assert(tmc2_reset(&c,duplicate,2)<0&&writes==media.writes);
  media.cut=cut;media.event=0;media.torn=torn;int r=tmc2_reset(&c,seed,3);if(!r)r=tmc2_collect(&c);tmc2_close(&c);media.cut=-1;
  assert(!tmc2_mount(&c,io()));assert(!tmc2_account(&c,0,b,0));T2Entry entry;int fresh=!tmc2_lookup(&c,1,"Users",&entry);
  if(fresh){
   assert(entry.key==next&&c.super.next_inode==next+3&&c.super.next_snapshot==ns&&tmc2_stat(&c,id,&entry)<0);
   for(unsigned j=0;j<512;j++)assert(!b[j]);u64 cookie=0;assert(!tmc2_snapshots(&c,&cookie,&entry));
   assert(!tmc2_lookup(&c,1,"Users",&entry)&&!tmc2_lookup(&c,entry.key,"User",&entry)&&!tmc2_lookup(&c,entry.key,"Documents",&entry)&&entry.kind==T2_DIR);
  }else{for(unsigned j=0;j<512;j++)assert(b[j]=='A');assert(tmc2_read(&c,id,0,b,512)==512&&b[0]=='A');assert(!tmc2_snapshot_verify(&c,"protected"));}
  if(!r)assert(fresh&&c.slots[0].live==c.slots[1].live&&!c.slots[0].control&&!c.slots[1].snapshots);
  assert(!tmc2_verify(&c));tmc2_close(&c);
 }
 puts("PASS atomic logical factory reset: 28 write/flush/torn faults retain either complete old account/files/snapshots or fresh directories/blank account/no snapshots; monotonic IDs, invalid plans refuse before writes, both fallback roots retired on success (not secure erase)");
 memcpy(media.ram,original,bytes);media.ram[0]^=0x80;assert(!tmc2_mount(&c,io()));tmc2_close(&c);media.ram[T2_BLOCK]^=0x80;u64 writes=media.writes;assert(tmc2_mount(&c,io())<0&&media.writes==writes);
 memcpy(media.ram,original,bytes);assert(!tmc2_mount(&c,io()));
 T2Entry file,map;assert(!tmc2_stat(&c,id,&file));/* first immutable MAP leaf entry */
 memcpy(&map,media.ram+file.root*T2_BLOCK+64,sizeof map);media.ram[map.parent*T2_BLOCK+17]^=0x40;
 writes=media.writes;assert(tmc2_read(&c,id,0,b,512)<0&&c.faulted&&media.writes==writes);tmc2_close(&c);
 memcpy(media.ram,original,bytes);assert(!tmc2_mount(&c,io()));u64 live=c.super.live;tmc2_close(&c);media.ram[live*T2_BLOCK+65]^=0x20;
 writes=media.writes;assert(tmc2_mount(&c,io())<0&&media.writes==writes);
 free(original);free(media.ram);media.ram=0;
 puts("PASS TMCBFS2 write/flush/torn-publication fault matrix: old or new whole-block state after remount, snapshot unchanged, faulted mounts refuse retries; one corrupt superblock fallback, two corrupt superblocks and tree/data checksum damage refuse without formatting");
}
int main(void){large();faults();return 0;}
