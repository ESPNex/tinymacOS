#include "desk.h"
#include "pointer_scale.h"
View views[NPROC];
u32 *desk_ras;               /* global C_RASTER staging (one payload at a time) */
int order[NPROC], nz, dirty = 1, mx, my, buttons;
SysInfo sys;
static int pend = -1, drag = -1, mode, ax, ay, clickPid, cx, cy, wallEpoch;
static Rect drag0;
static u64 clickTime;
static u32 undo[1920 * 1080];
int cursorDirty = 1;
static int curx, cury, curw, curh;
static u32 undercursor[28 * 28];
static int cursorValid;
static PointerScale pointer_scale;
static u64 scanout_heartbeat;
static u32 prevwall[1920 * 1080];
// Debug view geometry for the test harness: "view <app> <x> <y> <w> <h>"
// (outer rect, title included; clients add TITLE to reach their origin).
#ifdef TMC_DEBUG
static void viewtrace(int app, Rect r)
{
    char b[56], n[12];
    strset(b, "view ", 56);
    number(n, app);
    append(b, n, 56);
    append(b, " ", 56);
    number(n, r.x);
    append(b, n, 56);
    append(b, " ", 56);
    number(n, r.y);
    append(b, n, 56);
    append(b, " ", 56);
    number(n, r.w);
    append(b, n, 56);
    append(b, " ", 56);
    number(n, r.h);
    append(b, n, 56);
    trace(b);
}
#else
#define viewtrace(app, r) ((void)0)
#endif
static int wallpaper_age = 32;
// Mission Control: windows eased into a full-screen grid (F10).
int mc = 0;
static int mcdim;
static Rect mcslot(Rect, int, int);
static int mcvis(void);
// Only layers intersecting a material's sampling area invalidate it.
static struct {
    Rect area;
    u64 generation;
} layers[NPROC + 1];
static int nlayer;
static u64 sceneSeed;
static int lights;
static int titlePid;
static u64 titleTick;
static int titlex, titley;

