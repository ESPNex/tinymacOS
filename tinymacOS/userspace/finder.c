#include "preview.h"
#include "volume.h"
static char cwd[PATHLEN] = HOME, clipfile[PATHLEN], field[PATHLEN], note[96], cwdarg[PATHLEN];
static Dirent files[NSLOTS];
static int n, sel = -1, listview, offset, dialog;
static u32 rev;
static char prefix[48];
static u64 prefixTick;
static char trail[12][PATHLEN];
static int hp = -1, hn, trav;
static int side(void)
{
    return aw < 570 ? 145 : 171;
}
static int cols(void)
{
    return MAX(2, (aw - side() - 26) / 112);
}
static int rows(void)
{
    return MAX(1, (ah - 141) / (listview ? 31 : 82));
}
static int visible(void)
{
    return listview ? rows() : rows() * cols();
}
static const char *const navs[] = {"Inizio", "Scrivania", "Documenti", "Download", "Immagini", "Applicazioni", "Disco di avvio", "KernelCollections", "Libreria"};
static const char *const paths[] = {HOME, HOME "/Desktop", DOCS, HOME "/Downloads", HOME "/Pictures", "/Applications", "/", KERNELDIR, "/Library"};
static int navicon[] = {8, 9, 6, 10, 11, 13, 33, 32, 37};
static int ny(int i)
{
    return i < 5 ? 39 + i * 30 : 222 + (i - 5) * 30;
}
static int bundle(Dirent *f)
{
    if(!f->dir||!f->ro)return 0;
    char name[56];
    for (int a = 1; a <= lastapp(si.live_cd); a++) {
        strset(name, bundlename(a), 56);
        append(name, ".app", 56);
        if (eq(name, f->name)&&eq(cwd,apploc(a)))return a;
    }
    return 0;
}
static int idico(Dirent *f)
{
    int a = bundle(f);
    if (a)return appicon(a);
    if (bootpath(cwd))return f->dir ? 37 : eq(f->name, "KernelCache") || eq(f->name, "boot.efi") ? 32 : 7;
    if (!f->dir)return 7;
    /* Applications at the volume root uses its dedicated sidebar symbol too;
     * do not mislabel an unrelated user folder with the same name. */
    if (eq(cwd, "/") && eq(f->name, "Applications"))return 13;
    if (eq(f->name, "Desktop"))return 23;
    if (eq(f->name, "Documents"))return 24;
    if (eq(f->name, "Downloads"))return 25;
    if (eq(f->name, "Pictures"))return 26;
    return 6;
}
static const char *pretty(const char *s)
{
    const char *from[] = {"Desktop", "Documents", "Downloads", "Pictures", "Applications", "User", "Welcome.txt"};
    const char *to[] = {"Scrivania", "Documenti", "Download", "Immagini", "Applicazioni", "Inizio", "Benvenuto.txt"};
    for (int i = 0; i < 7; i++)if (eq(s, from[i]))return to[i];
    return s;
}
static void reload(void)
{
    n = sc(S_LIST, cwd, files, NSLOTS, 0);
    if (n < 0) {
        strset(cwd, HOME, PATHLEN);
        n = sc(S_LIST, cwd, files, NSLOTS, 0);
    }
    if (n < 0)
        n = 0;
    if (sel >= n)
        sel = n - 1;
    offset = MAX(0, MIN(offset, n - 1));
    rev = si.fsrev;
}
static void go(const char *p)
{
    Dirent d;
    if (sc(S_STAT, p, &d, 0, 0) < 0 || !d.dir)
        return;
    if (!trav && (hp < 0 || !eq(p, trail[hp]))) {
        if (hp == 11) {
            memmove(trail, trail + 1, 11 * PATHLEN);
            hp--;
        }
        hp++;
        strset(trail[hp], p, PATHLEN);
        hn = hp + 1;
    }
    static int personalView;
    if (bootpath(p) && !bootpath(cwd)) {
        personalView = listview;
        listview = 1;
    } else if (!bootpath(p) && bootpath(cwd))listview = personalView;
    strset(cwd, p, PATHLEN);
    sel = -1;
    prefix[0] = 0;
    offset = 0;
    note[0] = 0;
    reload();
    ui_motion(FX_SLIDE);
    trace(cwd);
}
static void historygo(int d)
{
    int p = hp + d;
    if (p >= 0 && p < hn) {
        trav = 1;
        hp = p;
        go(trail[p]);
        trav = 0;
    }
}
static const char *display(Dirent *f)
{
    int a = bundle(f);
    return a ? appname(a) : eq(cwd, HOME) || eq(cwd, "/") ? pretty(f->name) : f->name;
}
static void pathsel(char *p)
{
    if (sel >= 0 && sel < n)
        absolutepath(p, cwd, files[sel].name);
    else
        p[0] = 0;
}
static void openfile(void)
{
    if (sel < 0)
        return;
    char p[PATHLEN];
    pathsel(p);
    int ap=bundle(files+sel);if(ap){sc(S_SPAWN,ap,0,0,0);return;}
    if (files[sel].dir)
        go(p);
    else if (bootpath(p)) {
        dialog = 7;
        inspect_open(p, files + sel);
    } else {
        int a = bundle(files + sel);
        sc(S_SPAWN, a >= 1 && a <= LASTAPP ? a : TEXTEDIT, a >= 1 && a <= LASTAPP ? 0 : p, 0, 0);
    }
}
static void act(int k)
{
    char p[PATHLEN];
    pathsel(p);
    ui_motion(FX_SHEET);
    note[0] = 0;
    if(k==ACT_FILE)k=30;
    if(k==ACT_PACKAGE){if(sel>=0&&bundle(files+sel))go(p);return;}
    if (k == ACT_SYSTEM) {
        go(KERNELDIR);
        return;
    }
    if (bootpath(cwd)) {
        if (k == ACT_NEW || k == 30 || k == ACT_RENAME || k == ACT_DELETE || k == ACT_PASTE) {
            strset(note, "Il volume di avvio è in sola lettura.", 96);
            return;
        }
        if ((k == ACT_PREVIEW || k == ACT_PROPS) && sel >= 0 && sel < n && !files[sel].dir) {
            dialog = 7;
            inspect_open(p, files + sel);
            return;
        }
    }
    if (k == ACT_PREVIEW && sel >= 0 && sel < n) {
        dialog = 6;
        preview_read(p, files + sel, idico(files + sel));
        return;
    }
    if (k == ACT_VIEW) {
        listview = !listview;
        offset = 0;
    } else if (k == ACT_PARENT) {
        char s[PATHLEN];
        strset(s, cwd, PATHLEN);
        parentpath(s);
        go(s);
    } else if (k == ACT_OPEN)
        openfile();
    else if (k == ACT_NEW || k == 30) {
        dialog = k == ACT_NEW ? 1 : 2;
        field[0] = 0;
    } else if (k == ACT_RENAME && sel >= 0) {
        dialog = 3;
        strset(field, files[sel].name, sizeof field);
    } else if (k == ACT_DELETE && sel >= 0) {
        dialog = 4;
        strset(field, files[sel].name, sizeof field);
    } else if (k == ACT_COPY && sel >= 0 && !files[sel].dir) {
        strset(clipfile, p, PATHLEN);
        strset(note, "File copiato", 96);
    } else if (k == ACT_PASTE && clipfile[0]) {
        char name[64] = "Copia di ";
        append(name, leaf(clipfile), sizeof name);
        if (absolutepath(p, cwd, name) < 0 || copyfile(clipfile, p) < 0)
            strset(note, "Impossibile incollare: verifica nome e cartella", 96);
        else
            strset(note, "Copia creata", 96);
        reload();
    } else if (k == ACT_PROPS && sel >= 0) {
        dialog = 5;
        strset(field, files[sel].name, sizeof field);
    }
}
static int hasslash(const char *s)
{
    for (; *s; s++)
        if (*s == '/')
            return 1;
    return 0;
}
static void confirm(void)
{
    char a[PATHLEN], b[PATHLEN];
    int r = -1;
    pathsel(a);
    if (dialog == 4)
        r = sc(S_REMOVE, a, 0, 0, 0);
    else if (dialog == 5)
        r = 0;
    else if (field[0] && !eq(field, ".") && !eq(field, "..") && !hasslash(field)
             && absolutepath(b, cwd, field) == 0) {
        if (dialog == 3)
            r = sc(S_MOVE, a, b, 0, 0);
        else
            r = sc(S_CREATE, b, dialog == 1, 0, 0);
    }
    strset(note, r < 0 ? "Verifica nome, permessi e contenuto della cartella" : "Fatto", 96);
    dialog = 0;
    ui_motion(FX_DISSOLVE);
    reload();
}
static void paint(void)
{
    begin();
    int S = side(), x = S + 20, left = S + 15;
    Box(0, 0, S, ah, 0, PANEL);
    Box(S + 1, 0, aw - S - 1, ah, 0, CARD);
    Box(S, 0, 1, ah, 0, EDGE);
    Text(16, 14, "Preferiti", F_SMALL, SUB);
    for (int i = 0; i < 9; i++) {
        int y = ny(i);
        if (y + 26 > ah - 26)
            continue;
        if (eq(cwd, paths[i]) || (i == 7 && eq(cwd,KERNELDIR)))
            Box(8, y - 2, S - 16, 26, 6, SEL);
        Icon(16, y + 1, 19, navicon[i]);
        Text(43, y + 1, navs[i], F_SMALL, eq(cwd, paths[i]) ? EMP : TX);
    }
    if (ah > 250)
        Text(16, 198, "Posizioni", F_SMALL, SUB);
    const char *head = eq(cwd, HOME) ? "Inizio" : eq(cwd, "/") ? (si.live_cd?"tinymacOS Live":"Macintosh HD") : pretty(leaf(cwd));
    Text(x, 29, head, (int)strlen(head) > 26 || aw < 570 ? F_CAPTION : F_TITLE, TX);
    button(x, 62, 28, "←", 0);
    button(x + 33, 62, 28, "→", 0);
    char brief[96];int fit=MAX(18,(aw-x-92)/7);if(strlen(cwd)>(size_t)fit){strset(brief,"…/",96);append(brief,leaf(cwd),96);}else strset(brief,cwd,96);Text(x + 73, 67, brief, F_SMALL, SUB);
    button(aw - 97, 21, 78, listview ? "Griglia" : "Elenco", 0);
    Box(x, 95, aw - x - 19, 1, 0, EDGE);
    if (listview) {
        Box(left, 94, aw - left - 12, 1, 0, EDGE);
    }
    int cw = (aw - left - 12) / cols();
    int vis = visible();
    for (int j = 0; j < vis && j + offset < n; j++) {
        int i = j + offset;
        if (listview) {
            int y = 102 + j * 31;
            if (i == sel)
                Box(left, y, aw - left - 12, 29, 5, SEL);
            else if (j & 1)Box(left, y, aw - left - 12, 29, 5, SOFT);
            Icon(x + 4, y + 2, 24, idico(files + i));
            Text(x + 38, y + 5, display(files + i), F_SMALL, TX);
            if (aw > 695)Text(aw - 211, y + 6, bundle(files+i)?"Applicazione":bootpath(cwd) ? syskind(files + i) : files[i].dir ? "Cartella" : "Documento",
                                  F_SMALL, SUB);
            char z[32] = "–";
            if (!files[i].dir)sizebadge(z, files[i].size, 32);
            Text(aw - 94, y + 6, z, F_SMALL, SUB);
        } else {
            int xx = left + (j % cols()) * cw, yy = 103 + (j / cols()) * 82;
            if (i == sel)
                Box(xx + 2, yy, cw - 4, 76, 7, SEL);
            Icon(xx + cw / 2 - 27, yy + 1, 54, idico(files + i));
            char shortname[24];
            strset(shortname, display(files + i), MIN(19, cw / 8 + 1));
            if (strlen(display(files + i)) > strlen(shortname) && strlen(shortname) > 2) {
                int l = strlen(shortname);
                shortname[l - 2] = '.';
                shortname[l - 1] = '.';
            }
            Center(xx + cw / 2, yy + 56, shortname, F_SMALL, TX);
        }
    }
    if (!n) {
        Text(x, 123, "Questa cartella è vuota.", F_BODY, SUB);
        if(!bootpath(cwd))Text(x, 151, "Crea una cartella o un file qui sotto.", F_SMALL, SUB);
    }
    int fy = ah - 36;
    Box(S + 1, fy - 2, aw - S - 1, 1, 0, EDGE);
    if (bootpath(cwd)) {
        Icon(x - 4, fy + 4, 20, 33);
        Text(x + 24, fy + 6, "Supporto di avvio   ·   Sola lettura", F_SMALL, SUB);
    } else {
        button(x - 5, fy + 1, 78, "+ Cartella", 0);
        button(x + 79, fy + 1, 62, "+ File", 0);
    }
    char stat[96];
    if (note[0])
        strset(stat, note, 54);
    else {
        stat[0] = 0;
        addnum(stat, n, 96);
        append(stat, n == 1 ? " elemento" : " elementi", 96);
        if (n > vis)
            append(stat, " · scorri con le frecce", 96);
    }
    if (aw - x > 300)
        Text(bootpath(cwd) ? aw - 102 : x + 154, fy + 7, stat, F_SMALL, SUB);
    if (dialog == 7)inspect_draw();
    else if (dialog == 6)preview_draw();
    else if (dialog) {
        char details[90] = "Scegli un nome non ancora usato.";
        if (dialog == 4)
            strset(details, "L’elemento selezionato verrà rimosso.", 90);
        if (dialog == 5 && sel >= 0) {
            strset(details, files[sel].dir ? "Cartella  ·  " : "File  /  ", 90);
            addnum(details, files[sel].size, 90);
            append(details, files[sel].ro ? " byte  ·  Sola lettura" : " byte  ·  Lettura e scrittura", 90);
        }
        modal(dialog == 1 ? "Nuova cartella" : dialog == 2 ? "Nuovo file" : dialog == 3 ? "Rinomina" : dialog == 4 ?
              "Eliminare questo elemento?" : "Informazioni", field, details);
    }
    finish();
}
static int entryat(int x,int y){int left=side()+15;if(x<left||x>=aw-12||y<102||y>=ah-36)return -1;
    int cw=(aw-left-12)/cols(),col=(x-left)/MAX(1,cw);if(!listview&&(col>=cols()||y<103))return -1;
    int j=listview?(y-102)/31:(y-103)/82*cols()+col;return j>=0&&j<visible()&&j+offset<n?j+offset:-1;}
