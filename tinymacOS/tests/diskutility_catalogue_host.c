#include <assert.h>
#include <stdio.h>
#include "../userspace/lib.h"
static long hostcall(long,u64,u64,u64,u64);
#undef sc
#define sc(n,a,b,c,d) hostcall(n,(u64)(a),(u64)(b),(u64)(c),(u64)(d))
#include "../userspace/diskutility.c"
static int verifications,total=43,fail_at=-1,drop_first;
static char verified[28],opened[PATHLEN];
static int opens,missing;
static void entry(SnapInfo *s,int index){
 memset(s,0,sizeof *s);s->id=3+index;
 if(!index)strset(s->name,"Copia locale",sizeof s->name);
 else{strset(s->name,"punto-utente-",sizeof s->name);addnum(s->name,index,sizeof s->name);}
}
static long hostcall(long n,u64 a,u64 b,u64 c,u64 z){
 if(n==S_DISK&&a==0)return 0;
 if(n==S_STAT){assert(!memcmp((char*)a,"/.snapshots/",12));if(missing)return -1;*(Dirent*)b=(Dirent){.dir=1,.ro=1};return 0;}
 if(n==S_SPAWN){assert(a==FINDER);strset(opened,(char*)b,sizeof opened);opens++;return 1;}
 if(n==S_SNAP&&a==6){memset((void*)z,0,sizeof ei);return 0;}
 if(n==S_SNAP&&a==5){FsInfo *f=(void*)c;memset(f,0,sizeof *f);f->snaps=total;return 0;}
 if(n==S_SNAP&&a==7){
  assert(z<=16&&b<=0x7fffffffu);if((int)b==fail_at)return -1;
  int count=MAX(0,MIN((int)z,total-(int)b));
  for(int i=0;i<count;i++)entry((SnapInfo*)c+i,(int)b+i+drop_first);
  return count;
 }
 if(n==S_SNAP&&a==3){verifications++;strset(verified,(char*)b,sizeof verified);return 0;}
 assert(!"unexpected syscall in catalogue test");return -1;
}
static int catalogue_rows(void){int disk,v,n=0;char label[64];for(int row=0;side_row(row,&disk,&v,label);row++){
 assert(row<100);if(v>=100&&v<200){assert(disk==d.current);n++;}
 }return n;}
int main(void){
 d.count=2;d.current=0;d.disk[0].layout=d.disk[1].layout=2;sel=0;ah=460;
 refresh();assert(nsn==16&&snapmore&&!snaperror);assert(catalogue_rows()==16);
 side_wheel(-16);assert(side_scroll>0);
 int disk,v,found_next=0;char label[64];
 for(int i=side_scroll;i<side_scroll+MAX(1,(ah-51)/27);i++)
  if(side_row(i,&disk,&v,label)&&v==SNAP_NEXT)found_next=1;
 assert(found_next);side_wheel(16);assert(!side_scroll);
 view=100;verify_now();assert(verifications==1&&eq(verified,"Copia locale"));
 snapshot_open();assert(opens==1&&eq(opened,"/.snapshots/Copia locale"));
 missing=1;snapshot_open();assert(opens==1&&msg[0]);missing=0;
 sel=1;snapshot_open();assert(opens==1);assert(!catalogue_rows());view=100;verify_now();assert(verifications==1&&eq(msg,"Seleziona uno snapshot nella colonna laterale."));
 sel=0;si.live_cd=1;assert(!catalogue_rows());verify_now();assert(verifications==1);
 si.live_cd=0;d.current=-1;assert(!catalogue_rows());verify_now();assert(verifications==1);
 d.current=0;view=1;
 /* Every entry reachable exactly once, including beyond the former 8 slots. */
 int seen[43]={0},pages=0,local=0;
 do{
  for(int i=0;i<nsn;i++){int id=sn[i].id-3;assert(id>=0&&id<43);assert(!seen[id]++);if(eq(sn[i].name,"Copia locale"))local++;}
  pages++;if(!snapmore)break;side_select(0,SNAP_NEXT);side_reveal();assert(view==100&&side_scroll>=0);
 }while(pages<5);
 assert(pages==3&&snapbase==32&&nsn==11&&!snapmore&&local==1);
 for(int i=0;i<43;i++)assert(seen[i]==1);
 verify_now();assert(eq(verified,"punto-utente-32"));snapshot_open();assert(opens==2&&eq(opened,"/.snapshots/punto-utente-32"));
 /* Up from first snapshot activates Previous and lands on the previous last. */
 side_move(-1);assert(snapbase==16&&nsn==16&&view==115);
 side_move(1);assert(snapbase==32&&view==100);
 /* A disappearing last page clamps to the surviving page. */
 total=16;refresh();assert(!snapbase&&nsn==16&&!snapmore&&view==1);
 total=43;refresh();view=105;refresh();assert(view==105);
 /* Preserve selected identity across catalogue reordering; never reuse slot. */
 drop_first=1;refresh();assert(view==104&&eq(sn[4].name,"punto-utente-5"));
 drop_first=6;refresh();assert(view==1);drop_first=0;
 /* A failed page or look-ahead is explicit, not a stale or partial catalogue. */
 fail_at=16;refresh();assert(snaperror&&!nsn&&!snapmore&&msg[0]);
 fail_at=0;refresh();assert(snaperror&&!nsn);
 fail_at=-1;total=0;refresh();assert(!snaperror&&!nsn&&!snapmore);
 total=8;refresh();assert(nsn==8&&!snapmore);total=16;refresh();assert(nsn==16&&!snapmore);
 puts("PASS actual Disk Utility: all 43 snapshots paginated exactly once; local copy unique; wheel reveals Next for mouse; next/previous mouse model and keyboard; disappearing page; stable selection by name; empty and exact-page boundary; explicit read errors; mounted-only catalogue and verification; real Finder snapshot path and vanished/foreign selection refusal");
 return 0;
}
