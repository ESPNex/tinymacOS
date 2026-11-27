/* Actual Quick Look UI + production size formatter, not a copied formatter. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../userspace/ui.h"
static long hostcall(long,u64,u64,u64,u64);
#undef sc
#define sc(n,a,b,c,d) hostcall(n,(u64)(a),(u64)(b),(u64)(c),(u64)(d))
#define op ui_draw_op
#define readfile syscall_readfile
#include "../userspace/ui.c"
#undef readfile
#undef op
#include "../userspace/preview.c"
static long hostcall(long n,u64 a,u64 b,u64 c,u64 d){(void)a;(void)b;(void)c;(void)d;assert(n==S_WDRAW);return 0;}
int readfile(const char*p,char*b,int cap){(void)p;assert(cap>=5);memcpy(b,"caffè",6);return 6;}
int TextWidth(const char*s,int f){(void)f;return strlen(s)*7;}
static int contains(const char*s){for(int i=0;i<count;i++)if(list[i].type==C_TEXT&&eq(list[i].text,s))return 1;return 0;}
int main(void){
 struct{u64 bytes;const char *label;} cases[]={
 {999999,"999.9 kB"},{1000000,"1.0 MB"},{1048576,"1.0 MB"},{999999999,"999.9 MB"},
 {1000000000ull,"1.0 GB"},{1073741824ull,"1.0 GB"},{999999999999ull,"999.9 GB"},
 {1000000000000ull,"1.0 TB"},{1099511627776ull,"1.0 TB"},{~(u64)0,"18.4 EB"}};
 aw=760;ah=480;
 for(unsigned i=0;i<COUNT(cases);i++){
  Dirent f={.size=cases[i].bytes};strset(f.name,"capacità.txt",sizeof f.name);
  preview_read("/input.txt",&f,1);assert(eq(info,cases[i].label)&&!memcmp(body,"caffè",6));
  begin();preview_draw();finish();assert(count<NCMDS&&contains("Vista rapida")&&contains(cases[i].label));
  f.ro=1;preview_read("/input.txt",&f,1);char want[80];strset(want,cases[i].label,sizeof want);append(want,"  ·  Sola lettura",sizeof want);assert(eq(info,want));
 }
 assert(eq(appname(MONITOR),"Monitoraggio Attività"));assert(eq(appname(SETTINGS),"Impostazioni di Sistema"));
 assert(eq(bundlename(MONITOR),"Activity Monitor")&&eq(bundlename(SETTINGS),"System Settings"));
 puts("PASS localization6 actual Quick Look Draw frames: SI kB/MB/GB/TB/EB from real u64 bytes, exact thresholds and read-only suffix, document content unchanged; Italian app names without bundle/ABI rename");
}
