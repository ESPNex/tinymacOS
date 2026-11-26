/* Execute TextEdit's real action, toolbar, keyboard and confirmation logic. */
#include <assert.h>
#include <stdio.h>
#include "../userspace/ui.h"
static long mockcall(long n,u64 a,u64 b,u64 c,u64 d){(void)c;(void)d;if(n==S_STAT){if(eq((char*)a,"/large")){*(Dirent*)b=(Dirent){.size=65536};return 0;}return -1;}return 0;}
#undef sc
#define sc(n,a,b,c,d) mockcall(n,(u64)(a),(u64)(b),(u64)(c),(u64)(d))
#include "../userspace/textedit.c"
int aw=760,ah=480;SysInfo si;
static int opens,closed,readmode;
int readfile(const char*p,char*b,int cap){(void)p;assert(cap==TEXTEDIT_LIMIT);memset(b,'Q',65536);return readmode==1?-1:readmode==2?65535:65536;}
void filepanel_open(FilePanel*p,int save,const char*initial){opens++;p->active=1;p->save=save;strset(p->cwd,initial,sizeof p->cwd);}
void ui_motion(int m){(void)m;}
void quit(void){closed++;}
int writefile(const char*p,const char*b,int n){(void)p;(void)b;return n;}
void addnum(char*p,u64 n,int cap){char s[32];number(s,n);append(p,s,cap);}
const char*leaf(const char*p){const char*r=p;while(*p){if(*p=='/')r=p+1;p++;}return r;}
int main(void){
 blank();act(ACT_OPEN);assert(opens==1&&picker.active&&!picker.save&&!dialog);
 picker.active=0;Event key_o={.type=EV_KEY,.a='o',.b=1};key(&key_o);assert(opens==2&&picker.active&&!dialog);
 picker.active=0;paste("UNSAVED",7);assert(dirty&&length==7);act(ACT_OPEN);assert(dialog==6&&!picker.active&&opens==2);dk(27);assert(!dialog&&dirty&&length==7&&!picker.active);
 act(ACT_OPEN);confirm_click(&(Event){.type=EV_CLICK,.a=aw/2,.b=(ah-190)/2+152});assert(!dialog&&dirty&&opens==2);
 act(ACT_OPEN);confirm_click(&(Event){.type=EV_CLICK,.a=(aw+MIN(460,aw-40))/2-70,.b=(ah-190)/2+152});assert(!dialog&&picker.active&&!picker.save&&opens==3&&dirty&&length==7);
 /* Canceling the picker must retain the dirty document; choosing a new file is
  * the only operation allowed to replace it. Confirmation alone never clears it. */
 picker.active=0;act(ACT_OPEN);dk('\n');assert(picker.active&&opens==4&&dirty&&length==7);
 picker.active=0;prompt(1);assert(picker.active&&picker.save&&!dialog&&opens==5);
 picker.active=0;dialog=3;dk(27);assert(!closed&&dirty);dialog=3;dk('\n');assert(closed==1);
 readmode=1;assert(load("/large")<0&&length==7&&eq(doc,"UNSAVED"));readmode=2;assert(load("/large")<0&&length==7&&eq(doc,"UNSAVED"));readmode=0;assert(!load("/large")&&length==65536&&doc[0]=='Q'&&!dirty);
 puts("PASS TextEdit: modern Open/Save only, Ctrl+O and action share chooser, dirty-document Cancel/Continue by mouse and keyboard preserve bytes until a file is selected; close confirmation, 64KiB load and failed/partial load preservation remain functional");
}
