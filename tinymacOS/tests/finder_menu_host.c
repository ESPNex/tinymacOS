/* Actual shell context/edit menu construction; protects the >10-entry menu. */
#include <assert.h>
#include <stdio.h>
#include "../desktop/shell.c"
View views[NPROC];int order[NPROC],nz,dirty,cursorDirty,mx,my,buttons;SysInfo sys;
int W=1280,H=800;
int topwin(void){return 0;}
int main(void){
 views[0].s.app=FINDER;const char*labels[16];int actions[16];int n=menus(3,labels,actions);assert(n==5&&actions[0]==ACT_CUT&&actions[1]==ACT_COPY&&actions[2]==ACT_PASTE);
 views[0].s.app=TERMINAL;n=menus(3,labels,actions);assert(n==3&&actions[0]==ACT_CUT&&actions[1]==ACT_COPY&&actions[2]==ACT_PASTE);
 cmn=0;int ids[]={ACT_OPEN,ACT_PACKAGE,ACT_PREVIEW,ACT_PROPS,ACT_NEW,ACT_FILE,ACT_CUT,ACT_COPY,ACT_PASTE,ACT_RENAME,ACT_DELETE,ACT_VIEW};for(unsigned j=0;j<COUNT(ids);j++)cmadd("item",ids[j]);assert(cmn==12);n=menus(7,labels,actions);assert(n==12&&actions[6]==ACT_CUT&&actions[11]==ACT_VIEW);ov=OV_MENU;menuId=7;cmx=1270;cmy=790;Rect r=targetpanel();assert(r.x>=0&&r.y>=BAR&&r.x+r.w<=W&&r.y+r.h<=H);
 puts("PASS actual desktop menus: Finder+Terminal Cut/Copy/Paste, 12-entry context menu without old ten-slot overflow/truncation, menu fits display bounds");return 0;
}
