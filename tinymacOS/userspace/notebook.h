#pragma once
#include "ui.h"
// Plain text per note; memo color is stored separately so documents remain readable in Finder.
static char folder[PATHLEN], path[PATHLEN], body[6001], search[48], status[95];
static int ids[NSLOTS], ns, id = -1, dirty, focus, cur, confirm, palette;
static char snippets[NSLOTS][54], titles[NSLOTS][51];
static u64 due;
static const u32 paper[] = {0xffefa9, 0xd6ecc2, 0xcce4fa, 0xf8d2dc};
static void pathid(char *p, int i)
{
    char s[32] = "";
    addnum(s, i, 32);
    append(s, ".txt", 32);
    absolutepath(p, folder, s);
}
static int contains(const char *s, const char *q)
{
    for (; *s; s++) {
        int j = 0;
        while (q[j] && s[j] && (q[j] | 32) == (s[j] | 32))j++;
        if (!q[j])return 1;
    }
    return !*q;
}
static void extract(const char *s, char *p, int n)
{
    int j = 0;
    while (s[j] && s[j] != '\n' && j < n - 1) {
        p[j] = s[j];
        j++;
    }
    p[j] = 0;
}
static int validname(const char *s)
{
    int j = 0;
    while (s[j] >= '0' && s[j] <= '9')j++;
    return j && eq(s + j, ".txt");
}
static void indexnotes(void)
{
    static Dirent de[NSLOTS];
    int n = sc(S_LIST, folder, de, NSLOTS, 0);
    ns = 0;
    for (int j = 0; j < n; j++) {
        if (de[j].dir || !validname(de[j].name))continue;
        int i = val(de[j].name);
        char p[PATHLEN];
        static char b[6001];
        pathid(p, i);
        int z = readfile(p, b, sizeof(b) - 1);
        if (z < 0)continue;
        b[z] = 0;
        char t[51], s[54];
        extract(b, t, 48);
        char *q = b;
        while (*q && *q != '\n')q++;
        if (*q)q++;
        extract(q, s, 51);
        if (*search && !contains(b, search))continue;
        int h = ns++;
        ids[h] = i;
        strset(titles[h], *t ? t : "Senza titolo", 51);
        strset(snippets[h], *s ? s : "Nessun testo", 54);
    }
}
static int save(void)
{
    if (!dirty || id < 0)return 1;
    int n = strlen(body);
    if (writefile(path, body, n) != n) {
        strset(status, "Impossibile salvare: controlla lo spazio sul volume.", 95);
        return 0;
    }
    dirty = 0;
    strset(status, "Salvato", 95);
    indexnotes();
    return 1;
}
static void colorpath(char *p)
{
    strset(p, path, PATHLEN);
    append(p, ".color", PATHLEN);
}
static void selectid(int i)
{
    if (!save())return;
    id = i;
    cur = 0;
    pathid(path, id);
    int z = readfile(path, body, 6000);
    body[MAX(0, z)] = 0;
    cur = strlen(body);
    palette = i % 4;
    if (STICKY) {
        char p[PATHLEN], v[2];
        colorpath(p);
        if (readfile(p, v, 1) == 1 && v[0] >= '0' && v[0] <= '3')palette = v[0] - '0';
    }
    focus = 0;
    status[0] = 0;
}
static void create(void)
{
    if (!save())return;
    search[0] = 0;
    for (int i = 1; i <= NSLOTS; i++) {
        char p[PATHLEN];
        Dirent d;
        pathid(p, i);
        if (sc(S_STAT, p, &d, 0, 0)) {
            if (writefile(p, "", 0) == 0) {
                selectid(i);
                indexnotes();
            } else strset(status, "Impossibile creare: il volume è pieno.", 95);
            return;
        }
    }
}
static void removeone(void)
{
    if (id < 0)return;
    if (!sc(S_REMOVE, path, 0, 0, 0)) {
        if (STICKY) {
            char p[PATHLEN];
            colorpath(p);
            sc(S_REMOVE, p, 0, 0, 0);
        }
        dirty = 0;
        id = -1;
        body[cur = 0] = 0;
        indexnotes();
        if (ns)selectid(ids[0]);
    }
    confirm = 0;
}
static void recolor(void)
{
    if (id < 0)return;
    int c = (palette + 1) % 4;
    char p[PATHLEN], v = '0' + c;
    colorpath(p);
    if (writefile(p, &v, 1) == 1)palette = c;
}
static void edit(int k)
{
    int n = strlen(body);
    if (id < 0) {
        create();
        if (id < 0)return;
    }
    if (k == K_LEFT) {
        cur = text_prev(body,cur);
        return;
    }
    if (k == K_RIGHT) {
        cur = text_next(body,cur);
        return;
    }
    if (k == K_HOME) {
        while (cur && body[cur - 1] != '\n')cur--;
        return;
    }
    if (k == K_END) {
        while (cur < n && body[cur] != '\n')cur++;
        return;
    }
    if (k == K_UP || k == K_DOWN) {
        int a = cur;
        while (a && body[a - 1] != '\n')a--;
        int col = text_columns(body+a,cur-a);
        if (k == K_UP) {
            if (!a)return;
            int end = a - 1;
            a = end;
            while (a && body[a - 1] != '\n')a--;
            cur = text_offset(body,a,end,col);
        } else {
            while (cur < n && body[cur] != '\n')cur++;
            if (cur < n) {
                a = ++cur;
                int end=cur;while(end<n&&body[end]!='\n')end++;
                cur=text_offset(body,a,end,col);
            }
        }
        return;
    }
    if(k==8&&cur)text_backspace(body,&cur);
    else if(k==K_DEL&&cur<n)text_delete(body,cur);
    else if(text_printable(k)||k=='\n'){if(!text_insert(body,sizeof body,&cur,k))return;}
    else return;
    dirty = 1;
    due = sc(S_TICKS, 0, 0, 0, 0) + 75;
    strset(status, "Modificato", 95);
}
static int rail(void)
{
    return MIN(205, aw / 3);
}
static int firstrow(void)
{
    int ix = 0;
    for (int i = 0; i < ns; i++)if (ids[i] == id)ix = i;
    return MAX(0, ix - MAX(1, (ah - 135) / 55) + 1);
}
static int lineends[6002], nlines;
static int editor(int draw, int hitx, int hity)
{
    int r = rail(), c = MIN(88, MAX(12, (aw - r - 40) / 8)), at = 0, n = strlen(body), here = 0;
    lineends[nlines = 0] = 0;
    while (at < n) {
        int z = 0;
        while (at < n && z < c && body[at] != '\n') {
            at=text_next(body,at);
            z++;
        }
        if (at < n && body[at] == '\n')at++;
        else if (at == n)break;
        lineends[++nlines] = at;
    }
    nlines++;
    for (int j = 0; j < nlines; j++)if (lineends[j] <= cur)here = j;
    int rows = MAX(1, (ah - 111) / 22), start = MAX(0, here - rows + 1), x = r + 20, y = 74;
    for (int i = start; i < nlines && i < start + rows; i++, y += 22) {
        int a = lineends[i], b = i + 1 < nlines ? lineends[i + 1] : n;
        if (b > a && body[b - 1] == '\n')b--;
        int z = b - a;
        if (!draw && hity >= y - 2 && hity < y + 20)return text_offset(body,a,b,MAX(0,(hitx-x)/8));
        if (draw) {
            char s[360];
            memcpy(s, body + a, z);
            s[z] = 0;
            Text(x, y, s, F_MONO, STICKY ? 0x34322c : TX);
            if (!focus && i == here)Box(x + text_columns(body+a,MIN(z,cur-a)) * 8, y + 1, 1, 16, 0, STICKY ? 0x423941 : ACC);
        }
    }
    return cur;
}
static void paint(void)
{
    begin();
    int r = rail();
    Box(0, 0, r, ah, 0, PANEL);
    Box(r, 0, aw - r, ah, 0, STICKY ? paper[palette] : CARD);
    Text(14, 14, STICKY ? "Tutti i memo" : "Tutte le note", F_BOLD, TX);
    button(r - 43, 11, 30, "+", 1);
    Box(10, 48, r - 20, 27, 8, focus ? SEL : SOFT);
    Text(18, 53, *search ? search : "Cerca", F_SMALL, focus ? EMP : SUB);
    int st = firstrow();
    for (int j = st; j < ns && j < st + MAX(1, (ah - 135) / 55); j++) {
        int y = 89 + (j - st) * 55;
        Box(6, y - 3, r - 12, 51, 8, id == ids[j] ? SEL : PANEL);
        char s[36];
        strset(s, titles[j], MIN(32, (r - 22) / 8));
        Text(15, y + 2, s, F_BOLD, TX);
        strset(s, snippets[j], MIN(34, (r - 22) / 7));
        Text(15, y + 26, s, F_SMALL, SUB);
    }
    char badge[44] = "";
    addnum(badge, ns, 44);
    append(badge, STICKY ? " memo" : ns == 1 ? " nota" : " note", 44);
    Text(14, ah - 26, badge, F_SMALL, SUB);
    u32 ink = STICKY ? 0x5b5140 : SUB;
    Text(r + 19, 20, STICKY ? "Sulla scrivania" : "Sul mio computer", F_SMALL, ink);
    if (STICKY)button(aw - 163, 12, 74, "Colore", 0);
    button(aw - 81, 12, 66, "Elimina", 0);
    Box(r + 13, 53, aw - r - 27, 1, 0, STICKY ? 0xc3b48c : EDGE);
    if (id >= 0) {
        editor(1, 0, 0);
        if (!*body)Text(r + 25, 76, "Scrivi qui…", F_BODY, ink);
    } else Text(r + 20, 83, "Crea un nuovo elemento con +.", F_BODY, ink);
    Text(r + 20, ah - 27, status, F_SMALL, ink);
    if (confirm) {
        int x = (aw - 337) / 2, y = (ah - 130) / 2;
        Sheet(x, y, 337, 130, 12);
        Text(x + 19, y + 16, "Eliminare questo elemento?", F_BOLD, TX);
        Text(x + 19, y + 45, "L’operazione non può essere annullata.", F_SMALL, SUB);
        button(x + 102, y + 85, 94, "Annulla", 0);
        button(x + 208, y + 85, 106, "Elimina", 1);
    }
    finish();
}
void app_main(void)
{
    win(STICKY ? 582 : 688, STICKY ? 342 : 387, STICKY ? "Memo" : "Note");
    strset(folder, HOME "/Library/Application Support/", PATHLEN);
    append(folder, STICKY ? "Stickies" : "Notes", PATHLEN);
    sc(S_CREATE, folder, 1, 0, 0);
    indexnotes();
    if (ns)selectid(ids[0]);
    else create();
    paint();
    for (;;) {
        int d = 0;
        Event e;
        while (sc(S_EVENT, &e, 0, 0, 0) > 0) {
            d = 1;
            if (baseevent(&e))continue;
            if (e.type == EV_CLOSE) {
                if (save())quit();
                continue;
            }
            if (confirm) {
                if (e.type == EV_KEY) {
                    if (e.a == '\n')removeone();
                    if (e.a == 27)confirm = 0;
                }
                if (e.type == EV_CLICK && e.b >= (ah - 130) / 2 + 81 && e.b < (ah - 130) / 2 + 115) {
                    if (e.a > (aw - 337) / 2 + 202)removeone();
                    else confirm = 0;
                }
                continue;
            }
            if (e.type == EV_ACTION) {
                if (e.a == ACT_NEW)create();
                if (e.a == ACT_SAVE)save();
                if (e.a == ACT_DELETE && id >= 0)confirm = 1;
            }
            if (e.type == EV_CLICK) {
                int r = rail();
                if (e.a < r) {
                    if (e.b < 41 && e.a > r - 50)create();
                    else if (e.b >= 47 && e.b < 78)focus = 1;
                    else if (e.b >= 86 && e.b < ah - 31) {
                        int i = firstrow() + (e.b - 86) / 55;
                        if (i < ns)selectid(ids[i]);
                    }
                } else if (e.b < 45 && e.a > aw - 84 && id >= 0)confirm = 1;
                else if (STICKY && e.b < 45 && e.a > aw - 165)recolor();
                else {
                    focus = 0;
                    cur = editor(0, e.a, e.b);
                }
            }
            if (e.type == EV_KEY) {
                if (e.b) {
                    if (e.a == 'n')create();
                    if (e.a == 'f')focus = 1;
                    if (e.a == 's')save();
                    if (e.a == 'k' && STICKY)recolor();
                    if (e.a == K_DEL && id >= 0)confirm = 1;
                    continue;
                }
                if (focus) {
                    if (e.a == '\n') {
                        if (ns)selectid(ids[0]);
                        focus = 0;
                    } else if (e.a == 27) {
                        focus = 0;
                        *search = 0;
                    } else text_append_key(search,sizeof search,e.a);
                    indexnotes();
                } else edit(e.a);
            }
        }
        if (dirty && (u64)sc(S_TICKS, 0, 0, 0, 0) >= due) {
            save();
            due += 100;
            d = 1;
        }
        if (d)paint();
        slp(3);
    }
}
