#include "ui.h"
#include "vaultview.h"
static int vault_open;
// Impostazioni di Sistema, layout "System Settings" di macOS: colonna
// laterale cercabile a sinistra, schede raggruppate a destra. Ogni controllo
// è collegato a una API reale (S_PREF, S_SNAP, S_INFO, S_SESSION).

static int pane, focus_search;
static int res_open,res_pick,res_scroll;
enum { RES_ROWS=8, RES_ROW=24, RES_PAD=6 };
static char query[24];
static int modal_mode, modal_op, modal_err; // 0 nessuno, 1 passphrase, 2 conferma
static char modal_buf[65], msg[72];
static SnapInfo snaps[8];
static int nsn, sel_snap = -1;
static FsInfo fi;
static EncInfo ei;
static SessionInfo acct;

static const char *const pname[] = {"Aspetto", "Schermo", "Sicurezza e privacy", "Backup", "Data e ora", "Generali"};
static const int pglyph[] = {ICON_SETTINGS_APPEARANCE, ICON_SETTINGS_DISPLAY,
    ICON_SETTINGS_PRIVACY, ICON_SETTINGS_BACKUP, ICON_SETTINGS_CLOCK, ICON_SETTINGS_GENERAL};
#define NP 6

typedef struct { int x, y, w, h; } R;
static R hresmenu;
static R hsearch, hpane[NP], hlight[2], hcol[NTHEME], hwall[NWALL], hsw[4], henc[4], hbk[4], hsnap[5], hres;
static int nhpane, nenc, nbk, nsnapshow, hitpane[NP];
static const char *enclab[4];
static int encact[4], encpass[4];

static int hit(R r, int x, int y)
{
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}
static int match(const char *n)
{
    if (!query[0])return 1;
    for (const char *p = n; *p; p++) {
        const char *a = p, *b = query;
        while (*a && *b && ((*a | 32) == (*b | 32)))a++, b++;
        if (!*b)return 1;
    }
    return 0;
}
static void setpref(int n, int v)
{
    sc(S_PREF, n, v, 0, 0);
    sc(S_INFO, &si, 0, 0, 0);
    ui_motion(FX_SELECT);
}
static void refresh(void)
{
    sc(S_SESSION, &acct, 0, 0, 0);
    if (sc(S_SNAP, 6, 0, 0, &ei))memset(&ei, 0, sizeof ei);
    nsn = sc(S_SNAP, 0, 0, snaps, 8);
    if (nsn < 0 || sc(S_SNAP, 5, 0, &fi, 0)) {
        nsn = 0;
        memset(&fi, 0, sizeof fi);
    }
    if (sel_snap >= nsn)sel_snap = -1;
}
static void setpane(int p)
{
    if(si.recovery||!vault_view_can_leave())return;
    vault_view_leave();pane = p;res_open=0;vault_open=0;
    focus_search = 0;
    if (p == 2 || p == 3)refresh();
    ui_motion(FX_SLIDE);
}
static void sw(R r, int on)
{
    Box(r.x, r.y, r.w, r.h, r.h / 2, on ? 0x34c759 : 0xb9b9be);
    int k = r.h - 6;
    Box(on ? r.x + r.w - k - 3 : r.x + 3, r.y + 3, k, k, k / 2, 0xffffff);
}
static int rowcard(int x, int y, int w, int rows)
{
    Box(x, y, w, rows * 40 + 8, 10, PANEL);
    for (int i = 1; i < rows; i++)Line(x + 16, y + 4 + i * 40, x + w - 16, y + 4 + i * 40, EDGE);
    return y + 4;
}
static void row2(int x, int ry, int w, const char *l, const char *v)
{
    Text(x + 16, ry + 11, l, F_SMALL, TX);
    if (v)Text(x + 214, ry + 11, v, F_SMALL, SUB);
}
static void clockstr(char *s, int cap)
{
    char n[16];
    s[0] = 0;
    if (si.hour < 10)append(s, "0", cap);
    number(n, si.hour);
    append(s, n, cap);
    append(s, ":", cap);
    if (si.minute < 10)append(s, "0", cap);
    number(n, si.minute);
    append(s, n, cap);
    append(s, ":", cap);
    if (si.second < 10)append(s, "0", cap);
    number(n, si.second);
    append(s, n, cap);
}
static void datestr(char *s, int cap)
{
    char n[16];
    s[0] = 0;
    number(n, si.day);
    append(s, n, cap);
    append(s, " / ", cap);
    number(n, si.month);
    append(s, n, cap);
    append(s, " / ", cap);
    number(n, si.year);
    append(s, n, cap);
}
static void backupname(char *s, int cap)
{
    char n[16];
    s[0] = 0;
    append(s, "Backup-", cap);
    number(n, si.year);
    append(s, n, cap);
    append(s, "-", cap);
    if (si.month < 10)append(s, "0", cap);
    number(n, si.month);
    append(s, n, cap);
    append(s, "-", cap);
    if (si.day < 10)append(s, "0", cap);
    number(n, si.day);
    append(s, n, cap);
}

