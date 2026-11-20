#include "desk.h"
int lerpi(int a, int b, int p) {
    return a + ((b - a) * p >> 8);
}
int ease(int t) {
    t = clampi(t, 0, 256);
    return t * t * (768 - 2 * t) >> 16;
}
Rect mixrect(Rect a, Rect b, int t) {
    return (Rect) {
        lerpi(a.x, b.x, t), lerpi(a.y, b.y, t), MAX(1, lerpi(a.w, b.w, t)), MAX(1, lerpi(a.h, b.h, t))
    };
}
void startmotion(View *v, int k, Rect a, Rect b, int span) {
    v->motion = (Motion) {
        k, span, 0, 0, sys.ticks, sys.ticks, a, b
    };
    dirty = 1;
}
int motionstep(View *v, Rect *q, int *alpha, int *bend) {
    Motion *m = &v->motion;
    *q = v->box;
    *alpha = 255;
    *bend = 0;
    if (!m->kind)return 256;
    int dt = MIN(8ull, sys.ticks - m->last);
    m->last = sys.ticks;
    m->age = MIN(m->dur, m->age + dt);
    int t = sys.reduce_motion ? 256 : clampi(m->age * 256 / m->dur, 0, 256), p = ease(t);
    m->frames++;
    *q = mixrect(m->a, m->b, p);
    if (m->kind == AN_OPEN)*alpha = lerpi(38, 255, p);
    if (m->kind == AN_CLOSE)*alpha = lerpi(255, 0, p);
    if (m->kind == AN_MIN || m->kind == AN_RESTORE) {
        int u = m->kind == AN_MIN ? p : 256 - p;
        *bend = (u * (256 - u)) >> 8;
        *alpha = 255 - (u * 180 >> 8);
    }
    return t;
}
void motionfinish(View *v) {
    Motion *m = &v->motion;
#ifdef TMC_DEBUG
    if (m->kind) {
        static const char *const kn[] = {"idle", "open", "close", "minimize", "restore", "zoom", "mission"};
        char b[48], n[12];
        strset(b, "motion ", 48);
        append(b, kn[m->kind < 7 ? m->kind : 0], 48);
        append(b, " frames=", 48);
        number(n, m->frames);
        append(b, n, 48);
        trace(b);
    }
#endif
    m->kind = 0;
}
// Resample a real window texture. During Dock contraction the waist is narrowest
// midway down the surface; terminal size and position still coincide with the icon.
void sprite(const u32 *src, int sw, int sh, Rect q, int r, int alpha, int bend, int dir) {
    if (!src || sw <= 0 || sh <= 0 || q.w <= 0 || q.h <= 0 || alpha <= 0)return;
    int unit = sw == q.w && sh == q.h && alpha == 255 && !bend;
    r = MIN(r, MIN(q.w, q.h) / 2);
    for (int j = MAX(0, y1 - q.y); j < MIN(q.h, y2 - q.y); j++) {
        int yy = j * 256 / MAX(q.h, 1);
        int cut = bend ? ((yy * (256 - yy) >> 8) * bend * q.w >> 16) : 0;
        int wid = MAX(1, q.w - cut), X = q.x + cut * (dir > 0 ? 3 : 1) / 4;
        int lo = MAX(0, x1 - X), hi = MIN(wid, x2 - X);
        if (lo >= hi)continue;
        int sy = unit ? j : (int)((long)j * sh / q.h);
        const u32 *s = src + sy * sw;
        u32 *d = canvas + (q.y + j) * W + X + lo;
        if (unit && j >= r && j < q.h - r) {
            memcpy(d, s + lo, (hi - lo) * 4);
            continue;
        }
        int step = (sw << 16) / wid, t = lo * step;
        for (int i = lo; i < hi;) {
            if (unit && i >= r && i < wid - r) {
                int end = MIN(hi, wid - r);
                memcpy(d, s + i, (end - i) * 4);
                t += step * (end - i);
                d += end - i;
                i = end;
                continue;
            }
            int a = alpha;
            if ((j < r || j >= q.h - r) && (i < r || i >= wid - r))a = a * coverage(i, j, wid, q.h, r) / 255;
            if (a >= 255)*d = s[t >> 16];
            else if (a)*d = blend(*d, s[t >> 16], a);
            i++;
            d++;
            t += step;
        }
    }
}
