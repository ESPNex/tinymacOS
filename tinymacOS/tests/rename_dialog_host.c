#include <assert.h>
#include <stdio.h>
#include "../userspace/ui.h"
static int total=1100;
static int context_mask,folder_ro,move_fail,moves,creates;static char moved_from[PATHLEN],moved_to[PATHLEN];
static Clipboard shared_clip;
static long hostcall(long n,u64 a,u64 b,u64 c,u64 d){
 if(n==S_MOVE){moves++;strset(moved_from,(void*)a,PATHLEN);strset(moved_to,(void*)b,PATHLEN);return move_fail?-1:0;}
 if(n==S_CREATE){creates++;return 0;}
 if(n==S_STAT){*(Dirent*)b=(Dirent){.dir=1,.ro=folder_ro};return 0;}
 if(n==S_CLIPBOARD&&a==0){*(Clipboard*)b=shared_clip;return 0;}
 if(n==S_CONTEXT){context_mask=a;return 0;}
 if(n==S_LIST){int count=0;for(int i=d;i<total&&count<(int)c;i++){Dirent*e=(Dirent*)b+count++;*e=(Dirent){.id=i+1};sprintf(e->name,"item-%04d",i);}return count;}return -1;
}
#undef sc
#define sc(n,a,b,c,d) hostcall(n,(u64)(a),(u64)(b),(u64)(c),(u64)(d))
#include "../userspace/filebrowser.c"
#include "../userspace/finder.c"
void preview_read(const char*p,const Dirent*d,int i){(void)p;(void)d;(void)i;}
void inspect_open(const char*p,const Dirent*d){(void)p;(void)d;}
void inspect_close(void){}
void inspect_nav(int n){(void)n;}int inspect_click(int a,int b){(void)a;(void)b;return 0;}
int fileop_place(const char*a,const char*b,int m,char*p){(void)a;(void)b;(void)m;(void)p;return -1;}
static void key(int k,int ctrl){Event e={.type=EV_KEY,.a=k,.b=ctrl};event(&e);}
int main(void){aw=782;ah=392;total=1;reload();
 const char*roots[]={DOCS,"/Volumes/disk1s1","/Volumes/disk1s2","/Volumes/disk1s3","/Volumes/disk1s4"};
 for(unsigned i=0;i<COUNT(roots);i++){
  strset(cwd,roots[i],PATHLEN);sel=0;n=1;files[0]=(Dirent){0};strset(files[0].name,"caffè.txt",48);context(&(Event){.a=190,.b=120});assert(context_mask&AMASK(ACT_RENAME));
  act(ACT_RENAME);assert(dialog==3&&eq(field,"caffè.txt")&&field_selected);
  key('n',0);assert(eq(field,"n")&&!field_selected);key('o',0);key('m',0);key('e',0);key(K_HOME,0);key(0xe0,0);key(K_RIGHT,0);key(K_DEL,0);key(K_END,0);key(8,0);assert(eq(field,"ànm"));
  key('a',1);key('x',0);assert(eq(field,"x"));move_fail=1;key('\n',0);assert(dialog==3&&dialog_error[0]&&eq(field,"x"));move_fail=0;
  /* Selection changes in another window must not rename a different item. */
  strset(files[0].name,"different.txt",48);key('\n',0);assert(!dialog);char expected[PATHLEN];strset(expected,roots[i],PATHLEN);append(expected,"/caffè.txt",PATHLEN);assert(eq(moved_from,expected));
 }
 folder_ro=1;sel=0;act(ACT_RENAME);assert(!dialog);folder_ro=0;files[0].ro=1;act(ACT_RENAME);assert(!dialog);files[0].ro=0;
 act(ACT_NEW);assert(dialog==1);key('f',0);key('\n',0);assert(!dialog&&creates==1);
 sel=0;act(ACT_RENAME);int before=moves;key(27,0);assert(!dialog&&moves==before);
 puts("PASS actual Finder Rinomina: context mask on native/FAT32/NTFS/exFAT paths, shared new-folder dialog, UTF8 caret/delete/navigation/select-all, click-independent source identity, errors keep dialog, cancel and readonly protections");
}
