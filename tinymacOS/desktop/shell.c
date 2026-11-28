#include "text_input.h"
#include "desk.h"
#include "control.h"
#include "date.h"
#include "caret.h"
#include "desktop_layout.h"
#include "modifier_symbols.h"
#define NS 9
static const int pins[NS] = {FINDER, 0, CALENDAR, GALLERY, TEXTEDIT, CALC, TERMINAL, MONITOR, SETTINGS};
static int mag[NS], hover = -1, oldHover = -2, tipA, bounceA, bounceTime, dockAge = 28, dockLast = -1;
static int ov, want, age = 24, menuId, pick, popFrames, bootAge;
static Rect panel, barhit[7], srch, ctrl, widget;
static char query[48];
static int cmx,cmy,owner,pending,nonce,cmn,codes[16],deskSel=-2,lastHit=-2,keyboardPick=-1;
static unsigned deskRev=~0u;static u64 deskTick;static int ndesk;static Dirent ondesk[NSLOTS];
static const char *cmtext[16];
static void cmadd(const char*s,int a){if(cmn<16){cmtext[cmn]=s;codes[cmn++]=a;}}
static DesktopLayout desktopLayout;
static int layoutLoaded,desktopFocus,queryAt,querySelected,folderAt,folderSelected;
static int fieldPhase=-1;static u64 fieldTick,pendingUntil;
static char folderName[48],folderError[100],pendingName[48];static int pendingX,pendingY;
static const char *layoutPath=HOME "/Library/Preferences/Desktop.layout";
static int dmax(void){return MAX(1,(H-314)/91);}
static int dcount(void){return ndesk+!!sys.live_cd;}
static const char *position_name(int slot){int i=slot-!!sys.live_cd;return i<0?"@installer":ondesk[i].name;}
static u64 position_id(int slot){int i=slot-!!sys.live_cd;return i<0?~0ull:ondesk[i].id;}
static void layout_load(void){
    desktop_layout_init(&desktopLayout);int fd=sc(S_OPEN,layoutPath,0,0,0);
    if(fd>=0){int at=0;while(at<sizeof desktopLayout){int n=sc(S_READ,fd,(u8*)&desktopLayout+at,MIN(MAXFILE,sizeof desktopLayout-at),0);if(n<=0)break;at+=n;}sc(S_CLOSE,fd,0,0,0);
        if(at!=sizeof desktopLayout||!desktop_layout_valid(&desktopLayout))desktop_layout_init(&desktopLayout);}
    layoutLoaded=1;
}
static int layout_save(void){
    sc(S_CREATE,HOME "/Library",1,0,0);sc(S_CREATE,HOME "/Library/Preferences",1,0,0);
    int fd=sc(S_OPEN,layoutPath,1,0,0);
    if(fd<0){if(sc(S_CREATE,layoutPath,0,0,0)<0)return 0;fd=sc(S_OPEN,layoutPath,1,0,0);}if(fd<0)return 0;
    int at=0;while(at<sizeof desktopLayout){int n=MIN(MAXFILE,sizeof desktopLayout-at);if(sc(S_WRITE,fd,(u8*)&desktopLayout+at,n,0)!=n)break;at+=n;}sc(S_CLOSE,fd,0,0,0);return at==sizeof desktopLayout;
}
static Rect deskcell(int slot){
    int cap=dmax()*MAX(1,(W-150)/128),i=slot%cap;
    Rect fallback={W-135-(i/dmax())*128,220+(i%dmax())*91,122,83};
    int p=desktop_position_find(&desktopLayout,position_id(slot),position_name(slot),0);
    return desktop_position_box(p<0?0:desktopLayout.items+p,fallback,W,H);
}
static void position_set(int slot,int x,int y){
    int p=desktop_position_find(&desktopLayout,position_id(slot),position_name(slot),1);if(p<0)return;
    Rect q=desktop_position_box(0,(Rect){x,y,122,83},W,H);DesktopPosition*v=desktopLayout.items+p;
    v->x=q.x;v->y=q.y;v->id=position_id(slot);strset(v->name,position_name(slot),48);
}
int shell_desktop_active(void){return desktopFocus||topwin()<0;}
void shell_window_focus(void){desktopFocus=0;}
void shell_drop_position(const char *path,int x,int y){
    const char *base=path;for(const char*p=path;*p;p++)if(*p=='/')base=p+1;
    strset(pendingName,base,sizeof pendingName);pendingX=x-61;pendingY=y-25;pendingUntil=sys.ticks+1000;
}
void shell_new_folder(void){
    strset(folderName,"Nuova cartella",sizeof folderName);folderError[0]=0;folderAt=strlen(folderName);folderSelected=1;fieldTick=sys.ticks;
    overlay_show(OV_NEW_FOLDER);
}

