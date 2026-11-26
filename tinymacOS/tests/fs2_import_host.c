/* Real external checkpoint transplantation with independent media and complete
 * write/flush/torn-write interruption sweep. No whole-volume RAM staging. */
#define _FILE_OFFSET_BITS 64
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "tmcbfs2.h"
typedef struct {int fd;u8*ram;u64 blocks,writes,reads,flushes;int cut,event,torn;} Media;
static int rd(void*x,u64 b,void*p){Media*m=x;assert(b<m->blocks);m->reads++;if(m->ram){memcpy(p,m->ram+b*T2_BLOCK,T2_BLOCK);return 0;}return pread(m->fd,p,T2_BLOCK,b*T2_BLOCK)==T2_BLOCK?0:-1;}
static int wr(void*x,u64 b,const void*p){Media*m=x;assert(b<m->blocks);m->writes++;if(m->cut>=0&&m->event++==m->cut){if(m->torn){assert(m->ram);memcpy(m->ram+b*T2_BLOCK,p,71);}return -1;}if(m->ram){memcpy(m->ram+b*T2_BLOCK,p,T2_BLOCK);return 0;}return pwrite(m->fd,p,T2_BLOCK,b*T2_BLOCK)==T2_BLOCK?0:-1;}
static int fl(void*x){Media*m=x;m->flushes++;if(m->cut>=0&&m->event++==m->cut)return -1;return m->ram?0:fsync(m->fd);}
static void*al(void*x,size_t n){(void)x;assert(n<=T2_BLOCK+sizeof(T2Entry));return malloc(n);}
static void fr(void*x,void*p){(void)x;free(p);}
static T2Io io(Media*m){return (T2Io){m,m->blocks,rd,wr,fl,al,fr};}
static void medium(Media*m,u64 blocks,int sparse){*m=(Media){.fd=-1,.blocks=blocks,.cut=-1};if(sparse){char path[]="/var/tmp/tmc-import-XXXXXXXX";m->fd=mkstemp(path);assert(m->fd>=0&&!unlink(path)&&!ftruncate(m->fd,blocks*T2_BLOCK));}else{m->ram=calloc(blocks,T2_BLOCK);assert(m->ram);}}
static void done(Media*m){if(m->fd>=0)close(m->fd);free(m->ram);}
static void account(Tmc2*c,int value,int write){u8 p[512];memset(p,value,sizeof p);if(write)assert(!tmc2_account(c,0,p,1));else{assert(!tmc2_account(c,0,p,0));for(unsigned j=0;j<sizeof p;j++)assert(p[j]==value);}}
static u64 file(Tmc2*c,const char*name,const char*text){u64 id;assert(!tmc2_create(c,1,name,T2_FILE,&id));assert(tmc2_write(c,id,0,text,strlen(text))==(i64)strlen(text));return id;}
static void content(Tmc2*c,const char*name,const char*text){T2Entry e;u8 p[128];assert(!tmc2_lookup(c,1,name,&e)&&e.size==strlen(text)&&tmc2_read(c,e.key,0,p,e.size)==(i64)e.size&&!memcmp(p,text,e.size));}
static void scalable(void){
 Media sm,dm;medium(&sm,T2_MAX_BYTES/T2_BLOCK,1);medium(&dm,16384,1);Tmc2 s,d;assert(!tmc2_format(&s,io(&sm))&&!tmc2_format(&d,io(&dm)));
 u64 large;assert(!tmc2_create(&s,1,"large",T2_FILE,&large));static u8 a[65536],b[65536];
 for(unsigned k=0;k<48;k++){memset(a,k+11,sizeof a);assert(tmc2_write(&s,large,k*sizeof a,a,sizeof a)==sizeof a);}
 s.cursor=sm.blocks-20;assert(tmc2_write(&s,large,(1ull<<34)+19,"HIGH",4)==4);
 for(unsigned j=0;j<1100;j++){char name[48];snprintf(name,sizeof name,"file-%04u",j);file(&s,name,"source");}
 file(&s,".hidden-name","visible-dotfile");account(&s,0x39,1);assert(!tmc2_snapshot(&s,"external",0));assert(tmc2_write(&s,large,0,"NEW-LIVE",8)==8);account(&s,0x77,1);
 file(&d,"old","old destination");account(&d,0x21,1);assert(!tmc2_snapshot(&d,"before-import",0));u64 writes=sm.writes,flushes=sm.flushes,dw=dm.writes,needed=0;
 assert(!tmc2_import_check(&d,&s,"external",&needed)&&needed>1200&&dm.writes==dw&&sm.writes==writes&&sm.flushes==flushes);
 assert(!tmc2_import_snapshot(&d,&s,"external"));assert(sm.writes==writes&&sm.flushes==flushes);assert(!tmc2_verify(&d));account(&d,0x39,0);
 T2Entry e;assert(!tmc2_lookup(&d,1,"large",&e));
 for(unsigned k=0;k<48;k++){assert(tmc2_read(&d,e.key,k*sizeof b,b,sizeof b)==sizeof b);for(unsigned j=0;j<sizeof b;j++)assert(b[j]==k+11);}
 assert(tmc2_read(&d,e.key,(1ull<<34)+19,b,4)==4&&!memcmp(b,"HIGH",4));assert(!tmc2_lookup(&d,1,"file-1099",&e));content(&d,".hidden-name","visible-dotfile");
 assert(tmc2_lookup(&d,1,"old",&e)<0);assert(!tmc2_snapshot_verify(&d,"before-import"));tmc2_close(&d);assert(!tmc2_mount(&d,io(&dm)));account(&d,0x39,0);assert(!tmc2_snapshot_restore(&d,"before-import"));content(&d,"old","old destination");account(&d,0x21,0);
 assert(tmc2_import_check(&d,&s,"absent",0)<0&&tmc2_import_snapshot(&d,&d,"before-import")<0);
 tmc2_close(&s);tmc2_close(&d);done(&sm);done(&dm);
 puts("PASS external import: source 1TiB sparse medium, 1100+ files, split inode/map trees, 3MiB materialized + 16GiB sparse offset, exact snapshot not changed live bytes, private account, dotfiles, retained destination snapshots, remount/rollback; source zero writes/flushes, bounded one-page allocations");
}
static void interruptions(void){
 Media sm,dm;medium(&sm,256,0);medium(&dm,256,0);Tmc2 s,d;assert(!tmc2_format(&s,io(&sm))&&!tmc2_format(&d,io(&dm)));file(&s,"imported","new files");account(&s,0x41,1);assert(!tmc2_snapshot(&s,"external",0));file(&d,"old","old files");account(&d,0x42,1);assert(!tmc2_snapshot(&d,"before",0));tmc2_close(&d);
 size_t bytes=dm.blocks*T2_BLOCK;u8*saved=malloc(bytes),*source=malloc(sm.blocks*T2_BLOCK);assert(saved&&source);memcpy(saved,dm.ram,bytes);memcpy(source,sm.ram,sm.blocks*T2_BLOCK);
 assert(!tmc2_mount(&d,io(&dm)));dm.event=0;dm.cut=100000;assert(!tmc2_import_snapshot(&d,&s,"external"));int events=dm.event;tmc2_close(&d);
 for(int torn=0;torn<2;torn++)for(int cut=0;cut<events;cut++){
  memcpy(dm.ram,saved,bytes);dm.cut=-1;assert(!tmc2_mount(&d,io(&dm)));dm.cut=cut;dm.event=0;dm.torn=torn;int r=tmc2_import_snapshot(&d,&s,"external");assert(r<0);tmc2_close(&d);dm.cut=-1;assert(!tmc2_mount(&d,io(&dm))&&!tmc2_verify(&d));T2Entry e;
  if(!tmc2_lookup(&d,1,"old",&e)){content(&d,"old","old files");account(&d,0x42,0);assert(tmc2_lookup(&d,1,"imported",&e)<0);}else{content(&d,"imported","new files");account(&d,0x41,0);assert(tmc2_lookup(&d,1,"old",&e)<0);}
  assert(!tmc2_snapshot_restore(&d,"before"));content(&d,"old","old files");account(&d,0x42,0);tmc2_close(&d);assert(!memcmp(source,sm.ram,sm.blocks*T2_BLOCK));
 }
 printf("PASS external import: %d complete write/flush and torn-write interruption points, cold remount yields whole old OR new namespace/account; previous destination checkpoint always restorable; source byte-identical\n",events*2);
 /* Corrupt source data: refuse before any destination write. */
 memcpy(dm.ram,saved,bytes);assert(!tmc2_mount(&d,io(&dm)));T2Entry e;assert(!tmc2_lookup(&s,1,"imported",&e));u8*tree=sm.ram+e.root*T2_BLOCK;T2Entry*map=(void*)(tree+64);sm.ram[map->parent*T2_BLOCK]^=1;u64 w=dm.writes;assert(tmc2_import_snapshot(&d,&s,"external")<0&&dm.writes==w);tmc2_close(&d);tmc2_close(&s);free(saved);free(source);done(&sm);done(&dm);
 puts("PASS source checksum corruption refuses import with zero destination writes");
}
static void full(void){
 Media sm,dm;medium(&sm,512,0);medium(&dm,128,0);Tmc2 s,d;assert(!tmc2_format(&s,io(&sm))&&!tmc2_format(&d,io(&dm)));u64 id;assert(!tmc2_create(&s,1,"too-large",T2_FILE,&id));static u8 b[T2_BLOCK];memset(b,0x93,sizeof b);for(unsigned k=0;k<150;k++)assert(tmc2_write(&s,id,k*T2_BLOCK,b,sizeof b)==sizeof b);assert(!tmc2_snapshot(&s,"external",0));u64 w=dm.writes;assert(tmc2_import_check(&d,&s,"external",0)<0&&tmc2_import_snapshot(&d,&s,"external")<0&&dm.writes==w);tmc2_close(&s);tmc2_close(&d);done(&sm);done(&dm);puts("PASS insufficient staging space refuses before destination writes");
}
int main(void){scalable();interruptions();full();return 0;}
