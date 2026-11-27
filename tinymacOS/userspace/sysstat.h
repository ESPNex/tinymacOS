/* Native diagnostics: raw allocator and scheduler counters, not fabricated CPU
 * percentages or benchmarks. Included by the shared Command executable. */
#include "vmabi.h"
static volatile unsigned stat_mode,stat_done;
static void stat_worker(u64 id)
{
    for(;;){
        unsigned m=__atomic_load_n(&stat_mode,__ATOMIC_ACQUIRE);
        if(m==3)break;
        if(m==2||(m==1&&!id))asm volatile("pause":::"memory");
        else slp(1);
    }
    __atomic_fetch_add(&stat_done,1,__ATOMIC_RELEASE);
    sc(S_EXIT,1,0,0,0);for(;;){}
}
static void stat_record(const char *phase)
{
    SysInfo s={0};ProcInfo p[NPROC];sc(S_INFO,&s,0,0,0);
    int n=sc(S_PS,p,NPROC,0,0);int self=s.current_pid;
    say("{\"phase\":\"");say(phase);say("\",\"wall\":");num(s.ticks);
    say(",\"busy\":");num(s.busy_ticks);say(",\"mask\":");num(s.cpu_online_mask);say(",\"mem\":[");num(s.total);say(",");num(s.free);say(",");num(s.ram_bytes);say(",");num(s.reserved_bytes);say(",");num(s.user_bytes);
    say("],\"cores\":[");for(int c=0;c<8;c++){
        if(c)say(",");say("[");num(s.core_ticks[c]);say(",");num(s.core_busy[c]);say("]");
    }
    say("],\"self\":[");
    for(int i=0;i<n;i++)if(p[i].pid==self){num(p[i].pid);say(",");num(p[i].sample_ticks);say(",");num(p[i].cpu_ticks);say(",");num(p[i].resident_bytes);say(",");num(p[i].threads);}
    ln("]}");
}
static int sysstat(void)
{
    if(A.argc==1){stat_record("snapshot");return 0;}
    if(A.argc!=2||!eq(V(1),"test"))return fail("Uso: sysstat [test] (test: carico CPU e 8.4 MB temporanei)");
    SysInfo s={0};sc(S_INFO,&s,0,0,0);
    unsigned workers=MIN(8,MAX(1,s.ncpu));
    for(unsigned i=0;i<workers;i++)if(sc(S_THREAD,stat_worker,i,0,0)<0){__atomic_store_n(&stat_mode,3,__ATOMIC_RELEASE);return fail("Thread di prova non disponibile.");}
    slp(30);stat_record("idle0");slp(160);stat_record("idle1");
    __atomic_store_n(&stat_mode,1,__ATOMIC_RELEASE);slp(20);stat_record("one0");slp(200);stat_record("one1");
    __atomic_store_n(&stat_mode,2,__ATOMIC_RELEASE);slp(20);stat_record("multi0");slp(200);stat_record("multi1");
    __atomic_store_n(&stat_mode,0,__ATOMIC_RELEASE);slp(30);stat_record("sleep0");slp(160);stat_record("sleep1");
    stat_record("mem0");
    long a=sc(S_ALLOC,8*1024*1024,0,0,0);if(a<0)return fail("RAM insufficiente per la prova.");
    for(unsigned i=0;i<8*1024*1024;i+=4096)((volatile u8 *)a)[i]=i/4096;
    stat_record("alloc");sc(S_FREE,a,0,0,0);stat_record("free");
    long v=sc(S_VM,TMC_VM_RESERVE,4*1024*1024,4096,0);
    if(v<0||sc(S_VM,TMC_VM_COMMIT,v,4*1024*1024,TMC_VM_READ|TMC_VM_WRITE)<0)return fail("Commit VM non disponibile.");
    stat_record("commit");
    if(sc(S_VM,TMC_VM_RELEASE,v,4*1024*1024,0)<0)return fail("Rilascio VM non riuscito.");
    stat_record("release");__atomic_store_n(&stat_mode,3,__ATOMIC_RELEASE);
    for(unsigned wait=0;__atomic_load_n(&stat_done,__ATOMIC_ACQUIRE)<workers&&wait<200;wait++)slp(1);
    return stat_done==workers?0:1;
}