static int entryid(int slot){return slot-(int)sys.live_cd;}
static int deskhit(int x,int y){for(int k=dcount();k--;)if(inside(x,y,deskcell(k)))return entryid(k);return -2;}
static void deskpath(char *s,int i){strset(s,HOME "/Desktop",PATHLEN);if(i>=0&&i<ndesk){append(s,"/",PATHLEN);append(s,ondesk[i].name,PATHLEN);}}
static void reveal(char cmd,int i){char path[PATHLEN],s[PATHLEN]={'@',cmd,':',0};deskpath(path,i);if(strlen(path)+3>=PATHLEN)return;append(s,path,PATHLEN);openarg(FINDER,s);}
static void folder_confirm(void){
    if(!folderName[0]||eq(folderName,".")||eq(folderName,"..")||strchr(folderName,'/')){strset(folderError,"Scegli un nome valido senza /.",sizeof folderError);return;}
    char path[PATHLEN];strset(path,HOME "/Desktop/",PATHLEN);append(path,folderName,PATHLEN);
    if(sc(S_CREATE,path,1,0,0)<0){strset(folderError,"Cartella non creata: nome già presente o permesso negato.",sizeof folderError);return;}
    shell_drop_position(path,cmx,cmy);overlay_show(0);dirty=1;
}
int shell_desktop_drop(const char *path,int x,int y,int dx,int dy){
    int slot=-1;char candidate[PATHLEN];
    for(int k=0;k<dcount();k++){
        int i=k-!!sys.live_cd;
        if(i<0){if(eq(path,"@installer"))slot=k;}
        else{deskpath(candidate,i);if(eq(path,candidate))slot=k;}
    }
    if(slot<0)return 0;
    int target=deskhit(x,y);
    if(target>=0&&ondesk[target].dir&&slot-!!sys.live_cd!=target){
        if(eq(path,"@installer"))return 1;
        char dest[PATHLEN];deskpath(dest,target);append(dest,"/",PATHLEN);append(dest,position_name(slot),PATHLEN);
        if(sc(S_MOVE,path,dest,0,0)<0){strset(folderError,"Elemento non spostato: controlla nome e permessi.",sizeof folderError);}dirty=1;return 1;
    }
    Rect q=deskcell(slot);position_set(slot,q.x+dx,q.y+dy);layout_save();lastHit=-2;desktopFocus=1;dirty=1;return 1;
}
static void deskopen(int i){if(i==-1)launch(INSTALLER);else if(i>=0&&i<ndesk){char p[PATHLEN];deskpath(p,i);openarg(ondesk[i].dir?FINDER:TEXTEDIT,p);}}

typedef struct {
    int a, dir, ico;
    char label[56], path[PATHLEN];
} Hit;
static Hit results[16];
static int count;
static const char *menun[] = {"", "", "File", "Modifica", "Vista", "Finestra", "Aiuto"};

