#pragma once
#include "handoff.h"
// The alpha mask is rasterized from the supplied assets/apple.svg.
// Only grayscale pixels: identical byte order on RGB and BGR GOP modes.
static const unsigned char fruit_alpha[94 * 108] = {
#include "../assets/fruit.inc"
};
static inline void splash_logo(const struct Boot *b)
{
    if(b->boot_flags&1)return;
    volatile uint32_t *fb = (void *)b->fb;
    for (unsigned y = 0; y < b->h; y++)for (unsigned x = 0; x < b->w; x++)fb[y * b->stride + x] = 0;
    int X = (int)b->w / 2 - 47, Y = (int)b->h / 2 - 54;
    for (int y = 0; y < 108; y++)for (int x = 0; x < 94; x++) {
            unsigned a = fruit_alpha[y * 94 + x];
            fb[(Y + y)*b->stride + X + x] = a * 0x010101u;
        }
}
// 5x7 column-major bitmaps (bit0 = top row), just the glyphs of the boot hint.
// Drawn at 2x onto the splash so R's time window is discoverable on any GOP.
static const unsigned char hint_font[][5] = {
    /*  0 ' ' */ {0x00, 0x00, 0x00, 0x00, 0x00},
    /*  1 'P' */ {0x7f, 0x09, 0x09, 0x09, 0x06},
    /*  2 'r' */ {0x7c, 0x08, 0x04, 0x04, 0x08},
    /*  3 'e' */ {0x38, 0x54, 0x54, 0x54, 0x18},
    /*  4 'm' */ {0x7c, 0x04, 0x18, 0x04, 0x78},
    /*  5 'i' */ {0x00, 0x44, 0x7d, 0x40, 0x00},
    /*  6 'R' */ {0x7f, 0x09, 0x19, 0x29, 0x46},
    /*  7 'p' */ {0x7c, 0x14, 0x14, 0x14, 0x08},
    /*  8 'u' */ {0x3c, 0x40, 0x40, 0x20, 0x3c},
    /*  9 'c' */ {0x38, 0x44, 0x44, 0x44, 0x28},
    /* 10 'o' */ {0x38, 0x44, 0x44, 0x44, 0x38},
};
static inline int hint_index(char c)
{
    switch (c) {
        case ' ': return 0;
        case 'P': return 1;
        case 'r': return 2;
        case 'e': return 3;
        case 'm': return 4;
        case 'i': return 5;
        case 'R': return 6;
        case 'p': return 7;
        case 'u': return 8;
        case 'c': return 9;
        case 'o': return 10;
        default: return -1;
    }
}
static inline void splash_hint(const struct Boot *b, const char *s)
{
    volatile uint32_t *fb = (void *)b->fb;
    const int scale = 2, advance = 6 * scale;
    int len = 0;
    while (s[len]) len++;
    int x = ((int)b->w - len * advance + scale) / 2, y = (int)b->h / 2 + 122;
    for (const char *p = s; *p; p++) {
        int g = hint_index(*p);
        if (g > 0)
            for (int c = 0; c < 5; c++)for (int r = 0; r < 7; r++)
                if ((hint_font[g][c] >> r) & 1)
                    for (int dy = 0; dy < scale; dy++)for (int dx = 0; dx < scale; dx++)
                        fb[(y + r*scale + dy) * b->stride + x + c*scale + dx] = 0x8c8c8cu;
        x += advance;
    }
}
static inline void splash_bar(const struct Boot *b, int pc)
{
    if(b->boot_flags&1)return;
    volatile uint32_t *fb = (void *)b->fb;
    int width = 192, h = 6, X = (int)b->w / 2 - width / 2, Y = (int)b->h / 2 + 89;
    for (int y = 0; y < h; y++)for (int x = 0; x < width; x++) {
            int edge = x < 3 ? 3 - x : x >= width - 3 ? x - (width - 4) : 0, dy = y < 3 ? 2 - y : y - 3,
                ink = x < width * pc / 100 ? 238 : 52;
            if (edge * edge + dy * dy > 10)ink = 0;
            fb[(Y + y)*b->stride + X + x] = ink * 0x010101u;
        }
}
