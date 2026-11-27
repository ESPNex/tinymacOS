#include "ui.h"
#include "date.h"
#define AGENDA DOCS "/Agenda"
static int yr, mo, dy, edit, exists, longnote, marks[32];
static u32 lastrev;
static char memo[68], prev[68], notice[80];
static int memoAt,memoSelected;
static void two(char *s, int v, int cap)
{
    if (v < 10)append(s, "0", cap);
    addnum(s, v, cap);
}
static void path(char *s, int d)
{
    strset(s, AGENDA "/", PATHLEN);
    addnum(s, yr, PATHLEN);
    append(s, "-", PATHLEN);
    two(s, mo, PATHLEN);
    append(s, "-", PATHLEN);
    two(s, d, PATHLEN);
    append(s, ".txt", PATHLEN);
}
static void refresh(void)
{
    char p[PATHLEN];
    path(p, dy);
    int n = readfile(p, memo, sizeof(memo) - 1);
    exists = n >= 0;
    memo[MAX(0, n)] = 0;
    Dirent d;
    longnote = exists && !sc(S_STAT, p, &d, 0, 0) && d.size >= (int)sizeof memo;
    for (int i = 1; i <= monthdays(yr, mo); i++) {
        path(p, i);
        marks[i] = sc(S_STAT, p, &d, 0, 0) == 0 && !d.dir;
    }
    notice[0] = 0;
    sc(S_INFO, &si, 0, 0, 0);
    lastrev = si.fsrev;
}
static void step(int d)
{
    mo += d;
    if (mo < 1) {
        mo = 12;
        yr--;
    }
    if (mo > 12) {
        mo = 1;
        yr++;
    }
    if (yr < 1600) {
        yr = 1600;
        mo = 1;
    }
    if (yr > 9999) {
        yr = 9999;
        mo = 12;
    }
    dy = MIN(dy, monthdays(yr, mo));
    refresh();
    ui_motion(FX_SLIDE);
}
static void save(void)
{
    char p[PATHLEN];
    Dirent d;
    if (sc(S_STAT, AGENDA, &d, 0, 0) < 0)sc(S_CREATE, AGENDA, 1, 0, 0);
    path(p, dy);
    int r = memo[0] ? writefile(p, memo, strlen(memo)) : exists ? sc(S_REMOVE, p, 0, 0, 0) : 0;
    if (r >= 0) {
        edit = 0;
        refresh();
        strset(notice, si.persistent ? "Salvato sul disco" : "Salvato nella sessione live", 80);
        trace(p);
        ui_motion(FX_DISSOLVE);
    } else strset(notice, "Salvataggio non riuscito", 80);
}
static void editing(void)
{
    if (longnote) {
        char p[PATHLEN];
        path(p, dy);
        sc(S_SPAWN, TEXTEDIT, p, 0, 0);
        return;
    }
    strset(prev, memo, sizeof prev);
    edit = 1;
    ui_motion(FX_SHEET);
    memoAt=strlen(memo);memoSelected=0;
}
static int split(void)
{
    return aw >= 590 ? aw - 212 : aw - 158;
}
static void paint(void)
{
    begin();
    int P = split(), cw = (P - 24) / 7, ch = MAX(22, (ah - 120) / 6), offset = (weekday(yr, mo, 1) + 6) % 7;
    char title[60];
    strset(title, monthname(mo), 60);
    append(title, " ", 60);
    addnum(title, yr, 60);
    Text(16, 9, "Mese", F_SMALL, si.dark ? 0xff8e88 : 0xbf3936);
    Text(15, 29, title, F_TITLE, TX);
    button(P - 92, 20, 34, "←", 0);
    button(P - 52, 20, 34, "→", 0);
    const char *n[] = {"L", "M", "M", "G", "V", "S", "D"};
    for (int j = 0; j < 7; j++)Center(12 + j * cw + cw / 2, 83, n[j], F_MICRO, SUB);
    for (int j = 1; j <= monthdays(yr, mo); j++) {
        int p = offset + j - 1, x = 12 + p % 7 * cw, y = 107 + p / 7 * ch;
        int on = j == dy, now = yr == (int)si.year && mo == (int)si.month && j == (int)si.day;
        if (on || now)Box(x + (cw - 32) / 2, y, 32, MIN(ch - 4, 32), 16, on ? 0xd34f49 : SEL);
        char s[12];
        number(s, j);
        Center(x + cw / 2, y + 4, s, F_BODY, on ? 0xffffff : TX);
        if (marks[j])Box(x + cw / 2 - 2, y + 23, 4, 3, 2, on ? 0xffffff : ACC);
    }
    Box(P + 1, 2, aw - P - 7, ah - 9, 12, PANEL);
    int x = P + 16;
    Text(x, 19, "Agenda", F_SMALL, SUB);
    Text(x, 43, dayname(weekday(yr, mo, dy)), F_BOLD, TX);
    char s[32];
    number(s, dy);
    Text(x - 2, 62, s, F_HERO, TX);
    Text(x, 132, monthname(mo), F_CAPTION, SUB);
    Box(x, 170, aw - x - 15, 1, 0, EDGE);
    Text(x, 184, exists ? "Promemoria" : "Il tuo giorno", F_BOLD, TX);
    if (memo[0]) {
        int cols = MAX(12, MIN(30, (aw - x - 18) / 8));
        for (int j = 0; j < 4 && j * cols < (int)strlen(memo); j++) {
            char z[36];
            strset(z, memo + j * cols, cols + 1);
            Text(x, 207 + j * 18, z, F_SMALL, TX);
        }
    } else Text(x, 208, "Nessun promemoria", F_SMALL, SUB);
    button(x, ah - 72, MIN(142, aw - x - 12), longnote ? "Apri in TextEdit" : exists ? "Modifica nota" : "Aggiungi nota",
           1);
    Text(x, ah - 31, notice[0] ? notice : "Invio per scrivere", F_SMALL, SUB);
    if (edit)inputmodal_at("Promemoria del giorno", memo, "Una nota per data. Vuota per rimuoverla.",0,memoAt);
    finish();
}
void app_main(void)
{
    win(651, 378, "Calendario");
    yr = si.year;
    mo = si.month >= 1 && si.month <= 12 ? si.month : 1;
    dy = MAX(1, MIN((int)si.day, monthdays(yr, mo)));
    refresh();
    paint();
    for (;;) {
        Event e;
        int d = 0;
        while (sc(S_EVENT, &e, 0, 0, 0) > 0) {
            d = 1;
            if (baseevent(&e))continue;
            if (e.type == EV_CLOSE) {
                if (edit) {
                    edit = 0;
                    strset(memo, prev, sizeof memo);
                } else quit();
                continue;
            }
            if (edit && e.type == EV_KEY) {
                int k = e.a;
                if (k == 27) {
                    strset(memo, prev, sizeof memo);
                    edit = 0;
                } else if (k == '\n' || (e.b && k == 's'))save();
                else text_edit_key(memo,sizeof memo,&memoAt,&memoSelected,k,e.b);
                continue;
            }
            if(edit&&e.type==EV_CLICK){int w=MIN(aw-46,430),x=(aw-w)/2,y=(ah-143)/2;
                if(e.a>=x+16&&e.a<x+w-16&&e.b>=y+43&&e.b<y+78){memoAt=text_hit_offset(memo,memoAt,w-50,e.a-x-25);memoSelected=0;}}
            if (edit)continue;
            if (e.type == EV_KEY) {
                if (e.a == K_LEFT)step(-1);
                else if (e.a == K_RIGHT)step(1);
                else if (e.a == '\n')editing();
                else if (e.a == 't') {
                    yr = si.year;
                    mo = si.month;
                    dy = si.day;
                    refresh();
                    ui_motion(FX_SLIDE);
                } else if (e.a == K_UP || e.a == K_DOWN) {
                    dy = MAX(1, MIN(monthdays(yr, mo), dy + (e.a == K_UP ? -1 : 1)));
                    refresh();
                    ui_motion(FX_SELECT);
                }
            }
            if (e.type == EV_CLICK) {
                int P = split();
                if (e.a >= P - 94 && e.a < P - 17 && e.b < 54)step(e.a < P - 55 ? -1 : 1);
                else if (e.a >= P && e.b >= ah - 78 && e.b < ah - 39)editing();
                else if (e.a >= 12 && e.a < P - 12 && e.b >= 107) {
                    int cc = (P - 24) / 7, ch = MAX(22, (ah - 120) / 6), j = (e.b - 107) / ch * 7 + (e.a - 12) / cc - (weekday(yr, mo,
                                                1) + 6) % 7 + 1;
                    if (j >= 1 && j <= monthdays(yr, mo)) {
                        dy = j;
                        refresh();
                        ui_motion(FX_SELECT);
                        if (e.c)editing();
                    }
                }
            }
        }
        SysInfo now;
        sc(S_INFO, &now, 0, 0, 0);
        si = now;
        if (!edit && now.fsrev != lastrev) {
            lastrev = now.fsrev;
            refresh();
            d = 1;
        }
        if (d)paint();
        slp(3);
    }
}