static u32 *buf(int w, int h)
{
    long r = sc(S_ALLOC, (u64)w * h * 4, 0, 0, 0);
    return r > 0 ? (u32 *)r : 0;
}
static void dropbuf(u32 **p)
{
    if (*p)sc(S_FREE, *p, 0, 0, 0);
    *p = 0;
}
int topwin(void)
{
    for (int j = nz; j--;) {
        int i = order[j];
        if (!views[i].mini && !views[i].dead)return i;
    }
    return -1;
}
static void cleanup(int i)
{
    View *v = views + i;
    dropbuf(&v->surface);
    dropbuf(&v->previous);
    for (int j = 0; j < nz; j++)if (order[j] == i) {
            for (; j < nz - 1; j++)order[j] = order[j + 1];
            nz--;
            break;
        }
    if (drag == i)drag = -1;
    v->used = 0;
    dirty = 1;
}
static Rect dockrect(int app)
{
    int x, y;
    dockcenter(app, &x, &y);
    return (Rect) {
        x - 18, y - 18, 36, 36
    };
}
static Rect inset(Rect q, int dx, int dy)
{
    return (Rect) {
        q.x + dx, q.y + dy, q.w - 2 * dx, q.h - 2 * dy
    };
}
void eventto(int i, Event e)
{
    if (i >= 0 && !views[i].dead)sc(S_SEND, views[i].s.pid, &e, 0, 0);
}
int focuswin(int i)
{
    if (i < 0 || !views[i].used || views[i].dead)return -1;
    View *v = views + i;
    int old = topwin();
    if (v->mini) {
        v->mini = 0;
        startmotion(v, AN_RESTORE, dockrect(v->s.app), v->box, 36);
        viewtrace(v->s.app, v->box);
    }
    for (int j = 0; j < nz; j++)if (order[j] == i) {
            for (; j < nz - 1; j++)order[j] = order[j + 1];
            order[nz - 1] = i;
            break;
        }
    if (old != i) {
        v->focusWait = 12;
        dirty = 1;
    }
    return i;
}
void openarg(int a, const char *arg)
{
    if (sc(S_SPAWN, a, arg, 0, 0) > 0) {
        pend = a;
        shell_pop(a);
        dirty = 1;
    } else trace("Avvio rifiutato: limite processi");
}
void launch(int a)
{
    if (a < FINDER || a > lastapp(sys.live_cd))return;
    if (a == INSTALLER && !sys.live_cd)return;
    for (int j = nz; j--;) {
        int i = order[j];
        if (!views[i].dead && views[i].s.app == a) {
            focuswin(i);
            return;
        }
    }
    if (pend != a)openarg(a, 0);
}
static Rect fit(int w, int h, int count, int app)
{
    w = MIN(w, W - 28);
    h = MIN(h + TITLE + FOOT, H - BAR - 100);
    int x = (W - w) / 2 + (count ? ((count * 43) % 105) - 20 : app == FINDER ? -W / 10 : 0),
        y = (H - h) / 2 - 15 + (count ? ((count * 13) % 48) - 16 : 0);
    return (Rect) {
        clampi(x, 8, W - w - 8), clampi(y, BAR + 10, H - h - 14), w, h
    };
}
static void resize(int i)
{
    Rect q = views[i].box;
    eventto(i, (Event) {
        .type = EV_RESIZE, .a = q.w - 2, .b = q.h - TITLE - FOOT
    });
}
static void wins(void)
{
    Win list[NPROC];
    int n = sc(S_WLIST, list, NPROC, 0, 0), seen[NPROC] = {0};
    for (int a = 0; a < n; a++) {
        int i = 0;
        while (i < NPROC && (!views[i].used || views[i].s.pid != list[a].pid))i++;
        if (i == NPROC) {
            i = 0;
            while (i < NPROC && views[i].used)i++;
            if (i == NPROC)continue;
            View *v = views + i;
            memset(v, 0, sizeof * v);
            v->used = 1;
            v->s = list[a];
            v->s.seq = ~0u;
            v->box = fit(list[a].w, list[a].h, nz, list[a].app);
            order[nz++] = i;
            Rect f = inset(v->box, v->box.w / 19, v->box.h / 19);
            f.y += 24;
            startmotion(v, AN_OPEN, f, v->box, 31);
            viewtrace(list[a].app, v->box);
            if (list[a].app == pend)pend = -1;
            if (v->box.w != list[a].w || v->box.h - TITLE - FOOT != list[a].h)resize(i);
        }
        seen[i] = 1;
        View *v = views + i;
        if (v->s.seq != list[a].seq) {
            if (v->surface && list[a].transition && !sys.reduce_motion && !v->motion.kind) {
                dropbuf(&v->previous);
                v->previous = v->surface;
                v->surface = 0;
                if (v->previous) {
                    v->oldw = v->sw;
                    v->oldh = v->sh;
                    v->sw = v->sh = 0;
                    v->contentAge = 0;
                    v->contentTick = sys.ticks;
                    v->contentFx = list[a].transition;
                }
            }
            v->s = list[a];
            if (!desk_ras)
                desk_ras = (u32 *)sc(S_ALLOC, 8 * 1024 * 1024, 0, 0, 0);
            v->n = sc(S_WGET, v->s.pid, v->cmd, NCMDS, (u64)desk_ras);
            v->key = 0;
            dirty = 1;
        }
    }
    for (int j = nz; j--;) {
        int i = order[j];
        View *v = views + i;
        if (!seen[i] && !v->dead) {
            v->dead = 1;
            if (v->mini || !v->surface || sys.reduce_motion) {
                cleanup(i);
                continue;
            }
            Rect f = inset(v->box, v->box.w / 15, v->box.h / 15);
            f.y += 12;
            startmotion(v, AN_CLOSE, v->box, f, 22);
        }
    }
}
static void wincontent(View *v, int focus)
{
    Rect q = v->box;
    int sheet=0;for(int i=0;i<v->n;i++)if(v->cmd[i].type==C_GLASS){sheet=1;break;}
    glass_sheet_backdrop(q,sheet);
    glass(q, TMC_CHROME_RADIUS, MAT_WINDOW);
    int x = q.x, y = q.y, w = q.w;
    u32 lights[] = {tmc_chrome_light(0), tmc_chrome_light(1), tmc_chrome_light(2)};
    for (int k = 0; k < 3; k++) {
        roundr(x + 17 + k * 20, y + 16, 13, 13, 6, focus ? lights[k] : color(LN), 255);
        stroke((Rect) {
            x + 17 + k * 20, y + 16, 13, 13
        }, 6, 0x274060, 42);
    }
    int cx = x + w / 2;
    center(cx, y + TMC_CHROME_TEXT_Y, v->s.title, F_BOLD, focus ? TX : SUB);
    roundr(x + 1, y + TITLE - 1, w - 2, 1, 0, night ? 0xffffff : 0x253148, night ? 26 : 29);
    client(x + 1, y + TITLE, w - 2, q.h - TITLE - FOOT, v->cmd, MAX(0, v->n));
    stroke(q, 18, night ? 0xcaddff : 0xffffff, focus ? 152 : 87);
}
static u64 hash(u64 h, u64 a)
{
    return (h ^ a) * 0x100000001b3ull;
}
static u64 geo(u64 h, Rect q)
{
    return hash(hash(h, (u32)q.x | ((u64)(u32)q.y << 32)), (u32)q.w | ((u64)(u32)q.h << 32));
}
u64 backdrop_key(Rect q)
{
    u64 key = sceneSeed;
    for (int i = 0; i < nlayer; i++) {
        Rect b = layers[i].area;
        if (b.x + b.w + 36 > q.x && b.x - 36 < q.x + q.w && b.y + b.h + 40 > q.y &&
                b.y - 36 < q.y + q.h)key = hash(key, layers[i].generation);
    }
    return key;
}
static int cache(View *v, u64 tag, int focus)
{
    Rect q = v->box;
    int correct = v->sw == q.w && v->sh == q.h;
    if (!correct || !v->surface) {
        dropbuf(&v->surface);
        v->surface = buf(q.w, q.h);
        v->sw = q.w;
        v->sh = q.h;
        v->key = 0;
    }
    if (!v->surface)return 0;
    if (v->key == tag || v->dead)return 1;
    copyout(q, undo);
    wincontent(v, focus);
    copyout(q, v->surface);
    copyin(q, undo);
    v->key = tag;
    return 1;
}
void wintile(int i, Rect q)
{
    View *v = views + i;
    if (v->surface)sprite(v->surface, v->sw, v->sh, q, 9, 255, 0, 0);
}
static void undocursor(void)
{
    if (cursorValid) {
        copyin((Rect) {
            curx, cury, curw, curh
        }, undercursor);
        cursorValid = 0;
    }
}
static void drawcursor(void)
{
    // Clip the saved damage region, not the sprite origin: the hotspot stays
    // at the reported pixel even when the sprite crosses the top/left edge.
    int left=mx-3, top=my-2;
    curx=MAX(0,left); cury=MAX(0,top);
    curw=MIN(W,left+26)-curx; curh=MIN(H,top+26)-cury;
    copyout((Rect) {
        curx, cury, curw, curh
    }, undercursor);
    icon(left, top, 26, 15);
    cursorValid = 1;
}
static void scanrect(int x, int y, int w, int h)
{
    sc(S_PRESENT, canvas, ((u64)(u32)x << 32) | (u32)y, ((u64)(u32)w << 32) | (u32)h, ((u64)(u32)W << 32) | (u32)H);
}
static void render(int dt)
{
    dirty = 0;
    undocursor();
    restorebg();
    if (wallpaper_age < 32 && !sys.reduce_motion) {
        wallpaper_age = MIN(32, wallpaper_age + dt);
        int p = ease(wallpaper_age * 8);
        for (int i = 0; i < W * H; i++)canvas[i] = blend(prevwall[i], canvas[i], p);
        dirty = 1;
    }
    // Advance the dim layer BEFORE keying any cached material. Chrome caches
    // include shadow padding (real backdrop pixels), not only the glass body.
    // Reusing a tile from a different dim phase leaves a rectangle/halo at exit.
    {
        int target = mc ? 112 : 0;
        if (mcdim != target) {
            int step = 28;
            mcdim += mcdim < target ? MIN(step, target - mcdim) : -MIN(step, mcdim - target);
            dirty = 1;
        }
    }
    nlayer = 0;
    sceneSeed = hash(hash(hash(12631+sys.fsrev, wallEpoch * 97 + wallpaper_age), sys.day + 32 * sys.month), mcdim);
    shell_base();
    if (mcdim > 0)roundr(0, BAR, W, H - BAR, 0, 0x07090f, mcdim);
    int mcn = mc ? mcvis() : 0, mcidx = 0;
    int focus = topwin();
    for (int j = 0; j < nz;) {
        int i = order[j];
        View *v = views + i;
        if (v->mini && !v->motion.kind) {
            j++;
            continue;
        }
        u64 tag = backdrop_key(v->box);
        u64 key = geo(hash(hash(hash(tag, v->s.seq), v->s.pid), focus == i), v->box) | 1;
        Rect q;
        int alpha, bend;
        int p = motionstep(v, &q, &alpha, &bend);
        int mk = v->motion.kind;
        if (mcn && !v->mini && !v->dead) {
            // Settled windows stay on their grid slot while Mission Control is open.
            Rect slot = mcslot(v->box, mcidx, mcn);
            if (!mk)q = slot;
            mcidx++;
        }
        if (mk)dirty = 1;
        if (cache(v, key, focus == i)) {
            int radius = MAX(2, MIN(18, q.w * 18 / MAX(1, v->box.w)));
            shadow(q, radius, focus == i ? 52 : 29);
            if (v->previous && !mk && !sys.reduce_motion) {
                v->contentAge += MIN(7ull, sys.ticks - v->contentTick);
                v->contentTick = sys.ticks;
                int t = ease(clampi(v->contentAge * 256 / 21, 0, 256));
                // Animate the contents, not the window frame: keep the title and outline stationary.
                sprite(v->surface, v->sw, v->sh, q, radius, 255, 0, 0);
                if (t < 256) {
                    Rect oq = q, nq = q;
                    clip(q.x + 2, q.y + TITLE, q.w - 4, q.h - TITLE - 10);
                    sprite(v->previous, v->oldw, v->oldh, oq, radius, 255, 0, 0);
                    if (v->contentFx == FX_SLIDE) {
                        oq.x -= t * 11 / 256;
                        nq.x += (256 - t) * 11 / 256;
                    } else if (v->contentFx == FX_SHEET)nq.y += (256 - t) * 7 / 256;
                    sprite(v->previous, v->oldw, v->oldh, oq, radius, 255, 0, 0);
                    sprite(v->surface, v->sw, v->sh, nq, radius, MIN(255, t), 0, 0);
                    resetclip();
                    dirty = 1;
                } else dropbuf(&v->previous);
                tag = hash(tag, t);
            } else {
                if (v->previous)dropbuf(&v->previous);
                sprite(v->surface, v->sw, v->sh, q, radius, alpha, bend, q.x >= v->box.x ? 1 : -1);
            }
            if (v->focusWait > 0) {
                if (!sys.reduce_motion)stroke(q, radius, 0xb6d6ff, MIN(160, v->focusWait * 12));
                v->focusWait = MAX(0, v->focusWait - dt);
                dirty = 1;
            }
        } else wincontent(v, focus == i); // Hover marks are composited over the cached, untransformed traffic lights.
        if (!mk && lights == i + 1 && focus == i) {
            int x = q.x + 20, y = q.y + 20;
            line(x, y, x + 5, y + 5, 0x803235);
            line(x + 5, y, x, y + 5, 0x803235);
            line(x + 19, y + 3, x + 26, y + 3, 0x895d19);
            line(x + 39, y + 5, x + 45, y - 1, 0x286948);
            line(x + 39, y + 2, x + 39, y + 5, 0x286948);
            line(x + 43, y - 1, x + 45, y - 1, 0x286948);
        }
        layers[nlayer].area = q;
        layers[nlayer++].generation = geo(hash(hash(hash(hash(tag, key), alpha), v->focusWait), lights == i + 1), q);
        if (mk && p == 256) {
            motionfinish(v);
            if (v->dead) {
                cleanup(i);
                continue;
            }
        }
        j++;
    }
    shell_draw();
    drawcursor();
    sc(S_PRESENT, canvas, 0, 0, ((u64)(u32)W << 32) | (u32)H);
    cursorDirty = 0;
}
// Grid slot for window `i` of `n` visible windows: aspect-fit inside its cell.
static Rect mcslot(Rect box, int i, int n)
{
    if (n < 1)n = 1;
    int cols = 1;
    while (cols * cols < n)cols++;
    int rows = (n + cols - 1) / cols;
    int pad = 16, top = BAR + 46, bottom = H - 16;
    int cw = (W - pad * (cols + 1)) / cols;
    int ch = (bottom - top - pad * (rows - 1)) / rows;
    if (ch < 60)ch = 60;
    int col = i % cols, row = i / cols;
    int w = box.w, h = box.h;
    if (w > cw) {
        h = h * cw / w;
        w = cw;
    }
    if (h > ch) {
        w = w * ch / h;
        h = ch;
    }
    return (Rect) {
        pad + col * (cw + pad) + (cw - w) / 2, top + row * (ch + pad) + (ch - h) / 2, MAX(1, w), MAX(1, h)
    };
}
static int mcvis(void)
{
    int n = 0;
    for (int j = 0; j < nz;j++)if (!views[order[j]].mini && !views[order[j]].dead)n++;
    return n;
}
void mc_enter(void)
{
    if (mc || gate)return;
    mc = 1;
    int n = mcvis(), idx = 0;
    for (int j = 0; j < nz;j++) {
        View *v = views + order[j];
        if (v->mini || v->dead)continue;
        startmotion(v, AN_MC, v->box, mcslot(v->box, idx, n), sys.reduce_motion ? 1 : 24);
        idx++;
    }
    dirty = 1;
}
void mc_exit(void)
{
    if (!mc)return;
    mc = 0;
    int n = mcvis(), idx = 0;
    for (int j = 0; j < nz;j++) {
        View *v = views + order[j];
        if (v->mini || v->dead)continue;
        startmotion(v, AN_MC, mcslot(v->box, idx, n), v->box, sys.reduce_motion ? 1 : 20);
        idx++;
    }
    dirty = 1;
}
int mchit(int x, int y)
{
    int n = mcvis();
    if (!n)return -1;
    int idx = 0, hit = -1;
    for (int j = 0; j < nz;j++) {
        View *v = views + order[j];
        if (v->mini || v->dead)continue;
        Rect s = mcslot(v->box, idx, n);
        if (x >= s.x && x < s.x + s.w && y >= s.y && y < s.y + s.h)hit = order[j];
        idx++;
    }
    return hit;
}
void togglepref(int a)
{
    u32 v = a == 0 ? (sys.theme+1)%NTHEME : a == 1 ? (sys.wall + 1) % NWALL : a == 2 ? !sys.dock : a == 3 ? !sys.reduce_motion :
            !sys.opaque;
    sc(S_PREF, a, v, 0, 0);
    dirty = 1;
}
void action(int a)
{
    int i = topwin();
    if (a >= 101 && a <= 100 + LASTAPP) {
        launch(a - 100);
        return;
    }
    if (a == 143 || a == 144) {
        togglepref(a - 143);
        return;
    }
    if (a == 145) {
        overlay_show(OV_HELP);
        return;
    }
    if (a == 150 || a == 151) {
        overlay_show(a == 150 ? OV_RESTART : OV_OFF);
        return;
    }
    if (a == 152) {
        if (sc(S_LOCK, 0, 0, 0, 0) == 0) {
            drag = -1;
            shell_reset();
            login_enter();
        }
        return;
    }
    if (i < 0)return;
    View *v = views + i;
    if (a == 140) {
        Rect old = v->box;
        if (!v->max) {
            v->saved = old;
            int x = sys.dock ? 88 : 9;
            v->box = (Rect) {
                x, BAR + 7, W - x - 9, H - (BAR + 7) - (sys.dock ? 10 : 91)
            };
        } else v->box = v->saved;
        v->max = !v->max;
        resize(i);
        startmotion(v, AN_ZOOM, old, v->box, 32);
        viewtrace(v->s.app, v->box);
    } else if (a == 141) {
        v->mini = 1;
        startmotion(v, AN_MIN, v->box, dockrect(v->s.app), 43);
    } else if (a == 142)eventto(i, (Event) {
        .type = EV_CLOSE
    });
    else if (a < 100)eventto(i, (Event) {
        .type = EV_ACTION, .a = a
    });
    dirty = 1;
}
static void down(void)
{
    if (shell_click(mx, my))return;
    for (int j = nz; j--;) {
        int i = order[j];
        View *v = views + i;
        if (v->mini || v->dead || !inside(mx, my, v->box))continue;
        focuswin(i);
        Rect q = v->box;
        int x = mx - q.x, y = my - q.y;
        if (y < TITLE && x >= 11 && x < 73 && y >= 9 && y < 35) {
            action(x < 34 ? 142 : x < 54 ? 141 : 140);
            return;
        }
        if (y < TITLE) {
            if (titlePid == v->s.pid && sys.ticks - titleTick < 38 && mx >= titlex - 4 && mx <= titlex + 4 && my >= titley - 4 &&
                    my <= titley + 4) {
                titlePid = 0;
                action(140);
                return;
            }
            titlePid = v->s.pid;
            titleTick = sys.ticks;
            titlex = mx;
            titley = my;
            if (v->max)return;
            drag = i;
            mode = 1;
        } else if (x > q.w - 18 && y > q.h - 17 && !v->max) {
            drag = i;
            mode = 2;
        } else {
            int dbl = clickPid == v->s.pid && sys.ticks - clickTime < 42 && mx >= cx - 4 && mx <= cx + 4 && my >= cy - 4 &&
                      my <= cy + 4;
            eventto(i, (Event) {
                .type = EV_CLICK, .a = x - 1, .b = y - TITLE, .c = dbl
            });
            clickTime = sys.ticks;
            clickPid = v->s.pid;
            cx = mx;
            cy = my;
            return;
        }
        ax = mx;
        ay = my;
        drag0 = q;
        return;
    }
}
static void input_wheel(int amount)
{
    if(!amount || buttons || my<BAR || shell_overlay_open())return;
    for(int j=nz;j--;) {
        int id=order[j];View *v=views+id;
        if(v->dead || v->mini || !inside(mx,my,v->box))continue;
        if(my>=v->box.y+TITLE)eventto(id,(Event){.type=EV_SCROLL,.a=clampi(amount,-16,16)});
        break;
    }
}
static void input(Event *e)
{
    if (e->type == EV_KEY) {
        if (e->b && e->c && (e->a == 'q' || e->a == 'Q')) {
            action(152);
            return;
        }
        if (shell_key(e))return;
        if(e->a==K_F11){if(sys.live_cd)launch(INSTALLER);return;}
        if (e->a >= K_F1 && e->a <= K_F9) {
            launch(1 + e->a - K_F1);
            return;
        }
        if (e->b && (e->a == 'w' || e->a == 'm')) {
            action(e->a == 'w' ? 142 : 141);
            return;
        }
        if (e->b && e->a == '\t' && nz > 1) {
            for (int j = nz - 2; j >= 0; j--)if (!views[order[j]].dead) {
                    focuswin(order[j]);
                    break;
                }
            return;
        }
        eventto(topwin(), *e);
    } else if (e->type == EV_MOUSE || e->type == EV_POINTER_ABS) {
        pointer_position(e);
        lightx = mx;
        lighty = my;
        cursorDirty = 1;
        int i = topwin(), hl = 0;
        if (i >= 0 && !mc) {
            Rect b = views[i].box;
            if (mx >= b.x + 11 && mx < b.x + 76 && my >= b.y + 8 && my < b.y + 36)hl = i + 1;
        }
        if (hl != lights) {
            lights = hl;
            dirty = 1;
        }
        shell_mouse();
        shell_control_pointer(e->c & 1);
        if ((e->c & 2) && !(buttons & 2)){drag=-1;shell_context();}
        else if ((e->c & 1) && !(buttons & 1))down();
        else if ((e->c & 1) && drag >= 0) {
            View *v = views + drag;
            if (mode == 1) {
                v->box.x = clampi(drag0.x + mx - ax, 0, W - v->box.w);
                v->box.y = clampi(drag0.y + my - ay, BAR, H - v->box.h);
            } else {
                v->box.w = clampi(drag0.w + mx - ax, MIN(v->s.app==INSTALLER?659:460, W - v->box.x), W - v->box.x);
                v->box.h = clampi(drag0.h + my - ay, MIN(v->s.app == INSTALLER ? 426 : v->s.app == SETTINGS ? 398 : v->s.app == CALENDAR ||
                                  v->s.app == MONITOR ? 365 : 286, H - v->box.y), H - v->box.y);
                resize(drag);
            }
            dirty = 1;
        } else if (!(e->c & 1) && drag >= 0) {
            trace(mode == 1 ? "window moved" : "window resized");
            viewtrace(views[drag].s.app, views[drag].box);
            drag = -1;
        }
        buttons = e->c;
        input_wheel(e->d);
    } else if(e->type==EV_SCROLL)input_wheel(e->a);
}
void app_main(void)
{
    sc(S_INFO, &sys, 0, 0, 0);
    initgfx(sys.w, sys.h);
    lite = sys.opaque;
    background(sys.wall, sys.theme, sys.dark);
    mx = W - 92;
    my = H - 131;
    lightx = mx;
    lighty = my;
    u64 last = sys.ticks;
    int first = 1;
    login_enter();
    for (;;) {
        SysInfo s;
        sc(S_INFO, &s, 0, 0, 0);
        if (s.w != sys.w || s.h != sys.h) {
            // Runtime resolution change: rebuild the canvas at the new size
            // (fonts stay fixed px), keep windows and the cursor on screen.
#ifdef TMC_DEBUG
            {
                char nb[16], tb[48];
                tb[0] = 0;
                append(tb, "[resize] ", 48);
                number(nb, s.w);
                append(tb, nb, 48);
                append(tb, "x", 48);
                number(nb, s.h);
                append(tb, nb, 48);
                trace(tb);
            }
#endif
            initgfx((int)s.w, (int)s.h);
            pointer_scale=(PointerScale){0};
            background(sys.wall, sys.theme, sys.dark);
            for (int i = 0; i < nz; i++) {
                View *v = &views[order[i]];
                Rect *b = &v->box;
                /* A lower scanout must shrink oversized clients, not merely
                 * move their still-too-large rectangles to (0,36). */
                int resized=0;
                if(b->w>W-28){b->w=W-28;resized=1;}
                if(b->h>H-BAR-100){b->h=H-BAR-100;resized=1;}
                if(resized)resize(order[i]);
                if (b->x + b->w > W)b->x = W - b->w;
                if (b->y + b->h > H)b->y = H - b->h;
                if (b->x < 0)b->x = 0;
                if (b->y < 36)b->y = 36;   // never cover the 36px menu bar
                dropbuf(&v->surface);      // stale captures must not replay
                dropbuf(&v->previous);
                v->sw = v->sh = 0;
                viewtrace(v->s.app, *b);
            }
            if (mx >= W)mx = W - 1;
            if (my >= H)my = H - 1;
            lightx = mx;
            lighty = my;
            wallpaper_age = 32;
            wallEpoch++;
            dirty = 1;
        }
        int dt = MIN(7ull, s.ticks - last);
        last = s.ticks;
        if (s.theme != sys.theme || s.wall != sys.wall || s.dark != sys.dark) {
            memcpy(prevwall, back, (u64)W * H * 4);
            background(s.wall, s.theme, s.dark);
            wallpaper_age = s.reduce_motion ? 32 : 0;
            wallEpoch++;
            dirty = 1;
        }
        if (s.opaque != sys.opaque || s.reduce_motion != sys.reduce_motion || s.dock != sys.dock) {
            wallEpoch++;
            dirty = 1;
        }
        if (s.minute != sys.minute || s.fsrev!=sys.fsrev)dirty = 1;
        /* Repair scanout even while idle: cached tiles are not the framebuffer. */
        if(s.ticks-scanout_heartbeat>=100){scanout_heartbeat=s.ticks;dirty=1;}
        sys = s;
        lite = sys.opaque;
        if (!gate)wins();
        Event e;
        while (sc(S_INPUT, &e, 0, 0, 0) > 0) {
            if (e.type == EV_MOUSE) {
                e.a=pointer_scale_axis(e.a,W,PW,&pointer_scale.x);
                e.b=pointer_scale_axis(e.b,H,PH,&pointer_scale.y);
            } else if (e.type == EV_POINTER_ABS && (PW != W || PH != H)) {
                e.a=(int)((u64)(u32)e.a*(W-1)/(PW-1));
                e.b=(int)((u64)(u32)e.b*(H-1)/(PH-1));
            }
            /* EV_SCROLL.a is a wheel count, NOT a pixel coordinate. */
            if (gate)login_input(&e);
            else input(&e);
        }
        memset(&e, 0, sizeof e);
        while(sc(S_EVENT,&e,0,0,0)>0)if(!gate&&e.type==EV_CONTEXT_READY)context_ready(&e);
        if (gate) {
            if (login_step()) {
                cursorValid = 0;
                wallEpoch++;
                if (first) {
                    first = 0;
                    openarg(FINDER, HOME);
                }
                continue;
            }
            if (dirty) {
                dirty = 0;
                cursorValid = 0;
                login_draw();
                drawcursor();
                sc(S_PRESENT, canvas, 0, 0, ((u64)(u32)W << 32) | (u32)H);
                cursorDirty = 0;
            } else if (cursorDirty) {
                int X=curx,Y=cury,ww=curw,hh=curh;
                undocursor(); drawcursor();
                scanrect(X,Y,ww,hh); scanrect(curx,cury,curw,curh);
                cursorDirty=0;
            }
            slp(1);
            continue;
        }
        if (shell_step(dt))dirty = 1;
        if (dirty)render(dt);
        else if (cursorDirty) {
            int X = curx, Y = cury, ww = curw, hh = curh;
            undocursor();
            drawcursor();
            scanrect(X, Y, ww, hh);
            scanrect(curx, cury, curw, curh);
            cursorDirty = 0;
        }
        slp(1);
    }
}
