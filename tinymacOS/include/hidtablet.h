#pragma once
#include "base.h"
/* The supported HID application is one absolute XY pointer, optionally prefixed
 * by one report ID. Field offsets include that prefix. No VID/PID lookup. */
typedef struct {
    unsigned bit, width;
    int low, high;
} TabletField;
typedef struct {
    TabletField x, y, wheel, button[3];
    unsigned bytes, id;
    int relative;
} TabletLayout;
typedef struct {
    int x, y, buttons, wheel;
} TabletState;
/* Report-protocol absolute tablet OR signed relative USB mouse. */
int pointer_layout(const u8 *, unsigned, unsigned, TabletLayout *);
int pointer_read(const TabletLayout *, const u8 *, unsigned, unsigned, unsigned, TabletState *);
int tablet_layout(const u8 *descriptor, unsigned length, unsigned packet, TabletLayout *layout);
int tablet_read(const TabletLayout *, const u8 *, unsigned received, unsigned width, unsigned height,
                TabletState *);
/* Inclusive endpoints, independent X/Y dimensions; also accepts UEFI's 64-bit ranges. */
static inline int pointer_pixel(u64 value, u64 low, u64 high, unsigned extent)
{
    if (extent < 2 || high <= low || value <= low) return 0;
    if (value >= high) return (int)(extent - 1);
    value -= low;
    high -= low;
    while (high > UINT32_MAX) {
        value >>= 1;
        high >>= 1;
    }
    return (int)((value * (extent - 1) + high / 2) / high);
}
