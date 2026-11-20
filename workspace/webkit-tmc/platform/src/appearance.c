#include "tmc_appearance.h"
#include "tmc_backend.h"
#include "system_palette.h"
#include <errno.h>
int tmc_appearance_refresh(struct tmc_appearance *state)
{
    if (!state) { errno = EINVAL; return -1; }
    SysInfo info = {0};
    if (tmc_syscall(S_INFO, (uintptr_t)&info, 0, 0, 0) < 0) { errno = EIO; return -1; }
    struct tmc_appearance next = {0};
    next.theme = info.theme % NTHEME;
    next.dark = !!info.dark;
    next.reduce_motion = !!info.reduce_motion;
    next.opaque = !!info.opaque;
    next.wallpaper = info.wall % NWALL;
    next.initialized = 1;
    for (unsigned i = 0; i < 10; ++i)
        next.rgb[i] = tmc_system_color(next.theme, next.dark, INK(i));
    int changed = !state->initialized || next.theme != state->theme || next.dark != state->dark ||
        next.reduce_motion != state->reduce_motion || next.opaque != state->opaque || next.wallpaper != state->wallpaper;
    if (changed) *state = next;
    return changed;
}
