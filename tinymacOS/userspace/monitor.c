#include "ui.h"
#include "display_size.h"
#include "cpu_sample.h"
static ProcInfo rows[NPROC];
static int n, paused, sort, selected = -1, cpu, history[42], hlen;
static u64 lastTick;
static int corep[8];
static u64 ptick[8], pbusy[8];
static void sample(void)
{
    SysInfo t;
    sc(S_INFO, &t, 0, 0, 0);
    si=t;lastTick=t.ticks;
    int nc=MIN(8,(int)t.ncpu);if(nc<0)nc=0;
    u64 total=0,busy=0;
    for(int c=0;c<nc;c++){
        u64 dt,db;
        cpu_interval(t.core_ticks[c],t.core_busy[c],ptick[c],pbusy[c],&dt,&db);
        corep[c]=cpu_percent(db,dt);total+=dt;busy+=db;
        ptick[c]=t.core_ticks[c];pbusy[c]=t.core_busy[c];
    }
    for(int c=nc;c<8;c++){ptick[c]=pbusy[c]=0;corep[c]=0;}
    /* busy_ticks is summed across CPUs, while ticks is a wall clock. Dividing
     * the former by the latter incorrectly saturates multicore load at 100%. */
    cpu=cpu_percent(busy,total);
    if(total){
        if(hlen==42)memmove(history,history+1,41*sizeof(int));else hlen++;
        history[hlen-1]=cpu;
    }
    n = sc(S_PS, rows, NPROC, 0, 0);
    for (int i = 0; i < n; i++)for (int j = i + 1; j < n;
                                        j++)if (sort ? rows[j].cpu_ticks > rows[i].cpu_ticks : rows[j].pid < rows[i].pid) {
                ProcInfo v = rows[i];
                rows[i] = rows[j];
                rows[j] = v;
            }
}
static void paint(void)
{
    begin();
    Text(15, 9, "Monitoraggio in tempo reale", F_SMALL, SUB);
    Text(14, 29, "Attività del sistema", F_TITLE, TX);
    button(aw - 107, 21, 90, paused ? "Riprendi" : "Pausa", paused);
    int w = (aw - 38) / 2, y = 79;
    Box(13, y, w, 92, 13, CARD);
    Box(24 + w, y, w, 92, 13, CARD);
    Text(26, y + 10, "CPU attiva", F_SMALL, SUB);
    char z[90] = "";
    addnum(z, cpu, 90);
    append(z, "%", 90);
    Text(25, y + 29, z, F_TITLE, TX);
    int gx = 100, gw = w - 103;
    Box(gx, y + 20, gw, 52, 6, SOFT);
    if (hlen > 1)for (int j = 1; j < hlen; j++) {
            int x = gx + 3 + (42 - hlen + j - 1) * (gw - 7) / 41;
            int X = gx + 3 + (42 - hlen + j) * (gw - 7) / 41;
            Line(x, y + 68 - history[j - 1] * 44 / 100, X, y + 68 - history[j] * 44 / 100, si.dark ? 0x7edbb8 : 0x167855);
        }
    Text(26, y + 67, paused ? "In pausa" : "Campioni scheduler", F_MICRO, SUB);
    int x = w + 37;
    Text(x, y + 10, "Memoria in uso", F_SMALL, SUB);
    z[0] = 0;
    display_size(z,sizeof z,si.total>=si.free?si.total-si.free:0);
    Text(x, y + 26, z, F_TITLE, TX);
    Box(x, y + 65, w - 29, 7, 3, SOFT);
    Box(x, y + 65, MAX(3, (int)((si.total - si.free) * (w - 29) / MAX(1ull, si.total))), 7, 3, ACC);
    // Per-core load bars (SysInfo core_busy / core_ticks).
    int nc2 = (int)si.ncpu;
    if (nc2 < 1)nc2 = 1;
    if (nc2 > 4)nc2 = 4;
    for (int c = 0; c < nc2; c++) {
        int Y = 176 + c * 14;
        z[0] = 0;
        append(z, "CPU ", 90);
        addnum(z, c, 90);
        Text(17, Y, z, F_MICRO, SUB);
        Box(64, Y + 1, aw - 128, 8, 4, SOFT);
        int bw = corep[c] * (aw - 128) / 100;
        if (bw < 2 && corep[c])bw = 2;
        if (bw > aw - 128)bw = aw - 128;
        Box(64, Y + 1, bw, 8, 4, ACC);
        z[0] = 0;
        addnum(z, corep[c], 90);
        append(z, "%", 90);
        Text(aw - 56, Y, z, F_MICRO, SUB);
    }
    int yy = 236;
    Text(17, yy, "Processi", F_BOLD, TX);
    button(aw - 145, yy - 6, 127, sort ? "Ordina: CPU" : "Ordina: PID", 0);
    int cols[] = {21, 76, aw - 234, aw - 149, aw - 70};
    const char *head[] = {"PID", "NOME", "THREAD", "TICK CPU", "STATO"};
    for (int j = 0; j < 5; j++)Text(cols[j], yy + 29, head[j], F_MICRO, SUB);
    int vis = MAX(2, (ah - 294) / 27);
    int idx = 0;
    for (int j = 0; j < n; j++)if (rows[j].pid == selected)idx = j;
    int off = MAX(0, idx - vis + 1);
    for (int j = off; j < n && j < off + vis; j++) {
        int Y = yy + 51 + (j - off) * 27;
        ProcInfo *r = rows + j;
        if (r->pid != selected && (j & 1))Box(13, Y - 3, aw - 25, 25, 5, SOFT);
        if (r->pid == selected)Box(13, Y - 3, aw - 25, 25, 7, SEL);
        number(z, r->pid);
        Text(cols[0], Y, z, F_SMALL, TX);
        Icon(cols[1], Y - 2, 21, r->app ? appicon(r->app) : 14);
        Text(cols[1] + 28, Y, r->name, F_SMALL, TX);
        number(z, r->threads);
        Text(cols[2], Y, z, F_SMALL, TX);
        number(z, r->cpu_ticks);
        Text(cols[3], Y, z, F_SMALL, TX);
        Text(cols[4], Y, r->state == 1 ? "Pronto" : "Attesa", F_SMALL, SUB);
    }
    finish();
}
void app_main(void)
{
    win(660, 464, "Monitor Attività");
    sample();
    paint();
    trace("telemetria del kernel attiva");
    for (;;) {
        int d = 0;
        u64 t = sc(S_TICKS, 0, 0, 0, 0);
        Event e;
        while (sc(S_EVENT, &e, 0, 0, 0) > 0) {
            d = 1;
            if (baseevent(&e))continue;
            if (e.type == EV_CLOSE)quit();
            int pause = (e.type == EV_KEY && e.a == ' ') || (e.type == EV_CLICK && e.a >= aw - 110 && e.b < 56);
            if (pause) {
                paused = !paused;
                trace(paused ? "campionamento in pausa" : "campionamento ripreso");
                if (!paused)sample();
            }
            if ((e.type == EV_KEY && e.a == 's') || (e.type == EV_CLICK && e.a >= aw - 152 && e.b >= 228 && e.b < 261)) {
                sort = !sort;
                sample();
                ui_motion(FX_SELECT);
            }
            if (e.type == EV_CLICK && e.b >= 282 && e.b < ah - 9) {
                int cur = 0;
                for (int h = 0; h < n; h++)if (rows[h].pid == selected)cur = h;
                int j = MAX(0, cur - MAX(2, (ah - 294) / 27) + 1) + (e.b - 284) / 27;
                if (j >= 0 && j < n)selected = rows[j].pid;
            }
            if (e.type == EV_KEY && (e.a == K_DOWN || e.a == K_UP) && n) {
                int j = 0;
                while (j < n && rows[j].pid != selected)j++;
                j = (j + (e.a == K_DOWN ? 1 : n - 1)) % n;
                selected = rows[j].pid;
            }
        }
        if (!paused && t - lastTick >= 80) {
            sample();
            d = 1;
        }
        if (d)paint();
        slp(4);
    }
}
