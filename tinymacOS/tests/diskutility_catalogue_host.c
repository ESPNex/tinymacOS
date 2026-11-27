#include <assert.h>
#include <stdio.h>
#include "../userspace/lib.h"
static long hostcall(long,u64,u64,u64,u64);
#undef sc
#define sc(n,a,b,c,d) hostcall(n,(u64)(a),(u64)(b),(u64)(c),(u64)(d))
#include "../userspace/diskutility.c"
static int verifications;
static long hostcall(long n,u64 a,u64 b,u64 c,u64 z){
 (void)b;(void)c;(void)z;
 if(n==S_SNAP&&a==3){verifications++;return 0;}
 assert(!"unexpected syscall in catalogue test");return -1;
}
static int catalogue_rows(void){int disk,v,n=0;char label[64];for(int row=0;side_row(row,&disk,&v,label);row++){
 assert(row<100);if(v>=100&&v<200){assert(disk==d.current);n++;}
 }return n;}
int main(void){
 d.count=2;d.current=0;d.disk[0].layout=d.disk[1].layout=2;sel=0;nsn=2;
 strset(sn[0].name,"Copia locale",28);sn[0].id=3;strset(sn[1].name,"punto-utente",28);sn[1].id=8;
 assert(catalogue_rows()==2);view=100;verify_now();assert(verifications==1);
 sel=1;assert(!catalogue_rows());view=100;verify_now();assert(verifications==1&&eq(msg,"Seleziona uno snapshot nella colonna laterale."));
 sel=0;si.live_cd=1;assert(!catalogue_rows());verify_now();assert(verifications==1);
 si.live_cd=0;d.current=-1;assert(!catalogue_rows());verify_now();assert(verifications==1);
 puts("PASS actual Disk Utility catalogue: mounted snapshots including Copia locale listed once, never attributed to another selected disk/live/Recovery; foreign selection cannot verify the mounted snapshot by mistake");
 return 0;
}