static int scaleval(int t, int max)
{
    return ease(clampi(t * 256 / max, 0, 256));
}
static void white(int x, int y, const char *s, int f)
{
    text(x, y, s, f, 0xf9fbff);
}
static int match(const char *a, const char *b)
{
    if (!*b)return 1;
    for (; *a; a++) {
        int i = 0;
        for (; b[i] && a[i]; i++) {
            char A = a[i], B = b[i];
            if (A >= 'A' && A <= 'Z')A += 32;
            if (B >= 'A' && B <= 'Z')B += 32;
            if (A != B)break;
        }
        if (!b[i])return 1;
    }
    return 0;
}
static void hits(void)
{
    count = 0;
    for (int a = 1; a <= lastapp(sys.live_cd); a++) {
        if (a == INSTALLER && !sys.live_cd)continue;
        if (match(appname(a), query) || match(bundlename(a), query)) {
            Hit *h = results + count++;
            h->a = a;
            h->ico = appicon(a);
            strset(h->label, appname(a), 56);
        }
    }
    if (query[0]) {
        Dirent ds[NSLOTS];
        int n = sc(S_LIST, DOCS, ds, NSLOTS, 0);
        for (int i = 0; i < n && count < 15; i++)if (match(ds[i].name, query)) {
                Hit *h = results + count++;
                h->a = 0;
                h->dir = ds[i].dir;
                h->ico = h->dir ? 6 : 7;
                strset(h->label, ds[i].name, 56);
                strset(h->path, DOCS "/", PATHLEN);
                append(h->path, ds[i].name, PATHLEN);
            }
    }
    pick = 0;
}
void overlay_show(int n)
{
    control_cancel();
    if (n == OV_NONE) {
        if (ov && want) {
            want = 0;
            dirty = 1;
        }
        return;
    }
    ov = n;
    want = 1;
    age = sys.reduce_motion ? 24 : 0;
    popFrames = 0;
    pick = 0;
    if (n == OV_SEARCH) {
        query[0] = 0;queryAt=querySelected=0;fieldTick=sys.ticks;
        hits();
    }
    dirty = 1;

}
static void choose(int i)
{
    if (i < 0 || i >= count)return;
    Hit *h = results + i;
    overlay_show(0);
    if (h->a)launch(h->a);
    else openarg(h->dir ? FINDER : TEXTEDIT, h->path);
}
static int idx(int a)
{
    for (int i = 0; i < NS; i++)if (pins[i] == a)return i;
    return 1;
}
static int pitch(void)
{
    return sys.dock && H < 700 ? 49 : 61;
}
static int targetVert(void)
{
    return dockAge < 14 ? !sys.dock : sys.dock;
}
static void docklayout(Rect *bar, Rect *items)
{
    int vert = targetVert(), p = pitch(), base = p - 12, full = 18;
    int sz[NS];
    for (int k = 0; k < NS; k++) {
        sz[k] = base + (mag[k] * 18 >> 8);
        full += sz[k] + 10;
        if (k == 5)full += 9;
    }
    int along = (vert ? H : W) / 2 - full / 2;
    int off = (bootAge < 35 ? (256 - scaleval(bootAge, 35)) * 34 / 256 : 0);
    if (dockAge < 28)off += (14 - (dockAge > 14 ? dockAge - 14 : 14 - dockAge)) * 101 / 14;
    *bar = vert ? (Rect) {
        10 - off, along, 68, full
}:
    (Rect) {
        along, H - 84 + off, full, 69
    };
    int t = along + 13;
    for (int k = 0; k < NS; k++) {
        if (k == 6)t += 9;
        int size = sz[k], lift = (mag[k] * 6 >> 8);
        if (pins[k] == bounceA && bounceTime > 0) {
            int T = 30 - bounceTime;
            int half = T % 15;
            lift += half * (15 - half) * (T < 15 ? 14 : 6) / 56;
        }
        items[k] = vert ? (Rect) {
            bar->x + 9 + lift, t, size, size
}:
        (Rect) {
            t, bar->y + 54 - size - lift, size, size
        };
        t += size + 10;
    }
}
void dockcenter(int a, int *x, int *y)
{
    Rect q, items[NS];
    docklayout(&q, items);
    Rect t = items[idx(a)];
    *x = t.x + t.w / 2;
    *y = t.y + t.h / 2;
}
static int dockhit(int x, int y)
{
    Rect q, items[NS];
    docklayout(&q, items);
    for (int k = 0; k < NS; k++) {
        Rect t = items[k];
        t.x -= 4;
        t.y -= 5;
        t.w += 8;
        t.h += 11;
        if (inside(x, y, t))return k;
    }
    return -1;
}
void shell_pop(int a)
{
    bounceA = a;
    bounceTime = sys.reduce_motion ? 0 : 30;
}
void shell_control_pointer(int down)
{
    if(ov==OV_CONTROL&&want)control_pointer(panel,down);
    else control_cancel();
}
static Rect libcell(Rect,int);
static int libcount(void);
void shell_mouse(void)
{
    static int libraryHover=-2, lightBucket=-1;
    hover = dockhit(mx, my);
    if (ov == OV_LIBRARY && want) {
        int hit=-1;
        for(int j=0;j<libcount();j++)if(inside(mx,my,libcell(panel,j)))hit=j;
        /* The material light depends on x, not y. Keep exact lighting. */
        int bucket=mx;
        if(hit!=libraryHover||bucket!=lightBucket)dirty=1;
        libraryHover=hit;lightBucket=bucket;
    } else if (ov && want)dirty = 1;
}
int shell_step(int dt)
{
    int changed = 0;
    if(ov&&want&&(ov==OV_SEARCH||ov==OV_NEW_FOLDER)){int phase=caret_phase(sys.ticks,fieldTick,sys.reduce_motion);if(phase!=fieldPhase){fieldPhase=phase;changed=1;}}
    if (dockLast < 0)dockLast = sys.dock;
    if (dockLast != (int)sys.dock) {
        dockLast = sys.dock;
        dockAge = sys.reduce_motion ? 28 : 0;
        changed = 1;
    }
    if (dockAge < 28) {
        dockAge = MIN(28, dockAge + dt);
        changed = 1;
    }
    if (bootAge < 35) {
        bootAge = sys.reduce_motion ? 35 : MIN(35, bootAge + dt);
        changed = 1;
    }
    Rect bar, items[NS];
    docklayout(&bar, items);
    int vertical = targetVert();
    int insideDock = vertical ? (mx < bar.x + bar.w + 24 && my >= bar.y - 10 &&
                                 my <= bar.y + bar.h + 10) : (my > bar.y - 22 && mx >= bar.x - 10 && mx <= bar.x + bar.w + 10);
    hover = ov && want ? -1 : dockhit(mx, my);
    int step = MAX(1, dt) * 24;
    for (int k = 0; k < NS; k++) {
        int dist = vertical ? my - (items[k].y + items[k].h / 2) : mx - (items[k].x + items[k].w / 2);
        if (dist < 0)dist = -dist;
        int target = (insideDock && !want && !sys.reduce_motion) ? MAX(0, 256 - dist * 3) : 0;
        int next = mag[k] + clampi(target - mag[k], -step, step);
        if (next != mag[k]) {
            mag[k] = next;
            changed = 1;
        }
    }
    if (oldHover != hover) {
        oldHover = hover;
        tipA = 0;
        changed = 1;
    }
    if (hover >= 0 && tipA < 16) {
        tipA = sys.reduce_motion ? 16 : MIN(16, tipA + dt);
        changed = 1;
    }
    if (bounceTime) {
        bounceTime = MAX(0, bounceTime - dt);
        changed = 1;
    }
    if (ov) {
        if (want && age < 24) {
            age = sys.reduce_motion ? 24 : MIN(24, age + dt);
            changed = 1;
        }
        if (!want) {
            age = sys.reduce_motion ? 0 : MAX(0, age - dt * 2);
            changed = 1;
            if (!age) {
                ov = 0;
            }
        }
    }
    return changed;
}
static void clockstr(char *out)
{
    static const char *const wd[] = {"dom", "lun", "mar", "mer", "gio", "ven", "sab"};
    char n[24];
    int m = clampi(sys.month, 1, 12);
    strset(out, wd[weekday(sys.year, m, sys.day)], 48);
    append(out, " ", 48);
    number(n, sys.day);
    append(out, n, 48);
    append(out, " ", 48);
    char mo[4] = {monthname(m)[0] + 32, monthname(m)[1], monthname(m)[2], 0};
    append(out, mo, 48);
    append(out, "   ", 48);
    if (sys.hour < 10)append(out, "0", 48);
    number(n, sys.hour);
    append(out, n, 48);
    append(out, ":", 48);
    if (sys.minute < 10)append(out, "0", 48);
    number(n, sys.minute);
    append(out, n, 48);
}
// Reuse the actual refracted pixels, not an unrelated pre-blurred wallpaper.
// Panel geometry, sampled scene generation and nearby pointer light key the cache.
typedef struct {
    u64 key;
    Rect q;
    u32 *bits;
    int cap;
} Tile;
static u32 barbits[1920 * 36], dockbits[110 * 1120], widgetbits[303 * 202];
static Tile tiles[] = {{0, {0}, barbits, 1920 * 36}, {0, {0}, dockbits, 110 * 1120}, {0, {0}, widgetbits, 303 * 202}};
static Rect bounded(Rect b)
{
    int r = MIN(W, b.x + b.w), d = MIN(H, b.y + b.h);
    b.x = MAX(b.x, 0);
    b.y = MAX(b.y, 0);
    b.w = MAX(0, r - b.x);
    b.h = MAX(0, d - b.y);
    return b;
}
static u64 tilekey(Rect q, Rect b, int extra)
{
    int near = mx >= q.x - 70 && mx <= q.x + q.w + 70 && my >= q.y - 65 && my <= q.y + q.h + 65;
    u64 h = backdrop_key(b) + 5;
    h = h * 131 + (q.x + 128);
    h = h * 131 + (q.y + 128);
    h = h * 131 + q.w;
    h = h * 131 + q.h;
    h = h * 131 + extra;
    return (h * 257 + (near ? 1 + mx / 24 : 0)) | 1ull << 63;
}
static int reuse(Tile *t, Rect b, u64 key)
{
    if (b.w * b.h <= t->cap && t->key == key && t->q.x == b.x && t->q.y == b.y && t->q.w == b.w && t->q.h == b.h) {
        copyin(b, t->bits);
        return 1;
    }
    return 0;
}
static void remember(Tile *t, Rect b, u64 key)
{
    if (b.w * b.h <= t->cap) {
        copyout(b, t->bits);
        t->q = b;
        t->key = key;
    }
}
static void chrome(int id, Rect q, int r, int kind)
{
    /* A Dock tile with shadow padding also contains raw window pixels.
     * Replaying that rectangle can overwrite a moving/scaled window with an
     * earlier scene. Composite the small live material/shadow, never its
     * rectangular captured backdrop (also correct for a side-positioned Dock). */
    if(id==1){shadow(q,r,52);glass(q,r,kind);return;}
    int pad = id ? 18 : 0;
    Rect b = bounded((Rect) {
        q.x - pad, q.y - pad, q.w + 2 * pad, q.h + 2 * pad + (id ? 5 : 0)
    });
    Tile *t = tiles + id;
    u64 h = tilekey(q, b, 0);
    if (reuse(t, b, h))return;
    if (id)shadow(q, r, 52);
    glass(q, r, kind);
    remember(t, b, h);
}
void shell_reset(void)
{
    pending=0;deskSel=-2;lastHit=-2;ov = want = 0;layoutLoaded=0;deskRev=~0u;desktopFocus=0;pendingName[0]=0;
    query[0] = 0;
    pick = 0;
    hover = -1;
    dirty = 1;
}
static void menubar(void)
{
    chrome(0, (Rect) {
        0, 0, W, BAR
    }, 0, MAT_BAR);
    int x = 9, i = topwin();
    for (int k = 0; k < 7; k++) {
        const char *s = k == 1 ? (i < 0 ? "Finder" : appname(views[i].s.app)) : menun[k];
        int f = k < 2 ? F_BOLD : F_BODY, ex = k == 0 ? 16 : 0;
        int wid = textw(s, f) + 18 + ex;
        barhit[k] = (Rect) {
            x, 1, wid, BAR - 2
        };
        if (ov == OV_MENU && want && menuId == k)roundr(x, 3, wid, 27, 8, 0xffffff, 54);
        if (k == 0)icon(x + 3, 3, 24, 31);
        white(x + 8 + ex, 7, s, f);
        x += wid;
    }
    char clk[48];
    clockstr(clk);
    int cx = W - 18 - textw(clk, F_BODY);
    white(cx, 7, clk, F_BODY);
    srch = (Rect) {
        cx - 79, 2, 31, 30
    };
    ctrl = (Rect) {
        cx - 45, 2, 31, 30
    };
    if (ov == OV_SEARCH || ov == OV_CONTROL) {
        Rect a = ov == OV_SEARCH ? srch : ctrl;
        roundr(a.x, a.y, a.w, a.h, 9, 0xffffff, 51);
    }
    icon(srch.x + 6, 8, 18, 21);
    icon(ctrl.x + 5, 7, 21, 22);
}
static void dock(void)
{
    Rect q, it[NS];
    docklayout(&q, it);
    chrome(1, q, 23, MAT_DOCK);
    int vert = targetVert();
    for (int k = 0; k < NS; k++) {
        Rect b = it[k];
        if (k == 6) {
            int x = b.x - 9, y = b.y - 8;
            if (vert)roundr(q.x + 12, y, 42, 1, 0, 0xe5edff, 124);
            else roundr(x, q.y + 12, 1, 42, 0, 0xe5edff, 124);
        }
        icon(b.x, b.y, b.w, pins[k] ? appicon(pins[k]) : 20);
        for (int j = 0; j < nz; j++)if (!views[order[j]].dead && views[order[j]].s.app == pins[k]) {
                if (vert)roundr(q.x + 6, b.y + b.h / 2 - 3, 3, 6, 2, 0xe8efff, 255);
                else roundr(b.x + b.w / 2 - 2, q.y + 58, 5, 4, 2, 0xf0f5ff, 255);
                break;
            }
        if (hover == k && tipA > 3 && !want) {
            const char *name = pins[k] ? appname(pins[k]) : "Applicazioni";
            int tw = textw(name, F_SMALL) + 22;
            Rect tt = vert ? (Rect) {
                q.x + q.w + 9, b.y + b.h / 2 - 13, tw, 27
}:
            (Rect) {
                b.x + b.w / 2 - tw / 2, MIN(q.y - 33, b.y - 33) + (16 - tipA) / 3, tw, 27
            };
            roundr(tt.x, tt.y, tt.w, tt.h, 11, 0x1e2a45, MIN(245, tipA * 16));
            center(tt.x + tt.w / 2, tt.y + 4, name, F_SMALL, 0xf6f8ff);
        }
    }
}
static void desktop_icons(void)
{
    for(int k=0;k<dcount();k++){
        int i=entryid(k);Rect q=deskcell(k);const char *name=i<0?INSTALLER_TITLE:ondesk[i].name;
        icon(q.x+36,q.y+2,50,i<0?38:ondesk[i].dir?6:7);
        if(i<0){
            roundr(q.x,q.y+54,q.w,29,7,i==deskSel?0x146be3:0x181a24,i==deskSel?245:136);
            center(q.x+q.w/2,q.y+55,"Installa",F_MICRO,0xffffff);
            center(q.x+q.w/2,q.y+68,"tinymacOS Tramontana",F_MICRO,0xffffff);
            continue;
        }
        char label[24];strset(label,name,20);
        if(strlen(name)>=20){label[16]='.';label[17]='.';label[18]=0;}
        int nw=MIN(124,textw(label,F_SMALL)+12);roundr(q.x+(q.w-nw)/2,q.y+56,nw,23,7,i==deskSel?0x146be3:0x181a24,i==deskSel?245:136);center(q.x+q.w/2,q.y+59,label,F_SMALL,0xffffff);
    }
}
void shell_base(void)
{
    if(!layoutLoaded)layout_load();
    if(deskRev!=sys.fsrev){ndesk=MAX(0,sc(S_LIST,HOME "/Desktop",ondesk,NSLOTS,0));deskRev=sys.fsrev;deskSel=-2;
        for(int j=0;j<DESKTOP_POSITIONS;j++)if(desktopLayout.items[j].used){int found=0;for(int k=0;k<dcount();k++)if(desktopLayout.items[j].id==position_id(k)||eq(desktopLayout.items[j].name,position_name(k))){found=1;break;}if(!found)desktopLayout.items[j].used=0;}
    }
    if(pendingName[0]){for(int k=0;k<dcount();k++)if(eq(position_name(k),pendingName)){position_set(k,pendingX,pendingY);layout_save();pendingName[0]=0;break;}if(sys.ticks>pendingUntil)pendingName[0]=0;}
    widget = (Rect) {
        W - 291, BAR + 32, 265, 143
    };
    if (W >= 990) {
        Rect b = bounded((Rect) {
            widget.x - 17, widget.y - 16, widget.w + 34, widget.h + 38
        });
        u64 key = tilekey(widget, b, sys.day + (sys.month + sys.year * 12) * 32);
        if (reuse(tiles + 2, b, key)){desktop_icons();return;}
        shadow(widget, 21, 41);
        glass(widget, 21, MAT_WIDGET);
        int x = widget.x, y = widget.y, m = clampi(sys.month, 1, 12), year = sys.year, day = MAX(1, sys.day),
            wd = weekday(year, m, day);
        char n[24];
        white(x + 16, y + 12, dayname(wd), F_SMALL);
        number(n, day);
        white(x + 13, y + 27, n, F_HERO);
        white(x + 17, y + 109, monthname(m), F_BOLD);
        int first = (weekday(year, m, 1) + 6) % 7;
        for (int j = 0; j < 7; j++) {
            const char *s[] = {"L", "M", "M", "G", "V", "S", "D"};
            center(x + 121 + j * 20, y + 16, s[j], F_MICRO, 0xc0d5f1);
        }
        for (int j = 1; j <= monthdays(year, m); j++) {
            int p = first + j - 1, X = x + 121 + (p % 7) * 20, Y = y + 38 + (p / 7) * 16;
            if (j == day)roundr(X - 7, Y - 1, 15, 15, 7, 0xc9ddff, 239);
            number(n, j);
            center(X, Y - 1, n, F_MICRO, j == day ? 0x1d3560 : 0xe9f1ff);
        }
        remember(tiles + 2, b, key);
    } else widget = (Rect) {
        0, 0, 0, 0
    };
    desktop_icons();
}
static int menus(int k, const char **s, int *a)
{
    int n = 0, i = topwin(), app = i < 0 ? 0 : views[i].s.app;
#define ADD(t,v) do{s[n]=t;a[n++]=v;}while(0)
    if(k==7){for(int j=0;j<cmn;j++)ADD(cmtext[j],codes[j]);return n;}
    if (k == 0) {
        ADD("Info su tinymacOS", 105);
        ADD("Impostazioni di Sistema", 104);
        ADD("Applicazioni", 161);
        ADD("Utility Disco…",100+DISKUTIL);
        ADD("Blocca schermo", 152);
        ADD("Riavvia…", 150);
        ADD("Spegni…", 151);
    } else if (k == 1) {
        ADD("Cerca un’app…", 162);
        if (app) {
            ADD("Riduci a icona", 141);
            ADD("Chiudi finestra", 142);
        }
    } else if (k == 2) {
        if (app == FINDER) {
            ADD("Nuova cartella…", ACT_NEW);
            ADD("Apri selezione", ACT_OPEN);
            ADD("Ottieni informazioni", ACT_PROPS);
            ADD("Mostra KernelCollections", ACT_SYSTEM);
        } else if(app==NOTES||app==STICKIES||app==TERMINAL){ADD(app==NOTES?"Nuova nota":app==STICKIES?"Nuovo memo":"Nuova finestra",ACT_NEW);if(app!=TERMINAL){ADD("Salva",ACT_SAVE);ADD("Elimina…",ACT_DELETE);}}
        else if (app == TEXTEDIT) {
            ADD("Nuovo documento", ACT_NEW);
            ADD("Apri…", ACT_OPEN);
            ADD("Salva", ACT_SAVE);
        } else {
            ADD("Apri Finder", 101);
            ADD("Apri TextEdit", 103);
        }
    } else if (k == 3) {
        if (app == FINDER || app == TEXTEDIT || app == TERMINAL) {
            if(app!=TEXTEDIT)ADD("Taglia",ACT_CUT);
            ADD("Copia", ACT_COPY);
            ADD("Incolla", ACT_PASTE);
            if (app == FINDER) {
                ADD("Rinomina…", ACT_RENAME);
                ADD("Elimina…", ACT_DELETE);
            }
        } else ADD("Apri TextEdit", 103);
    } else if (k == 4) {
        if (app == FINDER) {
            ADD("Vista rapida", ACT_PREVIEW);
            ADD("Come icone / Come elenco", ACT_VIEW);
        }
        ADD("Centro di Controllo", 163);
    } else if (k == 5) {
        ADD("Mission Control", 164);
        if (app) {
            ADD("Riduci a icona", 141);
            ADD("Zoom", 140);
            ADD("Chiudi", 142);
        }
    } else {
        ADD("Guida alle scorciatoie", 145);
        ADD("Info su tinymacOS", 105);
    }
    return n;
}
static Rect targetpanel(void)
{
    if (ov == OV_MENU) {
        const char *l[16];
        int a[16];
        if(menuId==7){int h=cmn*31+12;return (Rect){clampi(cmx,3,W-228),clampi(cmy,BAR+2,H-h-4),225,h};}
        return (Rect) {
            barhit[menuId].x, BAR + 5, 209, menus(menuId, l, a) * 31 + 12
        };
    }
    if (ov == OV_CONTROL)return control_bounds(W,H);
    if(ov==OV_HELP)return (Rect){(W-560)/2,MAX(BAR+6,(H-422)/2-15),560,422};
    int w = ov == OV_LIBRARY ? 688 : ov == OV_SEARCH ? 510 : 434,
        h = ov == OV_SEARCH ? 94 + MIN(6, MAX(1, count)) * 46 : ov == OV_LIBRARY ? 469 : ov==OV_NEW_FOLDER?212:188;
    return (Rect) {
        (W - w) / 2, ov == OV_SEARCH ? H / 5 : (H - h) / 2 - 15, w, h
    };
}
static Rect libcell(Rect p, int j)
{
    return (Rect) {
        p.x + 22 + (j % 4) * 162, p.y + 83 + (j / 4) * 94, 156, 87
    };
}
/* App shown at library cell j. On installed volumes the installer cell is
   skipped (live-only app), so 14 live cells become 13 installed cells. */
