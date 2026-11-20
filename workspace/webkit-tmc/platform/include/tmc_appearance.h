#ifndef TMC_APPEARANCE_H
#define TMC_APPEARANCE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
struct tmc_appearance {
    uint32_t theme, dark, reduce_motion, opaque, wallpaper;
    uint32_t rgb[10];
    int initialized;
};
/* Real S_INFO settings snapshot; -1 failure, 0 unchanged, 1 changed.
 * Call on EV_THEME and after creation of the future browser window.
 * This does not change protected system preferences. */
int tmc_appearance_refresh(struct tmc_appearance *state);
#ifdef __cplusplus
}
#endif
#endif
