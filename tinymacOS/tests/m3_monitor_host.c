#include <assert.h>
#include <stdio.h>
#include "../userspace/ui.h"
static SysInfo fixture;static ProcInfo processes[NPROC];static int nr=2;
static long hostcall(long,u64,u64,u64,u64);
#undef sc
#define sc(n,a,b,c,d) hostcall(n,(u64)(a),(u64)(b),(u64)(c),(u64)(d))
#define op ui_draw_op
#include "../userspace/ui.c"
#undef op
#include "../userspace/monitor.c"
static long hostcall(long k,u64 a,u64 b,u64 c,u64 d){
 (void)b;(void)c;(void)d;
 if(k==S_INFO){*(SysInfo *)a=fixture;return 0;}
 if(k==S_PS){memcpy((void *)a,processes,nr*sizeof *processes);return nr;}
 assert(k==S_WDRAW);return 0;
}
int TextWidth(const char *s,int f){(void)f;return strlen(s)*6;}
static int contains(const char *s){for(int i=0;i<count;i++)if(list[i].type==C_TEXT&&eq(list[i].text,s))return 1;return 0;}
static void step(u64 delta,int load){
 fixture.ticks+=delta;
 for(int c=0;c<8;c++)if(fixture.cpu_online_mask&(1u<<c)){fixture.core_ticks[c]+=delta;if(load)fixture.core_busy[c]+=delta;}
 for(int j=0;j<nr;j++){processes[j].sample_ticks+=delta*fixture.ncpu;if(j==0&&load)processes[j].cpu_ticks+=delta;}
 sample();
}
int main(void){
 fixture=(SysInfo){.total=128*1024*1024,.free=32*1024*1024,.ram_bytes=256*1024*1024,.reserved_bytes=128*1024*1024,.user_bytes=12*1024*1024,.ncpu=8,.cpu_online_mask=255};
 processes[0]=(ProcInfo){.pid=7,.state=1,.sample_ticks=800,.cpu_ticks=70,.resident_bytes=8*1024*1024,.threads=8};strset(processes[0].name,"Carico",24);
 processes[1]=(ProcInfo){.pid=9,.state=2,.sample_ticks=800,.cpu_ticks=3};strset(processes[1].name,"Sleep",24);
 for(int c=0;c<8;c++){fixture.core_ticks[c]=100;fixture.core_busy[c]=10;}
 sample();step(100,1);assert(cpu==100&&rowcpu[0]==12&&!rowcpu[1]);
 sort=1;processes[1].cpu_ticks+=700;step(100,0);assert(rows[0].pid==9&&rowcpu[0]==87&&rows[1].pid==7);
 for(int k=0;k<60;k++)step(100,0);assert(hlen==42&&cpu==0);
 have_sample=0;sample();assert(cpu==0&&!rowcpu[0]&&!rowcpu[1]);
 fixture.cpu_online_mask=129;fixture.ncpu=2;step(100,1);assert(cpu==0&&!corep[1]);step(100,1);assert(cpu==100&&rowcpu[0]==50&&corep[7]==100);
 /* Counter wrap/reset and no-progress never fabricate a full CPU. */
 fixture.core_ticks[7]=1;fixture.core_busy[7]=0;sample();assert(cpu==0);step(100,0);assert(cpu==0);
 fixture.ncpu=8;fixture.cpu_online_mask=255;sample();
 int sizes[]={660,820,1280};
 for(unsigned k=0;k<COUNT(sizes);k++){
  aw=sizes[k];ah=620;paint();assert(contains("CPU 7")&&contains("RAM UTENTE")&&contains("% CPU")&&count<NCMDS);
  for(int j=0;j<count;j++)if(list[j].type==C_RECT){assert(list[j].w>=0&&list[j].h>=0&&list[j].x>=0&&list[j].x+list[j].w<=aw);}
  fixture.free=fixture.total+4096;si=fixture;paint(); /* bad input cannot underflow progress bar */
 }
 puts("PASS actual Monitor sample/paint: interval process CPU and sort, pause/rebaseline, 42 samples, 8 cores plus sparse AP mask, reset/wrap, bounded memory bar and serializer at 660/820/1280px");
}
