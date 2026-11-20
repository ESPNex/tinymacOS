#include "hidtablet.h"
/* Bounded short-item HID parser for absolute tablets and relative USB mice. Unknown HID devices
 * do not silently become relative mice. Multiple report layouts, arrays as axes,
 * global stacks and digitizer/pen collections are deliberately not supported. */
static int signed_value(u32 v, unsigned bits)
{
    if (bits == 32) return (int32_t)v;
    u32 sign = (u32)1 << (bits - 1);
    return (int)(v & (sign - 1)) - (int)(v & sign);
}
static int bind_field(TabletField *f, unsigned off, unsigned size, int lo, int hi)
{
    if (f->width || !size || size > 16 || lo >= hi) return 0;
    int pm = 1 << (size - (lo < 0));
    if (lo < -pm || hi >= pm) return 0;
    *f = (TabletField) {
        off, size, lo, hi
    };
    return 1;
}
static int parse_layout(const u8 *d, unsigned n, unsigned packet, TabletLayout *out, int allow_relative)
{
    if (!d || !out || !n || n > 1024 || !packet || packet > 64) return 0;
    TabletLayout t = {0};
    unsigned page = 0, size = 0, count = 0, used = 0, depth = 0, mouse_depth = 0;
    unsigned usage[16], nu = 0, first = 0, last = 0;
    int range = 0, lo = 0, hi = 0, axis_mode = -1;
    for (unsigned i = 0; i < n;) {
        unsigned h = d[i++], z = h & 3, type = (h >> 2) & 3, tag = h >> 4;
        if (z == 3) z = 4;
        if (h == 0xfe || z > n - i) return 0;
        u32 v = 0;
        for (unsigned k = 0; k < z; k++) v |= (u32)d[i++] << (8 * k);
        if (type == 1) {
            switch (tag) {
            case 0:
                page = v;
                break;
            case 1:
                if (!z) return 0;
                lo = signed_value(v, z * 8);
                break;
            case 2:
                if (!z || (lo >= 0 && v > INT32_MAX)) return 0;
                hi = lo < 0 ? signed_value(v, z * 8) : (int)v;
                break;
            case 7:
                size = v;
                break;
            case 8:
                if (!v || v > 255 || t.id || used) return 0;
                t.id = v;
                used = 8;
                break;
            case 9:
                count = v;
                break;
            case 10:
            case 11:
                return 0;
            }
        } else if (type == 2) {
            if (v > 65535) return 0;
            if (tag == 0) {
                if (nu == COUNT(usage)) return 0;
                usage[nu++] = v;
            } else if (tag == 1) {
                first = v;
                range = 1;
            } else if (tag == 2) last = v;
        } else if (type == 0) {
            if (tag == 10) {
                if (++depth > 16) return 0;
                if (depth == 1 && v == 1 && page == 1 && nu == 1 && (usage[0] == 1 || usage[0] == 2)) mouse_depth = depth;
            } else if (tag == 12) {
                if (!depth) return 0;
                if (depth == mouse_depth) mouse_depth = 0;
                depth--;
            } else if (tag == 8) {
                if (!size || size > 32 || !count || count > (packet * 8 - used) / size) return 0;
                if (mouse_depth && !(v & 1) && (v & 2)) {
                    for (unsigned j = 0; j < count; j++) {
                        unsigned u = j < nu ? usage[j] : (range && last >= first && j <= last - first ? first + j : 0);
                        int rel = (v & 4) != 0;
                        TabletField *f = 0;
                        if (page == 1 && (u == 0x30 || u == 0x31)) {
                            if ((rel && !allow_relative) || (axis_mode >= 0 && axis_mode != rel)) return 0;
                            if (rel && lo >= 0) return 0; /* relative deltas must be signed */
                            axis_mode = rel;
                            t.relative = rel;
                            f = u == 0x30 ? &t.x : &t.y;
                        }
                        if (page == 1 && rel && u == 0x38) f = &t.wheel;
                        if (page == 9 && !rel && u >= 1 && u <= 3 && size == 1) f = &t.button[u - 1];
                        if (f && !bind_field(f, used + j * size, size, lo, hi)) return 0;
                    }
                }
                used += size * count;
            }
            nu = first = last = 0;
            range = 0;
        }
    }
    if (depth || !t.x.width || !t.y.width || !t.button[0].width) return 0;
    t.bytes = (used + 7) / 8;
    *out = t;
    return 1;
}
int pointer_layout(const u8 *d, unsigned n, unsigned packet, TabletLayout *out)
{
    return parse_layout(d,n,packet,out,1);
}
int tablet_layout(const u8 *d, unsigned n, unsigned packet, TabletLayout *out)
{
    return parse_layout(d,n,packet,out,0);
}
static int extract(const u8 *p, TabletField f)
{
    u32 v = 0;
    for (unsigned j = 0; j < f.width; j++) {
        unsigned b = f.bit + j;
        v |= (u32)((p[b / 8] >> (b % 8)) & 1) << j;
    }
    return f.low < 0 ? signed_value(v, f.width) : (int)v;
}
static int axis(const u8 *p, TabletField f, unsigned extent)
{
    int val = extract(p, f);
    return pointer_pixel(val <= f.low ? 0 : (u64)(val - f.low), 0, (unsigned)(f.high - f.low), extent);
}
int pointer_read(const TabletLayout *t, const u8 *report, unsigned received, unsigned w, unsigned h, TabletState *s)
{
    if (!t || !t->bytes || !report || received < t->bytes || !w || !h || !s || (t->id && report[0] != t->id)) return 0;
    s->x = t->relative ? extract(report, t->x) : axis(report, t->x, w);
    s->y = t->relative ? extract(report, t->y) : axis(report, t->y, h);
    s->buttons = 0;
    for (unsigned i = 0; i < 3; i++) if (extract(report, t->button[i])) s->buttons |= 1 << i;
    s->wheel = extract(report, t->wheel);
    return 1;
}

int tablet_read(const TabletLayout *t, const u8 *report, unsigned received, unsigned w, unsigned h, TabletState *s)
{
    return t && !t->relative ? pointer_read(t,report,received,w,h,s) : 0;
}