static int libapp(int j)
{
    return !sys.live_cd && j >= INSTALLER - 1 ? j + 2 : j + 1;
}
static int libcount(void)
{
    return lastapp(sys.live_cd) - (sys.live_cd ? 0 : 1);
}
static void itemchoice(int a)
{
    if(ov==OV_MENU && menuId==7){
        int dest=topwin();overlay_show(0);
        if(owner && (dest<0||views[dest].s.pid!=owner))return;
        if(a==201){openarg(FINDER,HOME "/Desktop");return;}if(a==202){shell_new_folder();return;}
        if(a==203){deskopen(deskSel);return;}if(a>=204&&a<=206){reveal(a==204?'i':a==205?'r':'d',deskSel);return;}
    }else overlay_show(0);
    if (a == 164)mc_enter();
    else if (a >= 161 && a <= 163) {
        int map[] = {OV_LIBRARY, OV_SEARCH, OV_CONTROL};
        overlay_show(map[a - 161]);
    } else action(a);
}
static void paintoverlay(void)
{
    if (!ov)return;
    int t = scaleval(age, 24);
    Rect q = targetpanel();
    q.y += (256 - t) * (ov == OV_CONTROL || ov == OV_MENU ? -13 : 23) / 256;
    panel = q;
    popFrames++;
    /* App Library is a floating glass panel, not a modal dimming sheet.
     * Do not bake its own cast shadow into the refracted backdrop. */
    if(ov==OV_CONTROL){control_draw(q);return;}
    if(ov!=OV_LIBRARY)shadow(q, 20, 54);
    glass(q, ov == OV_MENU ? 13 : 22, ov==OV_LIBRARY?MAT_LIBRARY:ov==OV_MENU?MAT_MENU:MAT_SHEET);
    int x = q.x, y = q.y;
    if(ov==OV_HELP){shortcuts_help(q);return;}
    if(ov==OV_NEW_FOLDER){
        text(x+22,y+17,"Nuova cartella",F_BOLD,TX);roundr(x+20,y+55,q.w-40,36,7,INK(I_CARD),255);
        if(folderSelected)roundr(x+29,y+63,MIN(q.w-61,text_columns(folderName,strlen(folderName))*8),18,2,INK(I_SEL),180);
        inputline((Rect){x+30,y+64,q.w-60,18},folderName,folderAt,caret_phase(sys.ticks,fieldTick,sys.reduce_motion),TX);
        if(folderError[0])text(x+22,y+106,folderError,F_SMALL,SUB);
        button_face((Rect){x+q.w-210,y+q.h-47,85,28},0);center(x+q.w-168,y+q.h-41,"Annulla",F_SMALL,TX);
        button_face((Rect){x+q.w-111,y+q.h-47,85,28},1);center(x+q.w-69,y+q.h-41,"Crea",F_SMALL,TX);return;
    }
    if (ov == OV_MENU) {
        const char *l[16];
        int a[16], n = menus(menuId, l, a);
        for (int k = 0; k < n; k++) {
            Rect r = {x + 5, y + 6 + 31 * k, q.w - 10, 29};
            if (inside(mx, my, r)||(menuId==7 && keyboardPick==k))roundr(r.x, r.y, r.w, r.h, 7, INK(I_SEL), 244);
            text(x + 14, r.y + 5, l[k], F_BODY, TX);
        }
        return;
    }
    if (ov == OV_SEARCH) {
        icon(x + 19, y + 26, 27, 20);
        if(!query[0])text(x+59,y+23,"Ricerca Spotlight",F_TITLE,SUB);
        proportional_input((Rect){x+59,y+26,q.w-135,18},query,queryAt,caret_phase(sys.ticks,fieldTick,sys.reduce_motion),TX);
        roundr(x + q.w - 61, y + 26, 39, 24, 8, INK(I_CARD), 193);
        center(x + q.w - 41, y + 32, "ESC", F_MICRO, SUB);
        rect(x + 16, y + 72, q.w - 32, 1, LN);
        int first = MAX(0, pick - 5);
        if (!count)text(x + 22, y + 95, "Nessun risultato.", F_BODY, SUB);
        for (int j = first; j < count && j < first + 6; j++) {
            int yy = y + 81 + (j - first) * 46;
            if (j == pick)roundr(x + 10, yy, q.w - 20, 43, 10, INK(I_SEL), 222);
            icon(x + 19, yy + 4, 35, results[j].ico);
            text(x + 63, yy + 2, results[j].label, F_BOLD, TX);
            text(x + 63, yy + 22, results[j].a ? "Applicazione" : "Documenti", F_SMALL, SUB);
            if (j == pick)text(x + q.w - 49, yy + 14, "Apri", F_SMALL, INK(I_ACC_TXT));
        }
        return;
    }
    if (ov == OV_LIBRARY) {
        text(x + 25, y + 11, "Applicazioni", F_SMALL, SUB);
        text(x + 23, y + 30, "Applicazioni", F_TITLE, TX);
        text(x + q.w - 137, y + 42, sys.live_cd?"14 applicazioni":"13 applicazioni", F_SMALL, SUB);
        for (int j = 0; j < libcount(); j++) {
            Rect c = libcell(q, j);
            int add = (256 - t) * (1 + j / 3) * 3 / 256;
            c.y += add;
            if (inside(mx, my, c) || pick == j)roundr(c.x, c.y, c.w, c.h, 14, INK(I_SEL), 158);
            icon(c.x + c.w / 2 - 24, c.y + 4, 48, appicon(libapp(j)));
            center(c.x + c.w / 2, c.y + 58, appname(libapp(j)), F_BODY, TX);
        }
        return;
    }
    text(x + 23, y + 15, "tinymacOS  /  Tramontana", F_MICRO, INK(I_ACC_TXT));
    text(x + 22, y + 39, ov == OV_OFF ? "Vuoi spegnere il computer?" : "Vuoi riavviare il computer?",F_TITLE, TX);
    text(x + 23, y + 91, "I documenti non salvati andranno persi.", F_BODY, SUB);
    text(x + 23, y + 118, "Invio per confermare, Esc per annullare.", F_SMALL, SUB);
    button_face((Rect) {
        x + q.w - 100, y + 144, 80, 29
    }, 1);
    center(x + q.w - 60, y + 150, "Conferma", F_SMALL, night ? 0x10274a : 0xffffff);
}
void shell_draw(void)
{
    menubar();
    dock();
    paintoverlay();

}
static void doconfirm(void)
{
    int k = ov;
    overlay_show(0);
    if (k == OV_OFF || k == OV_RESTART)sc(S_POWER, k == OV_RESTART, 0, 0, 0);
}
int shell_click(int x, int y)
{
    pending=0;dirty = 1;
    if (y < BAR && x >= ctrl.x + ctrl.w + 3) {
        overlay_show(0);
        launch(CALENDAR);
        return 1;
    }
    if (y < BAR) {
        if (inside(x, y, srch)) {
            overlay_show(ov == OV_SEARCH && want ? 0 : OV_SEARCH);
            return 1;
        }
        if (inside(x, y, ctrl)) {
            overlay_show(ov == OV_CONTROL && want ? 0 : OV_CONTROL);
            return 1;
        }
        for (int k = 0; k < 7; k++)if (inside(x, y, barhit[k])) {
                if (ov == OV_MENU && menuId == k && want)overlay_show(0);
                else {
                    menuId = k;
                    overlay_show(OV_MENU);
                }
                return 1;
            }
    }
    if (ov && want) {
        Rect q = panel;
        if (!inside(x, y, q)) {
            overlay_show(0);
            return 1;
        }
        if(ov==OV_NEW_FOLDER){
            if(y>=q.y+q.h-50){if(x>=q.x+q.w-115)folder_confirm();else if(x>=q.x+q.w-215)overlay_show(0);}
            else if(y>=q.y+55&&y<q.y+91){folderAt=text_hit_offset(folderName,folderAt,q.w-60,x-q.x-30);folderSelected=0;fieldTick=sys.ticks;}
        } else if (ov == OV_MENU) {
            const char *l[16];
            int a[16], n = menus(menuId, l, a), dy = y - q.y - 6;
            if (dy >= 0 && dy / 31 < n)itemchoice(a[dy / 31]);
        } else if (ov == OV_SEARCH) {
            int dy = y - q.y - 81;
            if (dy >= 0)choose(MAX(0, pick - 5) + dy / 46);
            else{queryAt=proportional_hit(query,queryAt,q.w-135,x-q.x-59);querySelected=0;fieldTick=sys.ticks;}
        } else if (ov == OV_LIBRARY) {
            for (int j = 0; j < libcount(); j++)if (inside(x, y, libcell(q, j))) {
                    overlay_show(0);
                    launch(libapp(j));
                    break;
                }
        } else if (ov == OV_CONTROL) {
            control_click(q,x,y);
        } else if(ov==OV_HELP){
            if(inside(x,y,(Rect){q.x+q.w-100,q.y+q.h-44,80,29}))doconfirm();
        } else if (y > q.y + 139 && x > q.x + q.w - 116)doconfirm();
        return 1;
    }
    int dh = dockhit(x, y);
    if (dh >= 0) {
        if (pins[dh]) {
            launch(pins[dh]);
            shell_pop(pins[dh]);
        } else overlay_show(OV_LIBRARY);
        return 1;
    }
    if (inside(x, y, widget)&&deskhit(x,y)==-2) {
        for (int j = 0; j < nz; j++)if (!views[order[j]].mini && inside(x, y, views[order[j]].box))return 0;
        launch(CALENDAR);
        return 1;
    }
    if (mc) {
        int i = mchit(x, y);
        mc_exit();
        if (i >= 0)focuswin(i);
        return 1;
    }
    for(int j=nz;j--;){View*v=views+order[j];if(!v->mini&&!v->dead&&inside(x,y,v->box))return 0;}
    desktopFocus=1;int item=deskhit(x,y);deskSel=item;
    if(item!=-2){if(lastHit==item&&sys.ticks-deskTick<50){deskopen(item);lastHit=-2;}else{lastHit=item;deskTick=sys.ticks;}return 1;}
    lastHit=-2;
    return 0;
}
static void showctx(void){if(cmn){menuId=7;keyboardPick=-1;overlay_show(OV_MENU);trace("menu contestuale");}}
void context_ready(Event*e){if(!pending||e->a!=pending||e->d!=nonce||topwin()<0||views[topwin()].s.pid!=pending)return;
    pending=0;int a[]={ACT_OPEN,ACT_PACKAGE,ACT_PREVIEW,ACT_PROPS,ACT_NEW,ACT_FILE,ACT_CUT,ACT_COPY,ACT_PASTE,ACT_RENAME,ACT_DELETE,ACT_VIEW,ACT_MOUNT,ACT_UNMOUNT};
    const char*n[]={"Apri","Mostra contenuto pacchetto","Vista rapida","Ottieni informazioni","Nuova cartella…","Nuovo file…","Taglia","Copia","Incolla","Rinomina","Elimina…","Come icone / Come elenco","Monta","Smonta"};
    for(unsigned j=0;j<sizeof a/sizeof *a;j++)if(e->b&AMASK(a[j]))cmadd(n[j],a[j]);showctx();}
