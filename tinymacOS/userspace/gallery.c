#include "ui.h"
static int idx;

static const char *const desc[] = {"Vetro scolpito · Studio ottico", "Pieghe di luce · Studio cromatico", "Acque di marea · Studio aereo", "Ultima luce sulla costa · Paesaggio digitale", "Lamine di ghiaccio · Studio minerale", "Luci polari · Paesaggio notturno", "Ultima luce · Studio di rame e dune", "Riviera ligure · Luci del crepuscolo", "Vento caldo dal sud · Studio sabbioso", "Luce di mezzogiorno · Studio marino", "Notte sul mare · Studio lunare", "Aria di mare · Studio luminoso", "Vento del nord · Costa minerale"};
_Static_assert(COUNT(desc)==NWALL, "wallpaper description count");
static void selecti(int i)
{
    idx = (i + NWALL) % NWALL;
    ui_motion(FX_SLIDE);
    trace(wallname(idx));
}
static void apply(void)
{
    if (sc(S_PREF, 1, idx, 0, 0) == 0) {
        sc(S_INFO, &si, 0, 0, 0);
        trace("sfondo applicato");
        ui_motion(FX_SELECT);
    }
}
static void paint(void)
{
    begin();
    int side = 143, x = side + 12;
    Box(0, 0, side, ah, 0, PANEL);
    Box(side, 0, 1, ah, 0, EDGE);
    Text(14, 15, "Sfondi", F_BOLD, TX);
    for (int j = 0; j < NWALL; j++) {
        int step=MAX(23,MIN(49,(ah-43)/NWALL)),y=36+j*step;
        if(j==idx)Box(5,y,side-10,step-2,8,SEL);
        Photo(12,y+5,39,step-12,j,6);
        const char *shorts[]={"Prisma","Alba","Laguna","Aurora","Glaciale","Boreale","Vespertino","Riviera","Levante","Meriggio","Notturno","Brezza","Tramontana"};
        _Static_assert(COUNT(shorts)==NWALL, "wallpaper label count");
        Text(58,y+step/2-8,shorts[j],F_SMALL,j==idx?EMP:TX);
    }
    int pw = aw - x - 13, ph = ah - 104;
    Photo(x, 5, pw, ph, idx, 13);
    Text(x + 2, ph + 19, wallname(idx), F_TITLE, TX);
    Text(x + 2, ph + 59, desc[idx], F_SMALL, SUB);
    button(aw - 161, ph + 29, 143, si.wall == (u32)idx ? "Sfondo attuale" : "Usa come sfondo", si.wall != (u32)idx);
    finish();
}
void app_main(void)
{
    win(733, 399, "Galleria");
    idx = si.wall % NWALL;
    paint();
    for (;;) {
        Event e;
        int d = 0;
        while (sc(S_EVENT, &e, 0, 0, 0) > 0) {
            d = 1;
            if (baseevent(&e))continue;
            if (e.type == EV_CLOSE)quit();
            if (e.type == EV_KEY) {
                if (e.a == K_LEFT || e.a == K_UP)selecti(idx - 1);
                if (e.a == K_RIGHT || e.a == K_DOWN)selecti(idx + 1);
                if (e.a == '\n')apply();
            }
            if (e.type == EV_CLICK) {
                if (e.a < 135 && e.b >= 36 && e.b < 36+NWALL*MAX(23,MIN(49,(ah-43)/NWALL)))selecti((e.b-36)/MAX(23,MIN(49,(ah-43)/NWALL)));
                else if (e.a >= aw - 166 && e.b >= ah - 88)apply();
            }
        }
        SysInfo next;
        sc(S_INFO, &next, 0, 0, 0);
        if (next.wall != si.wall || next.theme != si.theme)d = 1;
        si = next;
        if (d)paint();
        slp(5);
    }
}
