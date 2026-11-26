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
static int layout=2,healthy,install_begins,cancels,closed,prepared,acknowledged;
void ui_motion(int kind){(void)kind;}
void quit(void){closed++;}
static long hostcall(long n,u64 a,u64 b,u64 c,u64 d){
 (void)d;
 if(n==S_FILEVAULT&&a==0){memset((void*)c,0,sizeof(VaultInfo));return 0;}
 if(n==S_INSTALL){
  if(a==0){memcpy((void*)b,&source,sizeof source);return 0;}
  if(a==1){assert((!!c)==(!!prepared));if(c)assert(acknowledged);install_begins++;return 0;}
  if(a==3){cancels++;return 0;}
  if(a==5){VaultRequest*q=(void*)b;if(q->disk==-1){prepared=acknowledged=0;return 0;}if(!eq(q->password,q->replacement)||strlen(q->password)<8)return -1;prepared++;memset(q->recovery,'a',64);q->recovery[64]=0;return 0;}
  if(a==6){VaultRequest*q=(void*)b;if(!prepared||!eq(q->password,"aaaaaaaa"))return -1;acknowledged++;return 0;}
 }
 if(n==S_DISK&&a==0){DiskInfo*q=(void*)b;memset(q,0,sizeof *q);q->count=1;q->disk[0].layout=layout;q->disk[0].healthy=healthy;q->disk[0].sectors=TMC_DISK_MAX_SECTORS;q->disk[0].map.scheme=MAP_GPT;strset(q->disk[0].model,"Native fixture",44);return 0;}
 assert(!"unexpected syscall: no restore/reset/writes during selection");return -1;
}
int main(void){
 assert(!strcmp(appname(INSTALLER),"Installa tinymacOS Tramontana"));
 assert(!strcmp(installer_title(0),"Installa tinymacOS Tramontana"));
 assert(!strcmp(installer_title(1),"Reinstalla tinymacOS Tramontana"));
 reinstall=1;healthy=1;page=0;source.live=0;refresh();assert(page==0&&info.count==1&&info.disk[0].usable&&info.disk[0].sectors==TMC_DISK_MAX_SECTORS);
 next();assert(page==1);choice=0;next();assert(page==2&&!confirm);next();assert(page==2&&!reinstall_pending&&!install_begins);
 back();assert(page==1);layout=0;refresh();assert(!can()&&strstr(info.disk[0].reason,"desktop live"));next();assert(page==1&&!install_begins&&!reinstall_pending);
 layout=2;healthy=0;refresh();assert(!can()&&strstr(info.disk[0].reason,"non reinstallabile"));next();assert(page==1);
 puts("PASS Recovery without native install: explicit live-desktop guidance, no invented corrupt-table verdict and zero install/reset calls");
 healthy=1;refresh();next();confirm=1;next();assert(page==3&&reinstall_pending&&!install_begins&&!cancels);
 page=4;next();assert(closed==1);page=5;next();assert(page==1&&choice==-1&&!cancels);
 puts("PASS shared Recovery installer wizard: no live payload needed, healthy native target only, confirmation required, deferred reinstall, no data-reset syscall, success closes back to Recovery");
 reinstall=0;memset(&source,0,sizeof source);source.live=1;source.count=1;source.disk[0].usable=1;page=0;refresh();next();choice=0;next();assert(page==2);next();assert(!install_begins);confirm=1;next();assert(install_begins==1);source.live=0;refresh();assert(page==6);
 puts("PASS desktop fresh-install flow preserved; context-specific install/reinstall titles; installed non-Recovery session still refuses live installation");
 source.live=1;source.state=0;source.encryption_available=1;refresh();choice=0;page=7;
 strset(password[0],"volume-password",65);strset(password[1],"different-pass",65);next();assert(page==7&&enc_error[0]&&!prepared);
 strset(password[0],"volume-password",65);strset(password[1],"volume-password",65);next();assert(page==8&&prepared&&!password[0][0]&&!password[1][0]&&install_begins==1);
 next();assert(page==8&&!acknowledged);strset(key_check,"aaaaaaaa",9);next();assert(page==2&&encrypted&&acknowledged&&!encryption.recovery[0]&&!confirm);
 back();assert(page==1&&!encrypted&&!prepared&&!acknowledged);page=7;strset(password[0],"secret-secret",65);back();assert(page==2&&!password[0][0]);
 encrypted=1;prepared=acknowledged=1;page=5;next();assert(page==1&&!encrypted&&!prepared&&!acknowledged);
 encrypted=1;prepared=acknowledged=1;page=3;info.state=INSTALL_SCAN;back();assert(page==1&&!encrypted&&!prepared&&!acknowledged);
 puts("PASS encrypted install enrollment: mismatch refused, password masked/staged then wiped, recovery suffix required before confirmation, backing out discards pending secrets without target writes");
 return 0;
}
