#pragma once
#include "gfx.h"
// Auth activity indicator. Twelve fixed, rounded white strokes; only their alpha
// changes. No color wheel, bitmaps of a cursor, sector fills or simulated progress.
#define ACTIVITY_RAYS 12
#define ACTIVITY_SIDE 32
static void login_activity(int cx, int cy, int phase)
{
    static u8 coverage[ACTIVITY_RAYS][ACTIVITY_SIDE * ACTIVITY_SIDE];
    static int ready;
    // Unit vectors in 8.8 fixed-point, clockwise starting at 12 o'clock.
    static const short dx[] = {0, 128, 222, 256, 222, 128, 0, -128, -222, -256, -222, -128};
    static const short dy[] = {-256, -222, -128, 0, 128, 222, 256, 222, 128, 0, -128, -222};
    static const u8 tail[] = {255, 226, 198, 170, 145, 122, 102, 85, 72, 62, 54, 47};
    if (!ready) {
        for (int k = 0; k < ACTIVITY_RAYS; k++) for (int j = 0; j < ACTIVITY_SIDE; j++) for (int i = 0; i < ACTIVITY_SIDE;
                        i++) {
                    int hits = 0;
                    for (int sy = 0; sy < 4; sy++) for (int sx = 0; sx < 4; sx++) {
                            int x = (i - 16) * 8 + sx * 2 + 1, y = (j - 16) * 8 + sy * 2 + 1;
                            int along = (x * dx[k] + y * dy[k]) / 32, cross = (y * dx[k] - x * dy[k]) / 32; // 1/64 px
                            // Capsule: 2.5px stroke, round caps centered at radii 7.5 and 13.25px.
                            int end = MAX(0, MAX(480 - along, along - 848));
                            hits += cross * cross + end * end <= 80 * 80;
                        }
                    coverage[k][j * ACTIVITY_SIDE + i] = (hits * 255 + 8) / 16;
                }
        ready = 1;
    }
    for (int k = 0; k < ACTIVITY_RAYS; k++) {
        int alpha = tail[(phase - k + ACTIVITY_RAYS) % ACTIVITY_RAYS];
        for (int j = 0; j < ACTIVITY_SIDE; j++) for (int i = 0; i < ACTIVITY_SIDE; i++) {
                int x = cx - 16 + i, y = cy - 16 + j;
                int a = (coverage[k][j * ACTIVITY_SIDE + i] * alpha + 127) / 255;
                if (a && x >= 0 && x < W && y >= 0 && y < H) {
                    u32 *p = canvas + y * W + x;
                    *p = a == 255 ? 0xffffff : blend(*p, 0xffffff, a);
                }
            }
    }
}