// Mini desktop preview used by the Light/Dark cards (Apple Appearance style).
static void preview(int x, int y, int w, int h, int dark)
{
    u32 bg = dark ? 0x17171b : 0xf3f4f8, panel = dark ? 0x26262c : 0xffffff,
        bar = dark ? 0x2e2e35 : 0xe9eaf0, line = dark ? 0x3d3d46 : 0xd8d9e0;
    Box(x, y, w, h, 8, bg);
    Box(x, y, w, 9, 8, bar);
    int wx = x + 26, wy = y + 17, ww = w - 52, wh = h - 32;
    Box(wx, wy, ww, wh, 5, panel);
    Box(wx, wy, ww, 11, 5, bar);
    Box(wx + 6, wy + 4, 4, 4, 2, 0xff5f57);
    Box(wx + 13, wy + 4, 4, 4, 2, 0xfebc2e);
    Box(wx + 20, wy + 4, 4, 4, 2, 0x28c840);
    Box(wx + 8, wy + 17, ww - 34, 4, 2, line);
    Box(wx + 8, wy + 25, ww - 20, 4, 2, line);
    Box(wx + 8, wy + 33, (ww - 20) / 2, 4, 2, ACC);
    Box(x + w / 2 - 40, y + h - 8, 80, 5, 3, dark ? 0x3a3a44 : 0xdfe0e8);
}
static void paint(void)
{
    begin();
    int S = 208, x = S + 18, w = aw - x - 18;
    Box(0, 0, S, ah, 0, PANEL);
    Box(S, 0, aw - S, ah, 0, CARD);
    Box(S, 0, 1, ah, 0, EDGE);
    // Ricerca
    hsearch = (R) {10, 12, S - 20, 28};
    if (focus_search)Box(8, 10, S - 16, 32, 9, ACC);
    Box(hsearch.x, hsearch.y, hsearch.w, hsearch.h, 8, focus_search ? CARD : SOFT);
    Text(20, 19, query[0] ? query : "Cerca", F_SMALL, query[0] ? TX : SUB);
    // Voci filtrate
    nhpane = 0;
    int y = 56;
    for (int i = 0; i < NP && y + 30 < ah - 8; i++) {
        if ((si.recovery&&i!=2)||!match(pname[i]))continue;
        hpane[nhpane] = (R) {7, y - 3, S - 14, 30};
        hitpane[nhpane] = i;
        nhpane++;
        if (i == pane)Box(7, y - 3, S - 14, 30, 8, SEL);
        static const u32 chip[6] = {0x5865d8, 0x2385e8, 0x687384, 0x2caa76, 0xef922c, 0x788394};
        Box(13, y + 1, 20, 20, 6, chip[i]);
        Icon(14, y + 2, 18, pglyph[i]);
        Text(40, y + 4, pname[i], F_SMALL, i == pane ? EMP : TX);
        y += 34;
    }
    // Intestazione contenuto
    Text(x - 1, 34, pname[pane], F_TITLE, TX);

    if (pane == 0) {
        // Apple "Appearance": Light/Dark cards, accent colour circles, wallpaper grid.
        Box(x, 88, w, 106, 10, PANEL);
        int tw = (w - 48) / 2, th = 64;
        for (int j = 0; j < 2; j++) {
            int xx = x + 16 + j * (tw + 16), yy = 98;
            int on = (!!si.dark) == (j == 1);
            if (on)Box(xx - 3, yy - 3, tw + 6, th + 6, 11, ACC);
            preview(xx, yy, tw, th, j);
            Center(xx + tw / 2, yy + th + 6, j ? "Scuro" : "Chiaro", F_SMALL, on ? EMP : SUB);
            hlight[j] = (R) {xx - 3, yy - 3, tw + 6, th + 26};
        }
        Text(x, 202, "COLORE", F_MICRO, SUB);
        Box(x, 220, w, 46, 10, PANEL);
        for (int j = 0; j < NTHEME; j++) {
            int rx = x + 20 + j * 40, ry = 230;
            if (si.theme == (u32)j)Box(rx - 4, ry - 4, 34, 34, 17, ACC);
            else Box(rx - 1, ry - 1, 28, 28, 14, EDGE);
            Box(rx, ry, 26, 26, 13, accent(j));
            hcol[j] = (R) {rx - 4, ry - 4, 34, 34};
        }
        Text(x + 352, 232, themename(si.theme), F_BOLD, TX);
        Text(x, 274, "SFONDO", F_MICRO, SUB);
        Text(x + 64, 271, wallname(si.wall), F_BOLD, TX);
        Box(x, 292, w, 110, 10, PANEL);
        int ww = 77, wh = 38;
        for (int j = 0; j < NWALL; j++) {
            int xx = x + 18 + (j % 7) * (ww + 6), yy = 302 + (j / 7) * 50;
            Photo(xx, yy, ww, wh, j, 6);
            if (si.wall == (u32)j)Box(xx + 3, yy + wh + 3, ww - 6, 2, 1, ACC);
            hwall[j] = (R) {xx, yy, ww, wh + 7};
        }
        Text(x, 410, "FINESTRA", F_MICRO, SUB);
        int r0 = rowcard(x, 428, w, 2);
        row2(x, r0, w, "Materiale", si.opaque ? "Opaco" : "Trasparente");
        hsw[0] = (R) {x + w - 56, r0 + 7, 38, 22};
        sw(hsw[0], si.opaque);
        row2(x, r0 + 40, w, "Animazioni", si.reduce_motion ? "Ridotte" : "Complete");
        hsw[1] = (R) {x + w - 56, r0 + 47, 38, 22};
        sw(hsw[1], si.reduce_motion);
    } else if (pane == 1) {
        char s[65] = "";
        addnum(s, si.w, 65);
        append(s, " × ", 65);
        addnum(s, si.h, 65);
        int r0 = rowcard(x, 84, w, 4);
        append(s, " >", 65);
        row2(x, r0, w, "RISOLUZIONE", s);
        hres = (R) {x, r0, w, 40};
        row2(x, r0 + 40, w, "USCITA", "Schermo integrato");
        char ap[40];
        strset(ap, themename(si.theme), 40);
        append(ap, si.dark ? " \xc2\xb7 scuro" : " \xc2\xb7 chiaro", 40);
        row2(x, r0 + 80, w, "ASPETTO", ap);
        row2(x, r0 + 120, w, "MATERIALE", si.opaque ? "Opaco, senza rifrazione" : "Vetro traslucido");
        Text(x, 270, "DOCK", F_MICRO, SUB);
        int r1 = rowcard(x, 288, w, 1);
        row2(x, r1, w, "Posizione", si.dock ? "Sinistra" : "In basso");
        hsw[0] = (R) {x + w - 56, r1 + 7, 38, 22};
        sw(hsw[0], si.dock);
        Text(x, 365, "Il dock si sposta tra la sinistra e il bordo inferiore.", F_SMALL, SUB);
        int caps=sc(S_RESMODE,0,1,0,0);
        Text(x,390,(caps&1)?"Cambio risoluzione immediato disponibile.":"Su questa GPU il cambio richiede un riavvio (GOP UEFI).",F_MICRO,SUB);
        if(!(caps&1)&&!(caps&2))Text(x,413,"Avvio live: per salvare la scelta serve un sistema installato.",F_MICRO,SUB);
        if(msg[0])Text(x,338,msg,F_SMALL,TX);
    } else if (pane == 2) {
        if(vault_open){
            vault_view_draw(x,70,w,ah-82);
        }else{
        Text(x,78,"SICUREZZA",F_MICRO,SUB);
        VaultRequest vq={.disk=-1};VaultInfo vinfo={0};sc(S_FILEVAULT,0,&vq,&vinfo,0);
        int r0=rowcard(x,96,w,2);
        row2(x,r0,w,"VOLUME DATI",vinfo.enabled?(vinfo.locked?"Bloccato":"Cifrato"):ei.state?"Vecchia cifratura dei file":"Non cifrato");
        row2(x,r0+40,w,"PROTEZIONE","Dati, nomi e metadati");
        nenc=1;enclab[0]="FileVault…";encact[0]=100;encpass[0]=0;henc[0]=(R){x,196,210,28};
        if(ei.state&&!vinfo.enabled){enclab[1]="Disattiva vecchia cifratura";encact[1]=2;encpass[1]=1;henc[1]=(R){x+222,196,235,28};nenc=2;}
        for(int i=0;i<nenc;i++)button(henc[i].x,henc[i].y,henc[i].w,enclab[i],i==0);
        if(msg[0])Text(x,232,msg,F_SMALL,SUB);
        Text(x, 268, "ACCOUNT", F_MICRO, SUB);
        int r1 = rowcard(x, 286, w, 2);
        row2(x, r1, w, "NOME", acct.name[0] ? acct.name : "—");
        row2(x, r1 + 40, w, "ACCESSO", "All’avvio del sistema");
        Text(x, 390, "Se l’account non è leggibile, il login offre «Reimposta l’account».", F_SMALL, SUB);
        }
    } else if (pane == 3) {
        Text(x, 78, "TIME MACHINE", F_MICRO, SUB);
        int r0 = rowcard(x, 96, w, 3);
        row2(x, r0, w, "STATO", nsn ? "Copia locale salvata" : "Nessuna copia");
        char ns[24] = "";
        addnum(ns, fi.snaps, 24);
        row2(x, r0 + 40, w, "SNAPSHOT", ns);
        char dim[64] = "";
        addnum(dim, fi.used / 1024, 64);
        append(dim, " KB usati", 64);
        row2(x, r0 + 80, w, "DIMENSIONE", dim);
        nbk = 0;
        hbk[nbk] = (R) {x, 236, 130, 26};
        nbk++;
        if (nsn) {
            hbk[nbk] = (R) {x + 142, 236, 120, 26};
            nbk++;
            hbk[nbk] = (R) {x + 274, 236, 100, 26};
            nbk++;
            hbk[nbk] = (R) {x + 386, 236, 100, 26};
            nbk++;
        }
        const char *blab[] = {"Copia ora", "Ripristina", "Verifica", "Elimina"};
        for (int i = 0;i < nbk;i++)button(hbk[i].x, hbk[i].y, hbk[i].w, blab[i], i == 0);
        if (msg[0])Text(x, 272, msg, F_SMALL, SUB);
        nsnapshow = 0;
        if (nsn) {
            Text(x, 306, "SNAPSHOT", F_MICRO, SUB);
            nsnapshow = MIN(nsn, 5);
            int ly = 324, rh = 34;
            Box(x, ly, w, nsnapshow * rh + 8, 10, PANEL);
            for (int i = 0; i < nsnapshow; i++) {
                int ry = ly + 4 + i * rh;
                if (i)Line(x + 16, ry, x + w - 16, ry, EDGE);
                if (i == sel_snap)Box(x + 4, ry + 2, w - 8, rh - 4, 7, SEL);
                Text(x + 16, ry + 8, snaps[i].name, F_SMALL, i == sel_snap ? EMP : TX);
                hsnap[i] = (R) {x, ry, w, rh};
            }
        } else Text(x, 312, "Nessuno snapshot: usa «Copia ora» per la prima copia.", F_SMALL, SUB);
    } else if (pane == 4) {
        char t[24], d[32];
        clockstr(t, 24);
        datestr(d, 32);
        Center(x + w / 2, 72, t, F_CLOCK, TX);   // F_CLOCK is 94px tall: ends at 166
        Center(x + w / 2, 176, d, F_BODY, SUB);  // date now clears the clock
        int r0 = rowcard(x, 216, w, 1);
        row2(x, r0, w, "SORGENTE", "Orologio del computer");
        Text(x, 278, "Data e ora lette dall'orologio del computer.", F_SMALL, SUB);
    } else {
        int r0 = rowcard(x, 84, w, 4);
        row2(x, r0, w, "VERSIONE", "tinymacOS 10.2.5 (30B21)");
        row2(x, r0 + 40, w, "NOME", "Tramontana");
        row2(x, r0 + 80, w, "PROCESSORE", si.cpu);
        row2(x, r0 + 120, w, "ARCHIVIAZIONE", si.persistent ? "Macintosh HD - Dati" : "Volume live temporaneo");
        Text(x, 274, "SPAZIO SU DISCO", F_MICRO, SUB);
        Box(x, 298, w, 12, 6, SOFT);
        u64 fill = fi.total ? (u64)w * fi.used / fi.total : 0;
        if (fill > (u64)w)fill = w;
        if (fill)Box(x, 298, (int)fill, 12, 6, ACC);
        char dim[80] = "";
        addnum(dim, fi.used / 1024, 80);
        append(dim, " KB usati su ", 80);
        addnum(dim, fi.total / 1024, 80);
        append(dim, " KB", 80);
        Text(x, 320, dim, F_SMALL, SUB);
        int r1 = rowcard(x, 360, w, 2);
        row2(x, r1, w, "UTENTE", si.user[0] ? si.user : "—");
        row2(x, r1 + 40, w, "INGRESSO", "Tastiera e mouse USB");
    }
    if (modal_mode == 1) {
        char dots[65];
        int n = (int)strlen(modal_buf);
        for (int i = 0; i < n;i++)dots[i] = '.';
        dots[n] = 0;
        modal("Passphrase di crittografia", n ? dots : "Inserisci la passphrase",
              modal_err ? "Operazione non riuscita: riprova." : "Invio per confermare · Esc per annullare");
    } else if (modal_mode == 2)
        modal("Ripristinare lo snapshot?", sel_snap >= 0 ? snaps[sel_snap].name : "",
              "Invio per confermare · Esc per annullare");
    if(res_open&&pane==1){
        int visible=MIN((int)si.nmodes,RES_ROWS), widest=0;
        for(u32 i=0;i<si.nmodes;i++){
            char label[32]="";addnum(label,si.modes[i]>>16,32);append(label," × ",32);addnum(label,si.modes[i]&65535,32);
            int measured=TextWidth(label,F_SMALL);widest=MAX(widest,measured);
        }
        int width=widest?MAX(132,widest+42):148;
        if(!visible)width=MAX(width,TextWidth("Nessuna modalità GOP",F_SMALL)+20);
        width=MIN(width,aw-16);
        int anchor=MAX(8,MIN(hres.x+214-26,aw-width-8));
        hresmenu=(R){anchor,hres.y+34,width,MAX(1,visible)*RES_ROW+2*RES_PAD};
        Sheet(hresmenu.x,hresmenu.y,hresmenu.w,hresmenu.h,9);
        if(!visible)Text(hresmenu.x+10,hresmenu.y+RES_PAD+2,"Nessuna modalità GOP",F_SMALL,SUB);
        for(int j=0;j<visible;j++){int i=res_scroll+j;if(i>=(int)si.nmodes)break;int yy=hresmenu.y+RES_PAD+j*RES_ROW;
            if(i==res_pick)Box(hresmenu.x+4,yy,hresmenu.w-8,RES_ROW-2,5,SEL);
            if(si.modes[i]==(((u32)si.w<<16)|si.h))Text(hresmenu.x+8,yy+2,"✓",F_SMALL,TX);
            char label[32]="";addnum(label,si.modes[i]>>16,32);append(label," × ",32);addnum(label,si.modes[i]&65535,32);
            Text(hresmenu.x+26,yy+2,label,F_SMALL,TX);
        }
    }
    finish();
}

