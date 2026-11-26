#include <assert.h>
#include <stdio.h>
#include "../userspace/ui.h"
static int total=33;
static long hostcall(long n,u64 a,u64 b,u64 c,u64 d){
 assert(n==S_SNAP);
 if(a==7){int count=0;for(int i=b;i<total&&count<(int)d;i++){SnapInfo*e=(SnapInfo*)c+count++;*e=(SnapInfo){.id=i+1};sprintf(e->name,"snapshot-%04d",i);}return count;}
 if(a==5){*(FsInfo*)c=(FsInfo){.snaps=total};return 0;}
 if(a==6){*(EncInfo*)d=(EncInfo){0};return 0;}
 assert(0);return -1;
}
#undef sc
#define sc(n,a,b,c,d) hostcall(n,(u64)(a),(u64)(b),(u64)(c),(u64)(d))
#include "../userspace/timemachine.c"
SysInfo si;int aw=600,ah=430;
int main(void){
 refresh();assert(nsn==8&&more&&fi.snaps==33);int seen=0;
 do{for(int i=0;i<nsn;i++){char expected[48];sprintf(expected,"snapshot-%04d",seen++);assert(eq(sn[i].name,expected));}if(!more)break;sel=0;timeline_page(1);assert(sel==-1);}while(1);
 assert(seen==33&&pagebase==32&&nsn==1&&!more);timeline_page(1);assert(pagebase==32);
 total=9;refresh();assert(pagebase==8&&nsn==1&&!more);timeline_page(-1);assert(!pagebase&&nsn==8&&more);
 puts("PASS real Time Machine: 33 snapshots in bounded 8-entry timeline pages, correct total count, previous/next navigation, no unreachable snapshots, retreat after delete");
}
