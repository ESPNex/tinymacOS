/* Production input handlers, not a second implementation of their geometry. */
#include <assert.h>
#include <stdio.h>
#if FIELD_CASE==1
#include "../desktop/desk.h"
#else
#include "../userspace/ui.h"
#endif
#undef sc
static long hostcall(long,u64,u64,u64,u64);
#define sc(n,a,b,c,d) hostcall(n,(u64)(a),(u64)(b),(u64)(c),(u64)(d))
#if FIELD_CASE!=7
static long hostcall(long n,u64 a,u64 b,u64 c,u64 d){(void)n;(void)a;(void)b;(void)c;(void)d;return -1;}
#endif
#if FIELD_CASE==1
#include "../desktop/login.c"
SysInfo sys;int W=1280,H=800,PW=1280,PH=800,dirty,cursorDirty,mx,my,buttons;
/* Deliberately proportional font: proves hit-testing is not byte*8. */
int textw(const char*s,int f){(void)f;int n=0;while(*s){n+=*s=='i'?3:*s=='W'?12:8;s++;}return n;}
static void click(int x,int y){buttons=0;Event e={.type=EV_POINTER_ABS,.a=x,.b=y,.c=1};login_input(&e);}
static void key(int k,int modifiers){Event e={.type=EV_KEY,.a=k,.d=modifiers};login_input(&e);}
int main(void){
 account.state=SESSION_SETUP;strset(form.name,"WiWi",32);carets[0]=4;
 Rect r=inputbox(0);click(r.x+16+15,r.y+12);assert(field==0&&carets[0]==2);key('X',0);assert(eq(form.name,"WiXWi"));
 strset(form.password,"abcdef",65);carets[1]=6;r=inputbox(1);click(r.x+16+18,r.y+12);assert(field==1&&carets[1]==2);key('X',0);assert(eq(form.password,"abXcdef"));
 key('a',KEY_COMMAND);key('Z',0);assert(eq(form.password,"Z"));
 strset(confirm,"caffè",65);carets[2]=6;r=inputbox(2);click(r.x+16+4*9,r.y+12);assert(carets[2]==4);key(K_DEL,0);assert(eq(confirm,"caff"));
 account.needs_seed=1;strset(setup_code,"abcdef",33);seedAt=6;r=seed_box(0);click(r.x+16+16,r.y+12);assert(seedAt==2);key('1',0);assert(eq(setup_code,"ab1cdef"));
 puts("PASS actual login: proportional-name and masked-password click insertion, UTF-8 delete, Command select-all, setup-code click insertion");
}
#elif FIELD_CASE==2
#include "../userspace/installer.c"
static void key(int k,int command){Event e={.type=EV_KEY,.a=k,.b=command};assert(installer_field_event(&e));}
static void click(int x,int y){Event e={.type=EV_CLICK,.a=x,.b=y};assert(installer_field_event(&e));}
int main(void){
 aw=850;ah=540;page=7;strset(password[0],"abcdef",65);inputAt[0]=6;
 click(24+12+16,185);assert(focus==0&&inputAt[0]==2);key('X',0);assert(eq(password[0],"abXcdef"));
 key('\t',0);key('e',0);assert(focus==1&&eq(password[1],"e"));key('a',1);key(0xe8,0);assert(eq(password[1],"è")&&inputAt[1]==2);key(K_LEFT,0);key(K_DEL,0);assert(!password[1][0]);
 page=8;strset(key_check,"abcd",9);inputAt[2]=4;click(24+12+8,218);key('1',0);assert(eq(key_check,"a1bcd"));
 Event esc={.type=EV_KEY,.a=27};assert(!installer_field_event(&esc));
 puts("PASS actual retail/Recovery installer: both password fields and recovery-key field click insertion, select-all, UTF-8 and navigation");
}
#elif FIELD_CASE==3
#include "../userspace/vaultpanel.c"
static void event(int t,int a,int b){Event e={.type=t,.a=a,.b=b};vault_view_event(&e);}
int main(void){
 vx=208;vy=0;vw=640;vh=540;page=1;fields=3;strset(input[0],"abcdef",65);inputAt[0]=6;
 event(EV_CLICK,vx+39+16,187);assert(focus==0&&inputAt[0]==2);event(EV_KEY,'X',0);assert(eq(input[0],"abXcdef"));
 event(EV_CLICK,vx+39,244);event(EV_KEY,0xe8,0);assert(focus==1&&eq(input[1],"è"));event(EV_KEY,K_LEFT,0);event(EV_KEY,K_DEL,0);assert(!input[1][0]);
 event(EV_CLICK,vx+39,301);event(EV_KEY,'Z',0);assert(focus==2&&eq(input[2],"Z"));
 page=2;strset(input[0],"abcd",65);inputAt[0]=4;event(EV_CLICK,vx+40+8,285);event(EV_KEY,'1',0);assert(eq(input[0],"a1bcd"));
 puts("PASS actual FileVault: all three masked fields and recovery-key confirmation click insertion, UTF-8 navigation/delete");
}
#elif FIELD_CASE==4
#include "../userspace/filepanel.c"
const char *const browser_paths[BROWSER_PLACES]={"/","/","/","/","/","/","/","/","/","/","/"};
int browser_page(const char*p,Dirent*d,int n,int base,int*more){(void)p;(void)d;(void)n;(void)base;*more=0;return 0;}
static void event(FilePanel*p,int t,int a,int b){Event e={.type=t,.a=a,.b=b};assert(!filepanel_event(p,&e));}
int main(void){
 aw=850;ah=540;FilePanel p={.active=1,.focus=1,.cursor=6,.save=1};geometry(&p);strset(p.name,"abcdef",PATHLEN);strset(p.location,"/Users/Test/Documents",PATHLEN);
 event(&p,EV_CLICK,p.x+141+16,p.y+p.h-90);assert(p.cursor==2&&!p.replace);event(&p,EV_KEY,'X',0);assert(eq(p.name,"abXcdef"));
 p.cursor=strlen(p.location);p.focus=2;event(&p,EV_CLICK,p.x+69+8,p.y+52);assert(p.cursor==1);event(&p,EV_KEY,'X',0);assert(eq(p.location,"/XUsers/Test/Documents"));
 p.newfolder=1;p.focus=3;strset(p.folder,"abcdef",48);p.cursor=6;int mw=MIN(410,p.w-28),mx=p.x+(p.w-mw)/2,my=p.y+(p.h-150)/2;
 event(&p,EV_CLICK,mx+24+16,my+52);assert(p.cursor==2&&!p.replace);event(&p,EV_KEY,0xe8,0);assert(eq(p.folder,"abècdef")&&p.cursor==4);
 puts("PASS actual Open/Save panel: location/name/child new-folder click insertion, no accidental replacement, UTF-8 insertion");
}
#elif FIELD_CASE==5
#include "../userspace/diskutility.c"
static void key(int k,int command){Event e={.type=EV_KEY,.a=k,.b=command};if(dialog==6)export_event(&e);else edit_event(&e);}
static void click(int x,int y){Event e={.type=EV_CLICK,.a=x,.b=y};if(dialog==6)export_event(&e);else edit_event(&e);}
int main(void){
 aw=850;ah=540;dialog=3;edit.op=DISK_EDIT_CREATE;strset(editsize,"1234",16);editAt[0]=4;strset(editname,"abcdef",48);editAt[1]=6;
 int tw=MIN(440,aw-48),xx=(aw-tw)/2,yy=(ah-282)/2;
 click(xx+168+8,yy+101);assert(editfield==0&&editAt[0]==1);key('9',0);assert(eq(editsize,"19234"));key('X',0);assert(eq(editsize,"19234"));
 click(xx+168+16,yy+143);assert(editfield==1&&editAt[1]==2);key(0xe8,0);assert(eq(editname,"abècdef"));key('a',1);key('Z',0);assert(eq(editname,"Z"));
 dialog=6;strset(export_request.destination,"/Volumes/abcd",PATHLEN);export_cursor=strlen(export_request.destination);tw=MIN(510,aw-40);xx=(aw-tw)/2;yy=(ah-266)/2;
 click(xx+29+9*8,yy+107);assert(export_cursor==9);key('X',0);assert(eq(export_request.destination,"/Volumes/Xabcd"));
 puts("PASS actual Disk Utility: size/name/export-path click insertion, numeric filtering, UTF-8, Command select-all; no partition writes");
}
#elif FIELD_CASE==6
#include "../userspace/settings.c"
void vault_view_draw(int x,int y,int w,int h){(void)x;(void)y;(void)w;(void)h;}
void vault_view_init(void){}void vault_view_leave(void){}
int vault_view_can_leave(void){return 1;}int vault_view_can_close(void){return 1;}int vault_view_tick(void){return 0;}
void vault_view_event(const Event*e){(void)e;assert(!"unexpected FileVault dispatch");}
static void inputkey(int k,int command){Event e={.type=EV_KEY,.a=k,.b=command};key(&e);}
int main(void){
 aw=860;ah=540;pane=0;hsearch=(R){16,12,180,30};strset(query,"abcdef",24);queryAt=6;
 click(20+16,19);assert(focus_search&&queryAt==2);inputkey('X',0);assert(eq(query,"abXcdef"));
 inputkey('a',1);inputkey(0xe8,0);assert(eq(query,"è")&&queryAt==2);inputkey(K_LEFT,0);inputkey(K_DEL,0);assert(!query[0]);
 modal_mode=1;strset(modal_buf,"abcdef",65);modalAt=6;int w=MIN(aw-46,430),x=(aw-w)/2,y=(ah-143)/2;
 click(x+25+16,y+52);assert(modalAt==2);inputkey('X',0);assert(eq(modal_buf,"abXcdef"));
 puts("PASS actual Settings: search/passphrase click insertion, UTF-8 navigation/delete, Command select-all; no backend mutation");
}
#elif FIELD_CASE==7
#include <setjmp.h>
static jmp_buf done;
static void calendar_win(int w,int h,const char*s){(void)s;aw=w-2;ah=h;}
static void calendar_finish(void){}
static int calendar_readfile(const char*p,void*b,int n){(void)p;(void)b;(void)n;return -1;}
#define win calendar_win
#define finish calendar_finish
#define readfile calendar_readfile
#include "../userspace/calendar.c"
static int eventAt;
static Event events[]={
 {.type=EV_KEY,.a='\n'}, {.type=EV_KEY,.a='a'}, {.type=EV_KEY,.a='b'},
 {.type=EV_KEY,.a='c'}, {.type=EV_KEY,.a='d'},
 {.type=EV_CLICK,.a=0,.b=0}, {.type=EV_KEY,.a=0xe8}
};
static long hostcall(long n,u64 a,u64 b,u64 c,u64 d){(void)b;(void)c;(void)d;
 if(n==S_INFO){*(SysInfo*)a=(SysInfo){.year=2026,.month=11,.day=18,.fsrev=1};return 0;}
 if(n==S_STAT)return -1;
 if(n==S_EVENT){if(eventAt==sizeof events/sizeof events[0])return 0;
  Event e=events[eventAt++];if(e.type==EV_CLICK){int w=MIN(aw-46,430),x=(aw-w)/2,y=(ah-143)/2;e.a=x+25+16;e.b=y+52;}*(Event*)a=e;return 1;}
 if(n==S_SLEEP)longjmp(done,1);
 assert(!"unexpected Calendar backend mutation");return -1;
}
int main(void){
 if(!setjmp(done))app_main();
 assert(edit&&eq(memo,"abècd")&&memoAt==4);
 puts("PASS actual Calendar app loop: enter memo editor, type, click insertion, UTF-8; no backend mutation");
}
#endif
