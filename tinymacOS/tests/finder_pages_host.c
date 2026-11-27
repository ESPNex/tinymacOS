#include <assert.h>
#include <stdio.h>
#include "../userspace/ui.h"
static int total=1100;
static int context_mask,folder_ro;
static Clipboard shared_clip;
static long hostcall(long n,u64 a,u64 b,u64 c,u64 d){
 if(n==S_STAT){*(Dirent*)b=(Dirent){.dir=1,.ro=folder_ro};return 0;}
 if(n==S_CLIPBOARD&&a==0){*(Clipboard*)b=shared_clip;return 0;}
 if(n==S_CONTEXT){context_mask=a;return 0;}
 if(n==S_LIST){int count=0;for(int i=d;i<total&&count<(int)c;i++){Dirent*e=(Dirent*)b+count++;*e=(Dirent){.id=i+1};sprintf(e->name,"item-%04d",i);}return count;}return -1;
}
#undef sc
#define sc(n,a,b,c,d) hostcall(n,(u64)(a),(u64)(b),(u64)(c),(u64)(d))
#include "../userspace/filebrowser.c"
#include "../userspace/finder.c"
int main(void){
 aw=782;ah=392;
 assert(BROWSER_PLACES==11&&eq(paths[9],"/.snapshots")&&eq(paths[10],"/Volumes"));
 assert(sidebar_at(ny(9)+8)==9);
 sidebar_wheel(-16);assert(sidebar_offset>0);sidebar_wheel(16);assert(!sidebar_offset);
 ah=300;assert(sidebar_at(ny(9)+8)==-1);ah=392;
 strset(cwd,DOCS,PATHLEN);reload();assert(n==64&&more&&!pagebase);int seen=0;
 do{for(int j=0;j<n;j++){char expected[48];sprintf(expected,"item-%04d",seen++);assert(eq(expected,files[j].name));}sel=n-1;offset=53;item_click.armed=1;}while(nextpage(1));
 assert(seen==1100&&pagebase==1088&&n==12&&!more);assert(nextpage(-1)&&pagebase==1024&&sel==-1&&!offset&&!item_click.armed);
 total=70;reload();assert(pagebase==64&&n==6&&!more);assert(nextpage(-1)&&!pagebase&&n==64&&more);assert(!nextpage(-1));
 total=1;pagebase=offset=0;reload();files[0].dir=1;Event menu={.a=190,.b=120};context(&menu);assert(context_mask&AMASK(ACT_COPY));assert(context_mask&AMASK(ACT_CUT));assert(!(context_mask&AMASK(ACT_PASTE)));
 shared_clip.kind=CLIP_FILE;context(&menu);assert(context_mask&AMASK(ACT_PASTE));files[0].ro=1;context(&menu);assert(!(context_mask&AMASK(ACT_CUT))&&!(context_mask&AMASK(ACT_PASTE))&&(context_mask&AMASK(ACT_COPY)));shared_clip.kind=CLIP_TEXT;context(&menu);assert(!(context_mask&AMASK(ACT_PASTE)));
 puts("PASS Finder context menu: folders can Copy/Cut, shared FILE clipboard enables Paste, TEXT clipboard cannot paste as a file, read-only folders refuse Cut/Paste and remain copyable");
 puts("PASS real Finder: bounded 64-entry pages enumerate 1100 files, last-page lookahead, back navigation and selection/double-click reset, safe retreat after deletion");
}
