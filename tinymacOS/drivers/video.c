#include "../kernel/k.h"
// A shadow of the physical scanout avoids expensive MMIO writes to unchanged tiles.
static u32 shadow[1920 * 1080];   /* one full frame at the largest switchable mode */
static int first = 1;
static u64 last_refresh;
/* sw/sh = logical source canvas; boot.w/boot.h = physical scanout. Equal sizes
 * take the direct path; otherwise every physical pixel samples the logical
 * canvas (constant on-screen UI size in every resolution). */
void video_present(const u32 *src, int x0, int y0, int width, int height, int sw, int sh) {
    u32 *s = shadow, *d = (u32 *)boot.fb;
    /* Shadow equality cannot prove scanout equality. Periodic full writes repair
     * stale hardware pixels without reading back slow MMIO. The desktop sends
     * a heartbeat even when no input/damage occurs. Promote partial rectangles
     * on repair, otherwise an unchanged band survives outside the cursor tile. */
    int refresh=first || ticks-last_refresh>=100;
    if(refresh){x0=y0=0;width=sw;height=sh;last_refresh=ticks;}
    if (sw <= 0 || sh <= 0 || sw != boot.w || sh != boot.h) {
        if (sw <= 0 || sh <= 0) return;
        for (unsigned y = 0; y < boot.h; y++) {
            unsigned sy = (unsigned)((u64)y * (u32)sh / boot.h);
            if (sy >= (unsigned)sh) sy = sh - 1;
            const u32 *row = src + (u64)sy * sw;
            for (unsigned x = 0; x < boot.w; x++) {
                unsigned sx = (unsigned)((u64)x * (u32)sw / boot.w);
                if (sx >= (unsigned)sw) sx = sw - 1;
                u32 v = row[sx];
                unsigned si = y * boot.w + x, di = y * boot.stride + x;
                if (!refresh && s[si] == v) continue;
                s[si] = v;
                if (!boot.rgb)
                    d[di] = v;
                else
                    d[di] = ((v & 255) << 16) | (v & 0xff00) | ((v >> 16) & 255);
            }
        }
        if (first)first--;
        asm volatile("sfence":::"memory");
        return;
    }
    s += y0 * boot.w;
    src += y0 * boot.w;
    d += y0 * boot.stride;
    for (int y = 0; y < height; y++) {
        for (unsigned x = x0; x < (unsigned)(x0 + width);) {
            unsigned n = MIN(32u, (unsigned)(x0 + width) - x);
            int change = refresh;
            for (unsigned k = 0; !change && k < n; k++)
                if (s[x + k] != src[x + k])
                    change = 1;
            if (change) {
                memcpy(s + x, src + x, n * 4);
                if (!boot.rgb)
                    memcpy(d + x, src + x, n * 4);
                else
                    for (unsigned k = 0; k < n; k++) {
                        u32 v = src[x + k];
                        d[x + k] = ((v & 255) << 16) | (v & 0xff00) | ((v >> 16) & 255);
                    }
            }
            x += n;
        }
        s += boot.w;
        src += boot.w;
        d += boot.stride;
    }
    asm volatile("sfence":::"memory");
    if (first)first--;   /* keep full-writing for several frames after a mode
                           switch: QEMU may recreate its surface deferred */
}

// Runtime resolution switch on the Bochs/QEMU stdvga (VBE DISPI registers).
// The desktop renders a logical canvas that is scaled to the scanout, so the
// on-screen UI size (readable DPI) stays identical in every mode.
#define DISPI_IDX 0x01CE
#define DISPI_DAT 0x01CF
int video_runtime_available(void)
{
    /* Never probe emulator-specific I/O ports on arbitrary physical GPUs. */
    static int cached=-1;if(cached>=0)return cached;
    u32 d=pci_find(0x030000);u32 id=d?pci_reg(d,0):0;
    cached=id==0x11111234u||id==0xbeef80eeu;return cached;
}
int video_setmode(int w, int h)
{
#ifdef TMC_DEBUG
    {
        char nb[16], tb[40];
        tb[0] = 0;
        append(tb, "[vsm] ", 40);
        number(nb, w);
        append(tb, nb, 40);
        append(tb, "x", 40);
        number(nb, h);
        append(tb, nb, 40);
        logstr(tb);
        logstr("\n");
    }
#endif
    if (!video_runtime_available() || w < 800 || h < 600 || w > 1920 || h > 1080)
        return -1;
    outw(DISPI_IDX, 0);
    if ((inw(DISPI_DAT) >> 8) != 0xB0) { /* VBE_DISPI_IDn present */
#ifdef TMC_DEBUG
        logstr("[vsm] idfail\n");
#endif
        return -1;
    }
    outw(DISPI_IDX, 4);
    u16 en = inw(DISPI_DAT);
    outw(DISPI_IDX, 4);
    outw(DISPI_DAT, (u16)(en & ~1u));    /* disable while reprogramming */
    outw(DISPI_IDX, 1);
    outw(DISPI_DAT, (u16)w);
    outw(DISPI_IDX, 2);
    outw(DISPI_DAT, (u16)h);
    outw(DISPI_IDX, 3);
    outw(DISPI_DAT, 32);
    outw(DISPI_IDX, 5);
    outw(DISPI_DAT, 0);                  /* virtual width follows xres */
    outw(DISPI_IDX, 7);
    outw(DISPI_DAT, 0);
    outw(DISPI_IDX, 8);
    outw(DISPI_DAT, 0);
    outw(DISPI_IDX, 4);
    outw(DISPI_DAT, (u16)(en | 1));      /* restore enable bits (LFB …) */
    boot.w = (u32)w;
    boot.h = (u32)h;
    boot.stride = (u32)w;
    first = 8;                           /* shadow stale: full repaints */
    return 0;
}
