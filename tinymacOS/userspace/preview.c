#include "preview.h"
// Read-only snapshots for Finder's Space-bar preview. Never opens an fd for writing.
static char name[48], info[76], body[643];
static int symbol;
void preview_read(const char *path, const Dirent *file, int icon_id) {
    strset(name, file->name, sizeof name);
    symbol = icon_id;
    body[0] = info[0] = 0;
    if (file->dir) {
        Dirent entries[NSLOTS];
        int n = sc(S_LIST, path, entries, NSLOTS, 0);
        if (n < 0)strset(body, "Lettura non riuscita.", sizeof body);
        else {
            addnum(info, n, sizeof info);
            append(info, n == 1 ? " elemento" : " elementi", sizeof info);
            for (int j = 0; j < n && j < 5; j++) {
                append(body, entries[j].name, sizeof body);
                if (entries[j].dir)append(body, "/", sizeof body);
                append(body, "\n", sizeof body);
            }
            if (!n)strset(body, "Cartella vuota.", sizeof body);
        }
    } else {
        int n = readfile(path, body, 640);
        addnum(info, file->size, sizeof info);
        append(info, " byte", sizeof info);
        if (n <= 0)strset(body, n < 0 ? "Lettura non riuscita." : "File vuoto.", sizeof body);
        else {
            body[n] = 0;
            for (int i = 0; i < n; i++) {
                unsigned char ch = body[i];
                if (ch == '\t' || ch == '\r')body[i] = ' ';
                else if (ch < 32 && ch != '\n') {
                    strset(body, "Nessuna anteprima testuale disponibile.", sizeof body);
                    break;
                }
            }
        }
    }
    append(info, file->ro ? "  ·  Sola lettura" : "  ·  Documento locale", sizeof info);

}
void preview_draw(void) {
    int w = MIN(478, aw - 54), h = MIN(238, ah - 22), x = (aw - w) / 2, y = (ah - h) / 2;
    Sheet(x, y, w, h, 17);
    Icon(x + 17, y + 14, 41, symbol);
    Text(x + 70, y + 12, "VISTA RAPIDA   /   FINDER", F_MICRO, EMP);
    Text(x + 69, y + 31, name, F_CAPTION, TX);
    Box(x + 15, y + 66, w - 30, h - 107, 11, CARD);
    int pos = 0, limit = MAX(12, MIN(70, (w - 62) / 8));
    for (int row = 0; row < (h - 119) / 18 && body[pos]; row++) {
        char s[74];
        int n = 0;
        while (n < limit && body[pos] && body[pos] != '\n')s[n++] = body[pos++];
        s[n] = 0;
        if (body[pos] == '\n')pos++;
        Text(x + 26, y + 77 + row * 18, s, F_MONO, TX);
    }
    Text(x + 18, y + h - 28, info, F_SMALL, SUB);
    Text(x + w - 166, y + h - 27, "Spazio chiude · Invio apre", F_SMALL, EMP);
}
