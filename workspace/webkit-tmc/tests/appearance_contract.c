#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "tmc_appearance.h"
#include "tmc_backend.h"
#include "system_palette.h"
static SysInfo settings;
static int failed;
long tmc_syscall(long n, uint64_t a, uint64_t b, uint64_t c, uint64_t d)
{
    assert(n == S_INFO && a && !b && !c && !d);
    if (failed) return -1;
    *(SysInfo *)(uintptr_t)a = settings;
    return 0;
}
int main(void)
{
    struct tmc_appearance a = {0};
    unsigned cases = 0;
    for (unsigned theme = 0; theme < NTHEME; ++theme)
    for (unsigned dark = 0; dark < 2; ++dark)
    for (unsigned motion = 0; motion < 2; ++motion)
    for (unsigned opaque = 0; opaque < 2; ++opaque) {
        settings.theme=theme;settings.dark=dark;settings.reduce_motion=motion;settings.opaque=opaque;
        assert(tmc_appearance_refresh(&a)==1);
        assert(a.theme==theme && a.dark==dark && a.reduce_motion==motion && a.opaque==opaque);
        for (unsigned slot=0;slot<10;slot++) assert(a.rgb[slot]==tmc_system_palette[theme][dark][slot]);
        assert(tmc_appearance_refresh(&a)==0);
        assert(tmc_system_color(theme,dark,0x123456)==0x123456);
        ++cases;
    }
    settings.wall=11;assert(tmc_appearance_refresh(&a)==1 && a.wallpaper==11);
    struct tmc_appearance before=a;failed=1;
    assert(tmc_appearance_refresh(&a)==-1 && errno==EIO && !memcmp(&a,&before,sizeof a));
    assert(tmc_appearance_refresh(NULL)==-1 && errno==EINVAL);
    failed=0;settings.theme=UINT32_MAX;settings.wall=UINT32_MAX;settings.dark=7;
    assert(tmc_appearance_refresh(&a)==1 && a.theme==UINT32_MAX%NTHEME && a.wallpaper==UINT32_MAX%NWALL && a.dark==1);
    printf("PASS: %u system appearance combinations, real S_INFO contract, shared compositor RGB palette, change detection and failure preservation.\n",cases);
}
