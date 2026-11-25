/* Shipping userspace wizard; only syscall transport and motion/quit are stubs.
 * Real media equality and actual reinstall writes are tested separately. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../userspace/ui.h"
#undef sc
static long hostcall(long,u64,u64,u64,u64);
#define sc(n,a,b,c,d) hostcall(n,(u64)(a),(u64)(b),(u64)(c),(u64)(d))
#include "../userspace/installer.c"
SysInfo si;
static InstallInfo source;
static int healthy,install_begins,cancels,closed;
void ui_motion(int kind){(void)kind;}
void quit(void){closed++;}
static long hostcall(long n,u64 a,u64 b,u64 c,u64 d){
 (void)c;(void)d;
 if(n==S_INSTALL){
  if(a==0){memcpy((void*)b,&source,sizeof source);return 0;}
  if(a==1){install_begins++;return 0;}
  if(a==3){cancels++;return 0;}
 }
 if(n==S_DISK&&a==0){DiskInfo*q=(void*)b;memset(q,0,sizeof *q);q->count=1;q->disk[0].layout=2;q->disk[0].healthy=healthy;q->disk[0].sectors=TMC_DISK_MAX_SECTORS;q->disk[0].map.scheme=MAP_GPT;strset(q->disk[0].model,"Native fixture",44);return 0;}
 assert(!"unexpected syscall: no restore/reset/writes during selection");return -1;
}
int main(void){
 assert(!strcmp(appname(INSTALLER),"Installa tinymacOS Tramontana"));
 assert(!strcmp(installer_title(0),"Installa tinymacOS Tramontana"));
 assert(!strcmp(installer_title(1),"Reinstalla tinymacOS Tramontana"));
 reinstall=1;healthy=1;page=0;source.live=0;refresh();assert(page==0&&info.count==1&&info.disk[0].usable&&info.disk[0].sectors==TMC_DISK_MAX_SECTORS);
 next();assert(page==1);choice=0;next();assert(page==2&&!confirm);next();assert(page==2&&!reinstall_pending&&!install_begins);
 back();assert(page==1);healthy=0;refresh();assert(!can());next();assert(page==1);
 healthy=1;refresh();next();confirm=1;next();assert(page==3&&reinstall_pending&&!install_begins&&!cancels);
 page=4;next();assert(closed==1);page=5;next();assert(page==1&&choice==-1&&!cancels);
 puts("PASS shared Recovery installer wizard: no live payload needed, healthy native target only, confirmation required, deferred reinstall, no data-reset syscall, success closes back to Recovery");
 reinstall=0;memset(&source,0,sizeof source);source.live=1;source.count=1;source.disk[0].usable=1;page=0;refresh();next();choice=0;next();assert(page==2);next();assert(!install_begins);confirm=1;next();assert(install_begins==1);source.live=0;refresh();assert(page==6);
 puts("PASS desktop fresh-install flow preserved; context-specific install/reinstall titles; installed non-Recovery session still refuses live installation");
 return 0;
}
