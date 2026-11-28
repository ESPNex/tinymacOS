#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../desktop/desk.h"
static long hostcall(long,u64,u64,u64,u64);
#undef sc
#define sc(n,a,b,c,d) hostcall(n,(u64)(a),(u64)(b),(u64)(c),(u64)(d))
View views[NPROC];int order[NPROC],nz,dirty,cursorDirty,mx,my,buttons,mc,gate,W=1280,H=800;
SysInfo sys;static int drag=-1,mode,overlay,login_kind=CUR_TEXT,shell_kind=CUR_TEXT;
static struct {int started;}filedrag;
static int standalone_dialog(View*v){return v->s.app==ABOUT;}
static Rect dialog_bounds(View*v){return v->box;}
int login_cursor(int x,int y){(void)x;(void)y;return login_kind;}
int shell_cursor(int x,int y){(void)x;(void)y;return shell_kind;}
int shell_overlay_open(void){return overlay;}
int recovery_overlay_open(void){return overlay;}
#include "../desktop/pointer.h"
static int reads;
static long hostcall(long n,u64 a,u64 b,u64 c,u64 d){
 assert(n==S_ART&&a>NWALL&&a<=NWALL+CUR_COUNT);
 char local[256];snprintf(local,sizeof local,"assets/cursors/%s",cursorname(a-NWALL-1));FILE*asset=fopen(local,"rb");assert(asset);
 fseek(asset,0,SEEK_END);long size=ftell(asset);if(!d){fclose(asset);return size;}
 assert(d<=MAXFILE&&b+d<=(u64)size);reads++;fseek(asset,b,SEEK_SET);long z=fread((void*)c,1,d,asset);fclose(asset);return z;
}
static void point(int x,int y,int expected){mx=x;my=y;pointer_step();assert(pointer_kind==expected);}
int main(void)
{
 pointer_load();for(int i=0;i<CUR_COUNT;i++)assert(pointer_ok[i]);int initial_reads=reads;
 gate=1;point(10,10,CUR_TEXT);gate=0;nz=1;order[0]=0;
 View*v=views;v->s.pid=23;v->s.app=TEXTEDIT;v->box=(Rect){100,100,600,400};v->n=3;
 v->cmd[0]=(Draw){.type=C_CURSOR,.x=20,.y=30,.w=400,.h=200,.style=CUR_TEXT};
 v->cmd[1]=(Draw){.type=C_BUTTON,.x=20,.y=5,.w=100,.h=24};
 v->cmd[2]=(Draw){.type=C_CURSOR,.x=30,.y=50,.w=30,.h=30,.style=CUR_UNAVAILABLE};
 point(130,180,CUR_TEXT);assert(pointer_owner==23);point(140,205,CUR_UNAVAILABLE);point(130,100+TITLE+12,CUR_SELECT);
 point(100,100,CUR_DIAGONAL1);point(699,100,CUR_DIAGONAL2);point(100,499,CUR_DIAGONAL2);point(699,499,CUR_DIAGONAL1);point(100,300,CUR_HORIZONTAL);point(699,300,CUR_HORIZONTAL);point(300,100,CUR_VERTICAL);point(300,499,CUR_VERTICAL);
 drag=0;mode=1;point(10,10,CUR_MOVE);mode=(EDGE_LEFT|EDGE_BOTTOM)<<1;point(10,10,CUR_DIAGONAL2);drag=-1;
 overlay=1;point(130,180,CUR_TEXT);sys.recovery=1;point(130,180,CUR_NORMAL);overlay=0;
 v->max=1;point(100,300,CUR_NORMAL);v->max=0;
 nz=2;order[1]=1;views[1]=*v;views[1].s.pid=24;views[1].n=0;point(130,180,CUR_NORMAL);views[1].mini=1;point(130,180,CUR_TEXT);
 v->cmd[0].style=CUR_BUSY;point(130,180,CUR_BUSY);int first=pointer_index;sys.ticks+=2;pointer_step();assert(pointer_index!=first);sys.reduce_motion=1;pointer_step();assert(pointer_index==0);
 v->dead=1;point(130,180,CUR_NORMAL);assert(pointer_owner==-1);
 assert(reads==initial_reads&&pointer_context_events[POINTER_CONTEXT_ENTER]>10&&pointer_context_events[POINTER_CONTEXT_LEAVE]>10);
 puts("PASS M7 production filesystem loader and pointer context events: 17 resources, topmost/modal/Recovery/login/client regions, 8 borders, captured move/resize, busy tick/reduced motion, no filesystem reads in pointer loop");
}