void shell_context(void){cmx=mx;cmy=my;owner=pending=cmn=0;nonce++;lastHit=-2;dirty=1;
    if(mc){mc_exit();return;}if(ov&&want){overlay_show(0);return;}if(my<BAR){cmadd("Centro di Controllo",163);showctx();return;}
    int d=dockhit(mx,my);if(d>=0){cmadd(pins[d]?"Apri applicazione":"Mostra applicazioni",pins[d]?100+pins[d]:161);if(pins[d])cmadd("Applicazioni",161);showctx();return;}
    for(int j=nz;j--;){int i=order[j];View*v=views+i;if(v->mini||v->dead||!inside(mx,my,v->box))continue;focuswin(i);owner=v->s.pid;
        int x=mx-v->box.x-1,y=my-v->box.y-TITLE;
        if(y>=0&&v->s.app==FINDER){pending=owner;eventto(i,(Event){.type=EV_CONTEXT,.a=x,.b=y,.d=nonce});return;}
        if(y>=0&&(v->s.app==TEXTEDIT||v->s.app==TERMINAL)){if(v->s.app==TERMINAL)cmadd("Taglia",ACT_CUT);cmadd("Copia",ACT_COPY);cmadd("Incolla",ACT_PASTE);if(v->s.app==TEXTEDIT)cmadd("Salva",ACT_SAVE);}
        cmadd("Riduci a icona",141);cmadd("Zoom",140);cmadd("Chiudi finestra",142);showctx();return;}
    deskSel=deskhit(mx,my);if(deskSel!=-2){cmadd("Apri",203);if(deskSel>=0){cmadd("Ottieni informazioni",204);cmadd("Rinomina",205);cmadd("Elimina…",206);}}
    else if(inside(mx,my,widget)){cmadd("Apri Calendario",100+CALENDAR);}
    else {desktopFocus=1;cmadd("Nuova cartella…",202);cmadd("Apri Scrivania in Finder",201);if(sys.live_cd)cmadd(INSTALLER_TITLE "…",100+INSTALLER);}showctx();}