static void apply_resolution(int i){
    res_open=0;if(i<0||i>=(int)si.nmodes)return;
    long z=sc(S_RESMODE,i,0,0,0);
    strset(msg,z==0?"Risoluzione applicata.":z==1?"Scelta salvata: sara applicata al prossimo riavvio.":z==-2?"Avvio live: risoluzione non salvabile su questo supporto.":"Cambio non riuscito: risoluzione precedente mantenuta.",sizeof msg);
    sc(S_INFO,&si,0,0,0);ui_motion(FX_SELECT);
}
static void click(int ax, int ay)
{
    if (modal_mode)return;
    if(pane==2&&vault_open){
        if(si.recovery){Event e={.type=EV_CLICK,.a=ax,.b=ay};vault_view_event(&e);return;}
        if(ax>=208){Event e={.type=EV_CLICK,.a=ax,.b=ay};vault_view_event(&e);return;}
        if(!vault_view_can_leave())return;
    }
    if(res_open){if(hit(hresmenu,ax,ay)){int j=(ay-hresmenu.y-RES_PAD)/RES_ROW;if(ay>=hresmenu.y+RES_PAD&&j<MIN((int)si.nmodes-res_scroll,RES_ROWS))apply_resolution(res_scroll+j);return;}res_open=0;if(!hit(hres,ax,ay))return;}
    if (hit(hsearch, ax, ay)) {
        focus_search = 1;
        return;
    }
    focus_search = 0;
    for (int i = 0; i < nhpane;i++)if (hit(hpane[i], ax, ay)) {
            setpane(hitpane[i]);
            return;
        }
    if (pane == 0) {
        for (int j = 0; j < 2;j++)if (hit(hlight[j], ax, ay)) {
                setpref(5, j);   // light / dark
                return;
            }
        for (int j = 0; j < NTHEME;j++)if (hit(hcol[j], ax, ay)) {
                setpref(0, j);   // accent colour theme
                return;
            }
        for (int j = 0; j < NWALL;j++)if (hit(hwall[j], ax, ay)) {
                setpref(1, j);
                return;
            }
        if (hit(hsw[0], ax, ay))setpref(4, !si.opaque);
        else if (hit(hsw[1], ax, ay))setpref(3, !si.reduce_motion);
    } else if (pane == 1) {
        if (hit(hres, ax, ay)) {
            res_open=!res_open;res_pick=0;res_scroll=0;
            for(u32 i=0;i<si.nmodes;i++)if(si.modes[i]==(((u32)si.w<<16)|si.h))res_pick=i;
            if(res_pick>=RES_ROWS)res_scroll=res_pick-RES_ROWS+1;
            return;
        }
        if (hit(hsw[0], ax, ay))setpref(2, !si.dock);
    } else if (pane == 2) {
        for (int i = 0; i < nenc;i++)if (hit(henc[i], ax, ay)) {
                msg[0] = 0;
                if(encact[i]==100){vault_open=1;return;}
                if (encpass[i]) {
                    modal_mode = 1;
                    modal_op = encact[i];
                    modal_buf[0] = 0;
                    modal_err = 0;
                } else {
                    int rr = sc(S_SNAP, 6, encact[i], 0, 0);
                    strset(msg, rr ? "Operazione non riuscita." : "Volume bloccato.", sizeof msg);
                    refresh();
                }
                return;
            }
    } else if (pane == 3) {
        for (int i = 0;i < nbk;i++)if (hit(hbk[i], ax, ay)) {
                msg[0] = 0;
                if (i == 0) {
                    char nm[32];
                    backupname(nm, 32);
                    int rr = sc(S_SNAP, 1, nm, 0, 0);
                    strset(msg, rr ? "Copia non riuscita." : "Copia creata e verificata.", sizeof msg);
                } else if (sel_snap >= 0 && sel_snap < nsn) {
                    if (i == 1) {
                        modal_mode = 2;
                        return;
                    }
                    int a = i == 2 ? 3 : 4;
                    int rr = sc(S_SNAP, a, snaps[sel_snap].name, 0, 0);
                    strset(msg, rr ? "Operazione non riuscita." : (i == 2 ? "Snapshot verificato." : "Snapshot eliminato."),
                           sizeof msg);
                } else strset(msg, "Seleziona uno snapshot dalla lista.", sizeof msg);
                refresh();
                return;
            }
        for (int i = 0; i < nsnapshow;i++)if (hit(hsnap[i], ax, ay)) {
                sel_snap = i;
                msg[0] = 0;
                return;
            }
    }
}

