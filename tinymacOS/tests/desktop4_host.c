#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../desktop/desk.h"
#undef sc
static long hostcall(long,u64,u64,u64,u64);
#define sc(n,a,b,c,d) hostcall(n,(u64)(a),(u64)(b),(u64)(c),(u64)(d))
#include "../desktop/shell.c"
View views[NPROC];SysInfo sys;int W=1280,H=800,PW=1280,PH=800,night,lite,recovery_mode,lightx,lighty;
int order[NPROC],nz,dirty,cursorDirty,mx=1100,my=660,buttons,mc,gate,x1,y1,x2=1280,y2=800;
static int spawned,created,fsCount=1;static Dirent fsFiles[10];
static unsigned char pref[sizeof(DesktopLayout)];static int prefSize,prefAt,prefWrite;
static long hostcall(long n,u64 a,u64 b,u64 c,u64 d){(void)d;
 if(n==S_LIST){assert(c>=fsCount);memcpy((void*)b,fsFiles,sizeof(Dirent)*fsCount);return fsCount;}
 if(n==S_OPEN){if(!prefSize&&!b)return -1;prefWrite=!!b;prefAt=0;if(b)prefSize=0;return 1;}
 if(n==S_CLOSE)return 0;
 if(n==S_READ){int z=MIN((int)c,prefSize-prefAt);memcpy((void*)b,pref+prefAt,z);prefAt+=z;return z;}
 if(n==S_WRITE){assert(prefWrite&&prefAt+c<=sizeof pref);memcpy(pref+prefAt,(void*)b,c);prefAt+=c;prefSize=prefAt;return c;}
 if(n==S_CREATE){const char *p=(const char*)a;
  if(strstr(p,"/Library"))return 0;
  const char *name=strrchr(p,'/');assert(name);name++;
  for(int i=0;i<fsCount;i++)if(eq(name,fsFiles[i].name))return -1;
  assert(fsCount<10);fsFiles[fsCount]=(Dirent){.id=100+fsCount,.dir=!!b};strset(fsFiles[fsCount++].name,name,48);created++;sys.fsrev++;return 0;}
 assert(!"unexpected shell syscall");return -1;
}
int topwin(void){return -1;}void control_cancel(void){}void control_pointer(Rect q,int d){(void)q;(void)d;}
int control_key(Rect q,Event*e){(void)q;(void)e;return 0;}void control_draw(Rect q){(void)q;}void control_click(Rect q,int x,int y){(void)q;(void)x;(void)y;}
void openarg(int a,const char*p){(void)a;(void)p;spawned++;}void launch(int a){(void)a;spawned++;}
void action(int a){(void)a;}void mc_enter(void){mc=1;}void mc_exit(void){mc=0;}int mchit(int x,int y){(void)x;(void)y;return -1;}
void eventto(int i,Event e){(void)i;(void)e;}int focuswin(int i){return i;}
int ease(int x){return x;}int lerpi(int a,int b,int t){return a+(b-a)*t/256;}
u64 backdrop_key(Rect q){(void)q;return sys.fsrev;}
void icon(int x,int y,int sz,int id){(void)x;(void)y;(void)sz;(void)id;}
void roundr(int x,int y,int w,int h,int r,u32 col,int a){(void)x;(void)y;(void)w;(void)h;(void)r;(void)col;(void)a;}
void text(int x,int y,const char*s,int f,u32 col){(void)x;(void)y;(void)s;(void)f;(void)col;}
void center(int x,int y,const char*s,int f,u32 col){text(x,y,s,f,col);}
int textw(const char*s,int f){(void)f;return strlen(s)*8;}
void line(int x,int y,int X,int Y,u32 c){(void)x;(void)y;(void)X;(void)Y;(void)c;}
void rect(int x,int y,int w,int h,u32 c){(void)x;(void)y;(void)w;(void)h;(void)c;}
void inputline(Rect q,const char*s,int at,int on,u32 col){(void)q;(void)s;(void)at;(void)on;(void)col;}
void shadow(Rect q,int r,int a){(void)q;(void)r;(void)a;}void glass(Rect q,int r,int k){(void)q;(void)r;(void)k;}
void button_face(Rect q,int strong){(void)q;(void)strong;}
void copyin(Rect q,const u32*p){(void)q;(void)p;}void copyout(Rect q,u32*p){(void)q;(void)p;}
static void type_name(const char*s){for(;*s;s++){Event e={.type=EV_KEY,.a=*s};assert(shell_key(&e));}}
int main(void){
 sys.live_cd=1;sys.year=2026;sys.month=11;sys.day=18;
 fsFiles[0]=(Dirent){.id=42,.dir=1};strcpy(fsFiles[0].name,"Esistente");shell_base();
 assert(desktop_layout_valid(&desktopLayout));cmx=450;cmy=330;
 shell_new_folder();assert(ov==OV_NEW_FOLDER&&want&&!spawned);
 type_name("Prova");assert(eq(folderName,"Prova"));Event enter={.type=EV_KEY,.a='\n'};shell_key(&enter);assert(created==1&&!want&&!spawned);shell_base();
 int slot=2;Rect q=deskcell(slot);assert(q.x==389&&q.y==305);
 shell_new_folder();type_name("Prova");shell_key(&enter);assert(created==1&&want&&folderError[0]&&!spawned);Event esc={.type=EV_KEY,.a=27};shell_key(&esc);assert(!want);
 char path[PATHLEN];deskpath(path,1);assert(shell_desktop_drop(path,700,500,13,27));Rect moved=deskcell(slot);assert(moved.x==q.x+13&&moved.y==q.y+27&&prefSize==sizeof desktopLayout);
 layoutLoaded=0;shell_base();q=deskcell(slot);assert(q.x==moved.x&&q.y==moved.y);
 assert(shell_desktop_drop(path,100,80,-10000,-10000));q=deskcell(slot);assert(q.x==0&&q.y==34);
 assert(shell_desktop_drop(path,100,80,357,47));q=deskcell(slot);assert(q.x==357&&q.y==81); /* not a grid location */
 DesktopLayout bad=desktopLayout;bad.items[0].used=2;assert(!desktop_layout_valid(&bad));bad=desktopLayout;bad.items[0].used=1;bad.items[0].name[47]='X';assert(!desktop_layout_valid(&bad));
 shell_new_folder();type_name("..");shell_key(&enter);assert(want&&created==1);shell_key(&esc);
 shell_new_folder();type_name("a/b");shell_key(&enter);assert(want&&created==1);shell_key(&esc);
 char field[48]="caffè";int at=strlen(field),selected=0;
 assert(text_edit_key(field,sizeof field,&at,&selected,K_LEFT,0)&&at==4);
 assert(text_edit_key(field,sizeof field,&at,&selected,8,0)&&eq(field,"cafè"));
 assert(text_hit_offset("abcdef",6,80,16)==2);
 Event mission={.type=EV_KEY,.a=K_UP,.d=KEY_CONTROL};assert(shell_key(&mission)&&mc);assert(shell_key(&mission)&&!mc);
 Event search={.type=EV_KEY,.a=' ',.b=1,.d=KEY_COMMAND};assert(shell_key(&search)&&ov==OV_SEARCH&&want);shell_key(&esc);
 Event library={.type=EV_KEY,.a='a',.b=1,.c=1,.d=KEY_COMMAND|KEY_SHIFT};assert(shell_key(&library)&&ov==OV_LIBRARY&&want);shell_key(&esc);
 char stale[16]="caffè";at=-1;selected=0;text_edit_key(stale,sizeof stale,&at,&selected,'X',0);assert(eq(stale,"Xcaffè")&&at==1);
 at=999;text_edit_key(stale,sizeof stale,&at,&selected,K_LEFT,1);assert(at==0);text_edit_key(stale,sizeof stale,&at,&selected,K_RIGHT,1);assert(at==strlen(stale));
 puts("PASS actual shell Command+Space/Shift+A and Control+Up enter/exit; stale UTF-8 cursor clamping and Command+arrows");
 puts("PASS actual shell: direct folder create/collision/cancel/invalid names WITHOUT Finder spawn; free exact positions, reload persistence, clipping, malformed preferences, UTF-8 mouse/edit offsets");
}