int shell_key(Event *e)
{
    pending=0;int k = e->a;
    if (mc) {if (key_mission(e) || k == 27)mc_exit();return 1;}
    if(ov==OV_MENU&&want&&menuId==7){if(k==K_UP||k==K_DOWN){keyboardPick=(keyboardPick+(k==K_DOWN?1:cmn-1)+cmn)%cmn;dirty=1;return 1;}if(k=='\n'){if(keyboardPick>=0)itemchoice(codes[keyboardPick]);return 1;}}
    if (e->b && k == ' ') {
        overlay_show(want && ov == OV_SEARCH ? 0 : OV_SEARCH);
        return 1;
    }
    if (key_mission(e)) {
        if (ov && want)overlay_show(0);
        mc_enter();
        return 1;
    }
    if (e->b && (k == 'a' || k == 'A') && e->c) {
        overlay_show(OV_LIBRARY);
        return 1;
    }
    if (!ov || !want)return 0;
    if (k == 27) {
        overlay_show(0);
        return 1;
    }
    if(ov==OV_NEW_FOLDER){
        fieldTick=sys.ticks;
        if(k=='\n')folder_confirm();else if(text_edit_key(folderName,sizeof folderName,&folderAt,&folderSelected,k,e->b))folderError[0]=0;
        dirty=1;return 1;
    }
    if (ov == OV_CONTROL)return control_key(panel,e);
    if (ov == OV_SEARCH) {
        if (k == '\n') {
            choose(pick);
            return 1;
        }
        if (k == K_UP)pick = MAX(0, pick - 1);
        else if (k == K_DOWN)pick = MIN(MAX(0, count - 1), pick + 1);
        else {
            text_edit_key(query,sizeof query,&queryAt,&querySelected,k,e->b);fieldTick=sys.ticks;
            hits();
        }
    } else if (ov == OV_LIBRARY) {
        if (k == '\n') {
            overlay_show(0);
            launch(libapp(pick));
        } else {
            int d = k == K_LEFT ? -1 : k == K_RIGHT ? 1 : k == K_UP ? -4 : k == K_DOWN ? 4 : 0;
            pick = clampi(pick + d, 0, libcount() - 1);
        }
    } else if ((ov == OV_HELP || ov == OV_OFF || ov == OV_RESTART) && k == '\n')doconfirm();
    dirty = 1;
    return 1;
}

int shell_overlay_open(void) { return ov && want; }

/* Desktop files participate in cross-window drag without changing clipboard. */
int shell_drag_source(int x,int y,char out[PATHLEN]){int i=deskhit(x,y);if(i==-2)return 0;if(i==-1){strset(out,"@installer",PATHLEN);return 2;}deskpath(out,i);return 1;}

int shell_drop_folder(int x,int y){int i=deskhit(x,y);return i>=0&&i<ndesk&&ondesk[i].dir?i:-1;}

int shell_cursor(int x,int y)
{
 /* The existing editable overlays own the pointer, not covered windows. */
 Rect q=panel;
 if(ov==OV_NEW_FOLDER&&inside(x,y,(Rect){q.x+20,q.y+55,q.w-40,36}))return CUR_TEXT;
 if(ov==OV_SEARCH&&inside(x,y,(Rect){q.x+59,q.y+20,q.w-135,38}))return CUR_TEXT;
 (void)x;(void)y;return CUR_NORMAL;
}