static void key(Event *e)
{
    int k = e->a;
    if(pane==2&&vault_open&&!focus_search){vault_view_event(e);return;}
    if(res_open){if(k==27)res_open=0;else if(k=='\n'||k=='\r')apply_resolution(res_pick);else if(k==K_UP||k==K_DOWN){res_pick=MAX(0,MIN((int)si.nmodes-1,res_pick+(k==K_UP?-1:1)));if(res_pick<res_scroll)res_scroll=res_pick;if(res_pick>=res_scroll+RES_ROWS)res_scroll=res_pick-RES_ROWS+1;}return;}
    if (modal_mode == 1) {
        if (k == 27)modal_mode = 0;
        else if (k == '\n') {
            int rr = sc(S_SNAP, 6, modal_op, modal_buf, 0);
            modal_mode = 0;
            strset(msg, rr ? "Operazione non riuscita." : "Crittografia aggiornata.", sizeof msg);
            refresh();
        } else {text_append_key(modal_buf,65,k);modal_err=0;}

        return;
    }
    if (modal_mode == 2) {
        if (k == 27)modal_mode = 0;
        else if (k == '\n') {
            int rr = -1;
            if (sel_snap >= 0 && sel_snap < nsn)rr = sc(S_SNAP, 2, snaps[sel_snap].name, 0, 0);
            modal_mode = 0;
            strset(msg, rr ? "Ripristino non riuscito." : "Volume ripristinato.", sizeof msg);
            refresh();
        }
        return;
    }
    if (focus_search) {
        if (k == 27) {
            query[0] = 0;
            focus_search = 0;
        } else if (k == '\n')focus_search = 0;
        else text_append_key(query,sizeof query,k);
        if (query[0] && !match(pname[pane])) {
            for (int i = 0; i < NP;i++)if (match(pname[i])) {
                    pane = i;
                    if (i == 2 || i == 3)refresh();
                    ui_motion(FX_SLIDE);
                    break;
                }
        }
        return;
    }
    if (k == K_UP || k == K_DOWN)setpane((pane + (k == K_UP ? NP - 1 : 1)) % NP);
}

