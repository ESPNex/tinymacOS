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
static int mounted=1,busy,queries,changes;
static long hostcall(long n,u64 a,u64 b,u64 c,u64 e){(void)c;(void)e;
 if(n==S_DISK&&a==15){VolumeMount*q=(void*)b;queries++;
  q->managed=eq(q->target,"/Volumes/disk0s1");q->mounted=q->managed?mounted:1;
  if(!q->managed)return q->operation?VOLUME_PROTECTED:0;
  if(q->operation){changes++;if(busy){strset(q->message,"Volume in uso",sizeof q->message);return VOLUME_BUSY;}
   mounted=q->mounted=q->operation==VOLUME_MOUNT;strset(q->message,mounted?"Volume montato.":"Volume smontato.",sizeof q->message);}
  return 0;
 }
 assert(n==S_WDRAW);return 0;
}
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
    /* The real production toolbar, not a copied UI model. */
    d.current=-1;d.disk[0].layout=0;d.disk[0].map.part[0].fs=FS_FAT32;d.disk[0].map.part[0].number=1;
    view=200;
    const int widths[]={560,600,808,1100};
    for(unsigned wi=0;wi<sizeof widths/sizeof *widths;wi++){
        aw=widths[wi];paint();int seen=0;
        for(int i=0;i<count;i++)if(list[i].type==C_BUTTON&&list[i].y==13){
            Draw*q=list+i;assert(q->x>=rail()+22&&q->x+q->w<=aw);seen++;
            for(int j=i+1;j<count;j++)if(list[j].type==C_BUTTON&&list[j].y==13)assert(q->x+q->w<=list[j].x||list[j].x+list[j].w<=q->x);
        }
        assert(seen==4&&contains("Smonta")&&!contains("Monta"));
    }
    mount_toggle();assert(!mounted&&changes==1);paint();assert(contains("Monta")&&!contains("Smonta"));
    mount_toggle();assert(mounted&&changes==2);paint();assert(contains("Smonta")&&!contains("Monta"));
    busy=1;mount_toggle();assert(mounted&&changes==3&&eq(msg,"Volume in uso"));busy=0;
    view=-1;int old=changes;mount_toggle();assert(changes==old);
    view=400;mount_toggle();assert(changes==old);
    d.current=0;d.disk[0].layout=2;view=1;mount_toggle();assert(changes==old);
    paint();int controls=0;for(int i=0;i<count;i++)if(list[i].type==C_BUTTON&&list[i].y==13)controls++;
    assert(controls==3&&contains("Smonta"));
    assert(queries>10);
    puts("PASS real mount toolbar: state-dependent adjacent SOS control, four widths no overlaps, actual toggle/readback, busy refusal, whole disk/gap/startup disabled and no stale target");
    puts("PASS actual Utility Disco paint: kernel extent sizes (not mb-45 estimates), actual used/total SI values, no duplicate units, disk partition map, distinct system-source label; no invented snapshot bytes/date");
}
