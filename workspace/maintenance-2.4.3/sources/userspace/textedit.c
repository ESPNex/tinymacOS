#include "ui.h"
static char doc[MAXFILE + 1], board[MAXFILE + 1], path[PATHLEN], field[PATHLEN], status[96];
static int length, cursor, dirty, dialog, replace, firstline;
static int starts[MAXFILE + 2], nlines;
static void layout(void)
{
    int width = MAX(20, MIN(88, (aw - 67) / 8));
    int n = 0, col = 0;
    starts[n++] = 0;
    for (int i = 0; i < length; i++) {
        if (doc[i] == '\n') {
            starts[n++] = i + 1;
            col = 0;
        } else if (++col >= width && i + 1 < length && doc[i + 1] != '\n') {
            starts[n++] = i + 1;
            col = 0;
        }
    }
    starts[n] = length;
    nlines = n;
}
static int cursorline(void)
{
    int j = 0;
    while (j + 1 < nlines && starts[j + 1] <= cursor)
        j++;
    return j;
}
static int endline(int j)
{
    int e = starts[j + 1];
    if (e > starts[j] && doc[e - 1] == '\n')
        e--;
    return e;
}
static void saved(void)
{
    int r = writefile(path, doc, length);
    if (r >= 0) {
        dirty = 0;
        strset(status, si.persistent ? "Salvato su disco" : "Salvato nella sessione live", 96);
        trace(status);
    } else
        strset(status, "Salvataggio non riuscito. Controlla cartella e permessi.", 96);
}
static int load(const char *p)
{
    Dirent d;
    if (sc(S_STAT, p, &d, 0, 0) < 0 || d.dir || d.size > MAXFILE)
        return -1;
    int r = readfile(p, doc, MAXFILE);
    if (r < 0)
        return -1;
    length = r;
    doc[r] = 0;
    cursor = 0;
    dirty = 0;
    firstline = 0;
    strset(path, p, sizeof path);
    strset(status, "Documento aperto", 96);
    trace(p);
    return 0;
}
static void blank(void)
{
    length = cursor = dirty = firstline = 0;
    doc[0] = 0;
    strset(path, DOCS "/Untitled.txt", PATHLEN);
    Dirent di;
    for (int j = 2; j < NSLOTS + 3 && sc(S_STAT, path, &di, 0, 0) == 0; j++) {
        strset(path, DOCS "/Untitled-", PATHLEN);
        addnum(path, j, PATHLEN);
        append(path, ".txt", PATHLEN);
    }
    strset(status, "Nuovo documento", 96);
}
static void prompt(int m)
{
    dialog = m;
    ui_motion(FX_SHEET);
    replace = 1;
    strset(field, path, sizeof field);
}
static void paste(const char *s, int n)
{
    if (n > MAXFILE - length) {
        strset(status, "Limite documento: 8192 byte", 96);
        return;
    }
    memmove(doc + cursor + n, doc + cursor, length - cursor + 1);
    memcpy(doc + cursor, s, n);
    length += n;
    cursor += n;
    dirty = 1;
    status[0] = 0;
}
static void act(int a)
{
    if (a == ACT_SAVE)
        saved();
    else if (a == ACT_OPEN)
        prompt(dirty ? 6 : 2);
    else if (a == ACT_NEW) {
        if (dirty) {
            dialog = 5;
            ui_motion(FX_SHEET);
        } else
            blank();
    } else if (a == ACT_COPY) {
        memcpy(board, doc, length + 1);
        strset(status, "Testo copiato negli appunti di TextEdit", 96);
    } else if (a == ACT_PASTE)
        paste(board, strlen(board));
}
static void dk(int k)
{
    if (k == 27) {
        dialog = 0;
        ui_motion(FX_DISSOLVE);
        return;
    }
    if (k == '\n') {
        int m = dialog;
        dialog = 0;
        ui_motion(FX_DISSOLVE);
        if (m == 3) {
            quit();
            return;
        }
        if (m == 5) {
            blank();
            return;
        }
        if (m == 6) {
            prompt(2);
            return;
        }
        char p[PATHLEN];
        if (absolutepath(p, DOCS, field) < 0) {
            strset(status, "Percorso non valido", 96);
            return;
        }
        if (m == 1) {
            char old[PATHLEN];
            strset(old, path, PATHLEN);
            strset(path, p, PATHLEN);
            saved();
            if (dirty)
                strset(path, old, PATHLEN);
        } else if (m == 2 && load(p) < 0)
            strset(status, "Impossibile aprire il documento", 96);
        return;
    }
    if (dialog >= 3)
        return;
    if (replace && (k == 8 || (k >= 32 && k < 127))) {
        field[0] = 0;
        replace = 0;
    }
    int l = strlen(field);
    if (k == 8 && l)
        field[l - 1] = 0;
    else if (k >= 32 && k < 127 && l < PATHLEN - 1) {
        field[l] = k;
        field[l + 1] = 0;
    }
}
static void key(Event *e)
{
    int k = e->a;
    if (dialog) {
        dk(k);
        return;
    }
    if (e->b) {
        if ((k == 's' || k == 'S') && e->c)
            prompt(1);
        else if (k == 's')
            saved();
        else if (k == 'o')
            act(ACT_OPEN);
        else if (k == 'n')
            act(ACT_NEW);
        else if (k == 'c')
            act(ACT_COPY);
        else if (k == 'v')
            act(ACT_PASTE);
        return;
    }
    if (k == 8 && cursor) {
        memmove(doc + cursor - 1, doc + cursor, length - cursor + 1);
        length--;
        cursor--;
        dirty = 1;
    } else if (k == K_DEL && cursor < length) {
        memmove(doc + cursor, doc + cursor + 1, length - cursor);
        length--;
        dirty = 1;
    } else if (k == K_LEFT)
        cursor = MAX(0, cursor - 1);
    else if (k == K_RIGHT)
        cursor = MIN(length, cursor + 1);
    else if (k == K_HOME || k == K_END || k == K_UP || k == K_DOWN) {
        layout();
        int j = cursorline(), col = cursor - starts[j];
        if (k == K_HOME)
            cursor = starts[j];
        else if (k == K_END)
            cursor = endline(j);
        else {
            j = MAX(0, MIN(nlines - 1, j + (k == K_UP ? -1 : 1)));
            cursor = MIN(starts[j] + col, endline(j));
        }
    } else if (k == '\n' || (k >= 32 && k < 127)) {
        char ch = k;
        paste(&ch, 1);
    } else if (k == '\t')
        paste("    ", 4);
}
static void paint(void)
{
    layout();
    begin();
    title(dirty ? "* " : "", leaf(path));
    int nrows = MAX(2, (ah - 115) / 20), j = cursorline();
    if (j < firstline)
        firstline = j;
    if (j >= firstline + nrows)
        firstline = j - nrows + 1;
    Box(0, 0, aw, 47, 0, PANEL);
    button(13, 10, 55, "Nuovo", 0);
    button(75, 10, 57, "Apri", 0);
    button(139, 10, 60, "Salva", 1);
    button(206, 10, 91, "Salva come", 0);
    Text(aw - 117, 16, "Testo semplice", F_SMALL, SUB);
    Box(0, 47, aw, 1, 0, EDGE);
    Box(0, 48, aw, ah - 82, 0, CARD);
    Box(0, ah - 34, aw, 1, 0, EDGE);
    for (int i = firstline; i < nlines && i < firstline + nrows; i++) {
        char s[96];
        int n = MIN(94, endline(i) - starts[i]);
        memcpy(s, doc + starts[i], n);
        s[n] = 0;
        Text(28, 70 + (i - firstline) * 20, s, F_MONO, TX);
    }
    Box(28 + 8 * (cursor - starts[j]), 71 + 20 * (j - firstline), 2, 16, 0, ACC);
    char z[96] = "Riga ";
    addnum(z, j + 1, 96);
    append(z, "   /   ", 96);
    addnum(z, length, 96);
    append(z, " byte", 96);
    Text(15, ah - 25, status[0] ? status : dirty ? "Modifiche non salvate" : "Pronto", F_SMALL, dirty ? EMP : SUB);
    Text(aw - 165, ah - 25, z, F_SMALL, SUB);
    if (dialog) {
        const char *t = dialog == 1 ? "Salva come" : dialog == 2 ? "Apri documento" : dialog == 3 ?
                        "Chiudere senza salvare?" : dialog == 5 ? "Creare un nuovo documento?" : "Aprire un altro documento?";
        modal(t, dialog <= 2 ? field : leaf(path),
              dialog <= 2 ? "Nome in Documenti, oppure un percorso assoluto." : "Le modifiche non salvate andranno perse.");
    }
    finish();
}
void app_main(void)
{
    win(618, 352, "TextEdit");
    blank();
    char s[PATHLEN];
    sc(S_STARTARG, s, 0, 0, 0);
    if (s[0] && load(s) < 0) {
        Dirent di;
        if (sc(S_STAT, s, &di, 0, 0) == 0)strset(status, "Apri i file di sistema con Vista rapida nel Finder.", 96);
        else {
            strset(path, s, PATHLEN);
            strset(status, "Nuovo file nel percorso scelto", 96);
        }
    }
    paint();
    for (;;) {
        Event e;
        int redraw = 0;
        while (sc(S_EVENT, &e, 0, 0, 0) > 0) {
            redraw = 1;
            if (baseevent(&e))
                continue;
            if (e.type == EV_CLOSE) {
                if (dirty) {
                    dialog = 3;
                    ui_motion(FX_SHEET);
                } else {
                    trace("window closed");
                    quit();
                }
            }
            if (e.type == EV_KEY)
                key(&e);
            if (!dialog && e.type == EV_ACTION)
                act(e.a);
            if (!dialog && e.type == EV_CLICK) {
                if (e.b < 46) {
                    if (e.a < 70)
                        act(ACT_NEW);
                    else if (e.a < 135)
                        act(ACT_OPEN);
                    else if (e.a < 202)
                        act(ACT_SAVE);
                    else if (e.a < 298)
                        prompt(1);
                } else if (e.b >= 67 && e.b < ah - 33) {
                    int j = MIN(nlines - 1, MAX(0, firstline + (e.b - 67) / 20));
                    cursor = MIN(endline(j), starts[j] + MAX(0, (e.a - 24) / 8));
                }
            }
        }
        if (redraw)
            paint();
        slp(2);
    }
}
