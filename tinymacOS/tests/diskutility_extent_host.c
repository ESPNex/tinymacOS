#include <assert.h>
#include <stdio.h>
#include "../userspace/ui.h"
static long hostcall(long,u64,u64,u64,u64);
#undef sc
#define sc(n,a,b,c,d) hostcall(n,(u64)(a),(u64)(b),(u64)(c),(u64)(d))
#define op ui_draw_op
#include "../userspace/ui.c"
#undef op
#include "../userspace/diskutility.c"
static long hostcall(long n,u64 a,u64 b,u64 c,u64 e){(void)a;(void)b;(void)c;(void)e;assert(n==S_WDRAW);return 0;}
int TextWidth(const char *s,int face){(void)face;return strlen(s)*8;}
static int contains(const char *s){for(int i=0;i<count;i++)if(list[i].type==C_TEXT&&eq(list[i].text,s))return 1;return 0;}
int main(void)
{
    aw=808;ah=460;d.count=1;d.current=0;sel=0;d.disk[0].layout=2;
    d.disk[0].sectors=2147483648u;d.disk[0].region_sectors[0]=49152;
    d.disk[0].region_sectors[1]=1000000;d.disk[0].region_sectors[2]=20480;d.disk[0].region_sectors[3]=49153;
    fi.used=1000000;fi.total=512000000;
    view=1;paint();assert(contains("1.0 MB su 512.0 MB")&&!contains("KB usati MiB")&&!contains("Copia dati")&&contains("Nuovo snapshot"));
    view=0;paint();assert(contains("25.1 MB"));
    view=2;paint();assert(contains("10.4 MB"));
    view=3;paint();assert(contains("Origine sistema")&&contains("25.1 MB"));
    view=-1;d.disk[0].map.count=1;d.disk[0].map.part[0]=(DiskPartition){.start=2048,.sectors=131072};
    paint();assert(contains("1.0 TB")&&contains("Partizioni del disco · spazio non allocato in grigio"));
    view=100;nsn=1;sn[0].date=0;paint();assert(contains("Condivisa (CoW)")&&contains("Non disponibile")&&contains("Mostra nel Finder"));
    assert(count<NCMDS);
    puts("PASS actual Utility Disco paint: kernel extent sizes (not mb-45 estimates), actual used/total SI values, no duplicate units, disk partition map, distinct system-source label; no invented snapshot bytes/date");
}