void app_main(void)
{
    win(860, 540, "Impostazioni di Sistema");
    vault_view_init();
    char arg[PATHLEN];sc(S_STARTARG,arg,0,0,0);if(eq(arg,"@vault")){pane=2;vault_open=1;}
    refresh();
    paint();
    for (;;) {
        Event e;
        int d = 0;
        SysInfo s;
        sc(S_INFO, &s, 0, 0, 0);
        int oldsec = (int)si.second;
        if (s.theme != si.theme || s.wall != si.wall || s.opaque != si.opaque || s.reduce_motion != si.reduce_motion ||
                s.dock != si.dock || s.dark != si.dark || s.w != si.w || s.h != si.h)d = 1;
        si = s;
        while (sc(S_EVENT, &e, 0, 0, 0) > 0) {
            d = 1;
            if (baseevent(&e))continue;
            if (e.type == EV_CLOSE){if(vault_view_can_close())quit();continue;}
            if (e.type == EV_CLICK)click(e.a, e.b);
            if (e.type == EV_KEY)key(&e);
            if(e.type==EV_SCROLL&&res_open){res_scroll=MAX(0,MIN(MAX(0,(int)si.nmodes-RES_ROWS),res_scroll-(e.a>0?1:-1)));res_pick=res_scroll;}
        }
        if (pane == 4 && (int)si.second != oldsec)d = 1;
        if(vault_view_tick())d=1;
        if (d)paint();
        slp(4);
    }
}
