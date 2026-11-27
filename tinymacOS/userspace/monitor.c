#include "ui.h"
#include "display_size.h"
#include "cpu_sample.h"
static ProcInfo rows[NPROC],previous[NPROC];
static int n, paused, sort, selected=-1, cpu, history[42],hlen;
static int rowcpu[NPROC], corep[8], have_sample, oldnc;
static u64 lastTick,ptick[8],pbusy[8];
static void order(void)
{
    for(int i=0;i<n;i++)for(int j=i+1;j<n;j++)
        if(sort?(rowcpu[j]>rowcpu[i]||(rowcpu[j]==rowcpu[i]&&rows[j].pid<rows[i].pid)):rows[j].pid<rows[i].pid){
            ProcInfo v=rows[i];rows[i]=rows[j];rows[j]=v;
            int c=rowcpu[i];rowcpu[i]=rowcpu[j];rowcpu[j]=c;
        }
}
static void sample(void)
{
    SysInfo t={0};
    if(sc(S_INFO,&t,0,0,0)<0)return;
    si=t;lastTick=t.ticks;
    int nc=MIN(8,(int)t.ncpu);if(nc<0)nc=0;
    unsigned mask=t.cpu_online_mask&255;
    u64 total=0,busy=0;int valid=have_sample&&nc&&mask==(unsigned)oldnc;
    for(int c=0;c<8;c++)if(mask&(1u<<c)){
        u64 dt,db;
        int ok=cpu_interval(t.core_ticks[c],t.core_busy[c],ptick[c],pbusy[c],&dt,&db);
        if(!ok)valid=0;
        corep[c]=cpu_percent(db,dt);total+=dt;busy+=db;
        ptick[c]=t.core_ticks[c];pbusy[c]=t.core_busy[c];
    }
    if(!valid){total=busy=0;memset(corep,0,sizeof corep);}
    for(int c=0;c<8;c++)if(!(mask&(1u<<c))){ptick[c]=pbusy[c]=0;corep[c]=0;}
    have_sample=1;oldnc=mask;cpu=cpu_percent(busy,total);
    if(total){
        if(hlen==42)memmove(history,history+1,41*sizeof(int));else hlen++;
        history[hlen-1]=cpu;
    }
    n=MAX(0,MIN(NPROC,(int)sc(S_PS,rows,NPROC,0,0)));
    memset(rowcpu,0,sizeof rowcpu);
    for(int i=0;i<n;i++)for(int j=0;j<NPROC;j++)if(rows[i].pid==previous[j].pid&&rows[i].pid){
        u64 dt,db;
        if(valid&&cpu_interval(rows[i].sample_ticks,rows[i].cpu_ticks,previous[j].sample_ticks,previous[j].cpu_ticks,&dt,&db))rowcpu[i]=cpu_percent(db,dt);
        break;
    }
    memset(previous,0,sizeof previous);memcpy(previous,rows,n*sizeof *rows);
    order();
}
static void resume(void){have_sample=0;memset(previous,0,sizeof previous);sample();}
static int visible_rows(void){return MAX(1,(ah-411)/27);}
static int row_offset(void){int idx=0;for(int j=0;j<n;j++)if(rows[j].pid==selected)idx=j;return MAX(0,idx-visible_rows()+1);}
static void size_line(int x,int y,const char *label,u64 value)
{
    char z[100],v[32];strset(z,label,sizeof z);display_size(v,sizeof v,value);append(z,v,sizeof z);Text(x,y,z,F_MICRO,SUB);
}
static void paint(void)
{
    begin();
    Text(15,9,"Monitoraggio in tempo reale",F_SMALL,SUB);
    Text(14,29,"Attività del sistema",F_TITLE,TX);
    button(aw-107,21,90,paused?"Riprendi":"Pausa",paused);
    int w=(aw-38)/2,y=79,x=w+37;
    Box(13,y,w,155,13,CARD);Box(24+w,y,w,155,13,CARD);
    Text(26,y+10,"CPU attiva · totale normalizzato",F_SMALL,SUB);
    char z[100]="";addnum(z,cpu,sizeof z);append(z,"%",sizeof z);Text(25,y+30,z,F_TITLE,TX);
    int gx=100,gw=w-103;Box(gx,y+22,gw,52,6,SOFT);
    if(hlen>1)for(int j=1;j<hlen;j++){
        int a=gx+3+(42-hlen+j-1)*(gw-7)/41,b=gx+3+(42-hlen+j)*(gw-7)/41;
        Line(a,y+68-history[j-1]*44/100,b,y+68-history[j]*44/100,si.dark?0x7edbb8:0x167855);
    }
    Text(26,y+85,paused?"In pausa":"Campioni per intervallo",F_MICRO,SUB);
    Text(26,y+104,"Timer; include kernel, non cicli o frequenza.",F_MICRO,SUB);
    Text(26,y+122,"Un core pieno su 4 equivale al 25% totale.",F_MICRO,SUB);
    u64 used=si.total>=si.free?si.total-si.free:0;
    Text(x,y+10,"Memoria gestita in uso",F_SMALL,SUB);
    display_size(z,sizeof z,used);Text(x,y+30,z,F_TITLE,TX);
    Box(x,y+65,w-29,6,3,SOFT);Box(x,y+65,cpu_percent(used,si.total)*(w-29)/100,6,3,ACC);
    char v[32];strset(z,"Libera ",sizeof z);display_size(v,sizeof v,si.free);append(z,v,sizeof z);append(z," / gestita ",sizeof z);display_size(v,sizeof v,si.total);append(z,v,sizeof z);Text(x,y+80,z,F_MICRO,SUB);
    strset(z,"Utente ",sizeof z);display_size(v,sizeof v,si.user_bytes);append(z,v,sizeof z);append(z," / altro pool ",sizeof z);display_size(v,sizeof v,used>=si.user_bytes?used-si.user_bytes:0);append(z,v,sizeof z);Text(x,y+97,z,F_MICRO,SUB);
    size_line(x,y+114,"Riservata/non gestita UEFI: ",si.reserved_bytes);
    Text(x,y+131,"Cache non separata; include backing di sistema.",F_MICRO,SUB);
    int slot=0;
    for(int c=0;c<8;c++)if(si.cpu_online_mask&(1u<<c)){
        int X=17+(slot/4)*(aw-25)/2,Y=245+(slot%4)*17,bw=(aw-25)/2-105;slot++;
        z[0]=0;append(z,"CPU ",sizeof z);addnum(z,c,sizeof z);Text(X,Y,z,F_MICRO,SUB);
        Box(X+46,Y+1,bw,8,4,SOFT);Box(X+46,Y+1,corep[c]*bw/100,8,4,ACC);
        z[0]=0;addnum(z,corep[c],sizeof z);append(z,"%",sizeof z);Text(X+50+bw,Y,z,F_MICRO,SUB);
    }
    int yy=328;Text(17,yy,"Processi · % CPU sul totale",F_BOLD,TX);
    button(aw-145,yy-6,127,sort?"Ordina: CPU":"Ordina: PID",0);
    int cols[]={21,76,aw-320,aw-264,aw-164,aw-80};
    const char *head[]={"PID","NOME","THR","RAM UTENTE","% CPU","STATO"};
    for(int j=0;j<6;j++)Text(cols[j],yy+29,head[j],F_MICRO,SUB);
    int off=row_offset();
    for(int j=off;j<n&&j<off+visible_rows();j++){
        int Y=yy+51+(j-off)*27;ProcInfo *r=rows+j;
        if(r->pid==selected)Box(13,Y-3,aw-25,25,7,SEL);else if(j&1)Box(13,Y-3,aw-25,25,5,SOFT);
        number(z,r->pid);Text(cols[0],Y,z,F_SMALL,TX);
        Icon(cols[1],Y-2,21,r->app?appicon(r->app):14);Text(cols[1]+28,Y,r->name,F_SMALL,TX);
        number(z,r->threads);Text(cols[2],Y,z,F_SMALL,TX);
        display_size(z,sizeof z,r->resident_bytes);Text(cols[3],Y,z,F_SMALL,TX);
        number(z,rowcpu[j]);append(z,"%",sizeof z);Text(cols[4],Y,z,F_SMALL,TX);
        Text(cols[5],Y,r->state==1?"Pronto":"Attesa",F_SMALL,SUB);
    }
    finish();
}
void app_main(void)
{
    win(820,620,"Monitor Attività");sample();paint();trace("telemetria del kernel attiva");
    for(;;){
        int d=0;u64 t=sc(S_TICKS,0,0,0,0);Event e;
        while(sc(S_EVENT,&e,0,0,0)>0){
            d=1;if(baseevent(&e))continue;if(e.type==EV_CLOSE)quit();
            if((e.type==EV_KEY&&e.a==' ')||(e.type==EV_CLICK&&e.a>=aw-110&&e.b<56)){
                paused=!paused;if(!paused)resume();
            }
            if((e.type==EV_KEY&&e.a=='s')||(e.type==EV_CLICK&&e.a>=aw-152&&e.b>=322&&e.b<355)){
                sort=!sort;order();ui_motion(FX_SELECT);
            }
            if(e.type==EV_CLICK&&e.b>=377&&e.b<ah-9){int j=row_offset()+(e.b-377)/27;if(j>=0&&j<n)selected=rows[j].pid;}
            if(e.type==EV_KEY&&(e.a==K_DOWN||e.a==K_UP)&&n){int j=0;while(j<n&&rows[j].pid!=selected)j++;j=(j+(e.a==K_DOWN?1:n-1))%n;selected=rows[j].pid;}
        }
        if(!paused&&(t<lastTick||t-lastTick>=80)){sample();d=1;}
        if(d)paint();slp(4);
    }
}