static void context(Event*e){if(dialog)return;int x=e->a,y=e->b;int sidebar=x<side();if(sidebar){int hit=0;for(int j=0;j<9;j++)if(y>=ny(j)-2&&y<ny(j)+25&&y<ah-26){go(paths[j]);hit=1;break;}if(!hit)return;}
    sel=sidebar?-1:entryat(x,y);Dirent wd;int writable=!sc(S_STAT,cwd,&wd,0,0)&&!wd.ro;
    unsigned m=AMASK(ACT_VIEW);if(sel>=0){m|=AMASK(ACT_OPEN)|AMASK(ACT_PROPS);if(!files[sel].dir)m|=AMASK(ACT_PREVIEW);if(bundle(files+sel))m|=AMASK(ACT_PACKAGE);
        if(!files[sel].dir&&!bootpath(cwd))m|=AMASK(ACT_COPY);if(writable&&!files[sel].ro)m|=AMASK(ACT_RENAME)|AMASK(ACT_DELETE);
    }else if(writable)m|=AMASK(ACT_NEW)|AMASK(ACT_FILE);
    if(writable&&clipfile[0])m|=AMASK(ACT_PASTE);sc(S_CONTEXT,m,e->d,0,0);}
static void event(Event *e)
{
    if(e->type==EV_CONTEXT){context(e);return;}
    if (e->type == EV_SCROLL) {
        if (dialog == 7) inspect_nav(e->a>0 ? K_UP : K_DOWN);
        else if (!dialog) offset = MAX(0, MIN(MAX(0,n-visible()), offset-e->a*(listview ? 3 : cols())));
        return;
    }
    if (baseevent(e))
        return;
    if (e->type == EV_CLOSE) {
        trace("window closed");
        quit();
    }
    if (dialog == 7) {
        if ((e->type == EV_KEY && (e->a == 27 || e->a == ' ' || e->a == '\n')) || (e->type == EV_CLICK &&
                inspect_click(e->a, e->b))) {
            inspect_close();
            dialog = 0;
            ui_motion(FX_DISSOLVE);
        } else if (e->type == EV_KEY)inspect_nav(e->a);
        return;
    }
    if (dialog == 6) {
        if (e->type == EV_KEY && (e->a == ' ' || e->a == 27 || e->a == '\n')) {
            dialog = 0;
            ui_motion(FX_DISSOLVE);
            if (e->a == '\n')openfile();
        }
        return;
    }
    if (dialog) {
        if (e->type != EV_KEY)
            return;
        int k = e->a;
        if (k == 27)
            dialog = 0;
        else if (k == '\n')
            confirm();
        else if (dialog <= 3) {
            text_append_key(field,48,k);
        }
        return;
    }
    if (e->type == EV_ACTION) {
        act(e->a);
        return;
    }
    if (e->type == EV_KEY) {
        int k = e->a;
        if (e->b) {
            if (e->c && (k == 'K' || k == 'k')) {
                go(KERNELDIR);
                return;
            }
            if (k == '[' || k == ']')historygo(k == '[' ? -1 : 1);
            if (k == 'n' || k == 'N')act(e->c ? 30 : ACT_NEW);
            if (k == 'c')
                act(ACT_COPY);
            if (k == 'v')
                act(ACT_PASTE);
            if (k == 'r')
                act(ACT_RENAME);
            if (k == 'i')
                act(ACT_PROPS);
            return;
        }
        if (k == ' ') {
            act(ACT_PREVIEW);
            return;
        }
        if (text_printable(k) && k != 32 && n) {
            u64 now = sc(S_TICKS, 0, 0, 0, 0);
            if (now - prefixTick > 80)prefix[0] = 0;
            prefixTick = now;
            text_append_key(prefix,sizeof prefix,k>='A'&&k<='Z'?k+32:k);
            for (int z = 0; z < n; z++) {
                const char *s = files[z].name;
                int j = 0;
                while (prefix[j] && s[j]) {
                    char c = s[j];
                    if (c >= 'A' && c <= 'Z')c += 32;
                    if (c != prefix[j])break;
                    j++;
                }
                if (!prefix[j]) {
                    sel = z;
                    if (sel < offset)offset = sel;
                    if (sel >= offset + visible())offset = sel - visible() + 1;
                    ui_motion(FX_SELECT);
                    break;
                }
            }
            return;
        }
        prefix[0] = 0;
        if (k == 8)
            act(ACT_PARENT);
        else if (k == '\n')
            act(ACT_OPEN);
        else if (k == K_DEL)
            act(ACT_DELETE);
        else if (k == K_UP || k == K_DOWN || k == K_LEFT || k == K_RIGHT) {
            if (!n)return;
            int step = listview ? 1 : cols();
            sel = MAX(0, MIN(n - 1, sel + (k == K_UP ? -step : k == K_DOWN ? step : k == K_LEFT ? -1 : 1)));
            if (sel < offset)
                offset = sel;
            if (sel >= offset + visible())
                offset = sel - visible() + 1;
        }
        return;
    }
    if (e->type != EV_CLICK)
        return;
    int x = e->a, y = e->b, S = side();
    if (x < S) {
        for (int i = 0; i < 9; i++)
            if (y >= ny(i) - 2 && y < ny(i) + 25 && y < ah - 26) {
                go(paths[i]);
                return;
            }
    } else if (y >= ah - 35) {
        if (bootpath(cwd))return;
        if (x >= S + 15 && x < S + 95)
            act(ACT_NEW);
        else if (x >= S + 97 && x < S + 164)
            act(30);
    } else if (y >= 60 && y <= 90 && x >= S + 20 && x < S + 83) {
        historygo(x < S + 51 ? -1 : 1);
    } else if (x >= aw - 99 && y > 13 && y < 53)
        act(ACT_VIEW);
    else if (y >= 102) {
        int colw = (aw - S - 27) / cols();
        int j = listview ? (y - 102) / 31 : (y - 103) / 82 * cols() + (x - (S + 15)) / MAX(1, colw);
        if (j >= 0 && j < visible() && j + offset < n) {
            sel = j + offset;
            ui_motion(FX_SELECT);
            if (e->c)
                openfile();
        }
    }
}
void app_main(void)
{
    win(782, 392, "Finder");
    sc(S_STARTARG, cwdarg, 0, 0, 0);
    if(cwdarg[0]=='@'&&cwdarg[2]==':'&&cwdarg[3]=='/'){
        char path[PATHLEN];strset(path,cwdarg+3,PATHLEN);int cmd=cwdarg[1];
        if(cmd=='n'){go(path);act(ACT_NEW);}else{char name[48];strset(name,leaf(path),48);parentpath(path);go(path);
            for(int j=0;j<n;j++)if(eq(files[j].name,name)){sel=j;break;}
            if(sel>=0)act(cmd=='i'?ACT_PROPS:cmd=='r'?ACT_RENAME:ACT_DELETE);}
    }else go(cwdarg[0] ? cwdarg : HOME);
    reload();
    paint();
    for (;;) {
        int dirty = 0;
        Event e;
        sc(S_INFO, &si, 0, 0, 0);
        if (si.fsrev != rev) {
            reload();
            dirty = 1;
        }
        while (sc(S_EVENT, &e, 0, 0, 0) > 0) {
            event(&e);
            dirty = 1;
        }
        if (dialog == 7 && inspect_step())dirty = 1;
        if (dirty)
            paint();
        slp(3);
    }
}
