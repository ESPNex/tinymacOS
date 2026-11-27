#include <assert.h>
#include <stdio.h>
#include "../include/container.h"
#include "../include/disk_size_input.h"
#include "../include/display_size.h"
int main(void)
{
    NativeContainer h={.version=1,.sectors=TC_SECTORS,.data=TC_DATA,.rec=TC_REC,.source=TC_SOURCE,.duet=TC_DUET};
    memcpy(h.magic,TC_MAGIC,8);assert(container_bounds(&h,TC_SECTORS));
    h.sectors+=2048;assert(!container_bounds(&h,h.sectors));h.sectors=TC_SECTORS;
    u32 sizes[]={TC_SECTORS,TC_SECTORS+2048,524288,20971520,TMC_DISK_MAX_SECTORS};
    for(unsigned i=0;i<COUNT(sizes);i++){
        h.version=2;h.sectors=sizes[i];h.rec=container_rec(h.sectors);h.source=h.rec+20480;h.duet=h.source+49153;
        assert(container_bounds(&h,h.sectors));assert(h.rec-h.data==h.sectors-(TC_SECTORS-TC_DATA_N));
        assert(h.duet<h.sectors&&h.duet-h.source==49153);
        assert(!container_bounds(&h,h.sectors-1));h.rec++;assert(!container_bounds(&h,h.sectors));h.rec--;
    }
    h.version=3;assert(!container_bounds(&h,h.sectors));h.version=2;h.sectors=TC_SECTORS-2048;assert(!container_bounds(&h,TC_SECTORS));
    u32 out;assert(!disk_size_mb("33.554432",&out)&&out==65536);
    assert(!disk_size_mb("33,554432",&out)&&out==65536);
    assert(!disk_size_mb("34",&out)&&out==65536);
    assert(disk_size_mb("0",&out)&&disk_size_mb("1",&out));
    assert(disk_size_mb("-32",&out)&&disk_size_mb("32MiB",&out)&&disk_size_mb("33.",&out));
    assert(disk_size_mb("33.5544321",&out)&&disk_size_mb("1099511.627777",&out));
    assert(disk_size_mb("999999999999999999999999999999",&out));
    for(u64 i=2048;i<=TMC_DISK_MAX_SECTORS;i+=1048576){
        char exact[32];disk_size_mb_exact(exact,sizeof exact,(u32)i);assert(!disk_size_mb(exact,&out)&&out==i);
    }
    char exact[32];disk_size_mb_exact(exact,sizeof exact,TMC_DISK_MAX_SECTORS);assert(eq(exact,"1099511.627776"));assert(!disk_size_mb(exact,&out)&&out==TMC_DISK_MAX_SECTORS);
    puts("PASS M2 geometry: v1 backward compatibility, v2 min/10GiB/1TiB spans and malformed bounds; decimal MB exact round-trip, alignment never grows request, overflow/extra precision rejected");
}
