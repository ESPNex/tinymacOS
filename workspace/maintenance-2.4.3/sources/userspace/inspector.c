#include "volume.h"
#include "preview.h"
// Separate descriptors let the inspector page around immediately while the SHA
// stream runs in bounded slices. Neither reads live kernel memory or a prefilled digest.
static Dirent file;
static char path[PATHLEN], digest[65];
static int fd = -1, stream = -1, binary, total, off, nread, next, last, undo[32], nu;
static u8 page[1024], chunk[MAXFILE];
static SHA256 hash;
static const char *digits = "0123456789abcdef";
void inspect_close(void)
{
    if (fd >= 0)sc(S_CLOSE, fd, 0, 0, 0);
    if (stream >= 0)sc(S_CLOSE, stream, 0, 0, 0);
    fd = stream = -1;
}
static void loadpage(void)
{
    nread = 0;
    if (fd < 0 || sc(S_SEEK, fd, off, 0, 0) < 0)return;
    nread = sc(S_READ, fd, page, sizeof page, 0);
    nread = MAX(0, nread);
}
void inspect_open(const char *p, const Dirent *d)
{
    inspect_close();
    file = *d;
    strset(path, p, sizeof path);
    off = total = nu = 0;
    last = -1;
    digest[0] = 0;
    fd = sc(S_OPEN, p, 0, 0, 0);
    stream = sc(S_OPEN, p, 0, 0, 0);
    sha_init(&hash);
    loadpage();
    binary = (nread>=8&&!memcmp(page,"TMKCACHE",8))||(nread>=4&&*(u32*)page==0xfeedfacf)||(nread>=4&&(!memcmp(page,"qoif",4)||!memcmp(page,"TFN2",4))) || (nread >= 4 && !memcmp(page, "\x7f" "ELF", 4)) || (nread >= 2 && !memcmp(page, "MZ", 2));
    ui_motion(FX_SHEET);
    trace("inspecting boot volume");
    trace(path);
}
int inspect_step(void)
{
    if (stream < 0)return 0;
    for (int j = 0; j < 32; j++) {
        int n = sc(S_READ, stream, chunk, sizeof chunk, 0);
        if (n < 0 || (!n && total != file.size)) {
            strset(digest, "Lettura incompleta", 65);
            break;
        }
        if (!n) {
            u8 h[32];
            sha_final(&hash, h);
            sha_hex(h, digest);
            break;
        }
        sha_update(&hash, chunk, n);
        total += n;
    }
    if (digest[0]) {
        sc(S_CLOSE, stream, 0, 0, 0);
        stream = -1;
        return 1;
    }
    int pct = (u64)total * 100 / MAX(1, file.size) / 10;
    if (last != pct) {
        last = pct;
        return 1;
    }
    return 0;
}
static int height(void)
{
    return MIN(350, ah - 12);
}
static int nrows(void)
{
    return MAX(1, (height() - 202) / 17);
}
static int width(void)
{
    return MIN(589, aw - 28);
}
static int advance(int delta)
{
    int at = 0;
    if (binary)return MIN(nread, delta * 12);
    int cols = MIN(70, (width() - 73) / 8);
    for (int j = 0; j < delta && at < nread; j++) {
        int k = 0;
        while (at < nread && page[at] != '\n' && k < cols) {
            at++;
            k++;
        }
        if (at < nread && page[at] == '\n')at++;
    }
    return at;
}
void inspect_nav(int key)
{
    int step = advance(nrows());
    if (key == K_HOME) {
        off = nu = 0;
    } else if (key == K_END) {
        nu = 0;
        off = MAX(0, file.size - (binary ? 12 * nrows() : 160));
    } else if (key == K_LEFT || key == K_UP) {
        off = nu ? undo[--nu] : MAX(0, off - (binary ? 12 * nrows() : 160));
    } else if (key == K_RIGHT || key == K_DOWN) {
        if (off + step >= file.size || !step)return;
        if (nu == 32) {
            memmove(undo, undo + 1, 31 * sizeof(int));
            nu--;
        }
        undo[nu++] = off;
        off += step;
    } else return;
    loadpage();

}
static void hexval(char *p, u32 value, int digitsn)
{
    for (int i = digitsn - 1; i >= 0; i--) {
        p[i] = digits[value & 15];
        value >>= 4;
    }
    p[digitsn] = 0;
}
void inspect_draw(void)
{
    int w = width(), h = height(), x = (aw - w) / 2, y = (ah - h) / 2;
    Sheet(x, y, w, h, 15);
    Icon(x + 16, y + 14, 40, binary ? 32 : 7);
    Text(x + 70, y + 12, file.name, F_CAPTION, TX);
    char sub[93];
    strset(sub, syskind(&file), 93);
    append(sub, "  ·  ", 93);
    exactsize(sub, file.size, 93);
    Text(x + 70, y + 40, sub, F_SMALL, SUB);
    Text(x + w - 45, y + 21, "×", F_CAPTION, SUB);
    int yy = y + 76;
    int nr = nrows();
    Box(x + 15, yy, w - 30, nr * 17 + 19, 9, si.dark ? 0x1c1c20 : 0xf0f1f5);
    int at = 0, cols = MIN(70, (w - 73) / 8);
    for (int j = 0; j < nr && at < nread; j++) {
        char s[92] = {0};
        if (binary) {
            hexval(s, off + at, 8);
            append(s, "   ", 92);
            int m = MIN(12, nread - at);
            for (int k = 0; k < 12; k++) {
                char a[3] = "  ";
                if (k < m)hexval(a, page[at + k], 2);
                append(s, a, 92);
                append(s, " ", 92);
            }
            if (w >= 500) {
                append(s, "  ", 92);
                for (int k = 0; k < m; k++) {
                    char a[2] = {page[at + k] >= 32 && page[at + k] < 127 ? (char)page[at + k] : '.', 0};
                    append(s, a, 92);
                }
            }
            at += m;
        } else {
            int k = 0;
            while (at < nread && k < cols && page[at] != '\n') {
                u8 v = page[at++];
                s[k++] = v == '\t' ? ' ' : v < 32 ? ' ' : v;
            }
            s[k] = 0;
            if (at < nread && page[at] == '\n')at++;
        }
        Text(x + 25, yy + 8 + j * 17, s, F_MONO, TX);
    }
    if (!nread)Text(x + 25, yy + 8, fd < 0 ? "Lettura non riuscita" : "File vuoto", F_BODY, SUB);
    next = at;
    yy += nr * 17 + 24;
    char range[80] = "Byte ";
    addnum(range, off, 80);
    append(range, "–", 80);
    addnum(range, next ? off + next - 1 : off, 80);
    append(range, "   ·   Sola lettura", 80);
    Text(x + 19, yy, range, F_SMALL, SUB);
    button(x + w - 100, yy - 4, 34, "←", 0);
    button(x + w - 59, yy - 4, 34, "→", 0);
    yy = y + h - 87;
    Box(x + 16, yy - 7, w - 32, 1, 0, EDGE);
    Text(x + 19, yy + 1, "SHA-256", F_MICRO, EMP);
    if (digest[0] && strlen(digest) == 64) {
        char a[33];
        strset(a, digest, 33);
        Text(x + 19, yy + 19, a, F_MONO, TX);
        Text(x + 19, yy + 38, digest + 32, F_MONO, TX);
    } else {
        char m[92] = "Lettura integrale  ·  ";
        addnum(m, (u64)total * 100 / MAX(1, file.size), 92);
        append(m, "%", 92);
        Text(x + 19, yy + 24, digest[0] ? digest : m, F_SMALL, SUB);
    }
    Text(x + 19, y + h - 23, "Supporto di avvio", F_SMALL, SUB);
    Text(x + w - 130, y + h - 23, "Spazio per chiudere", F_SMALL, SUB);
}
int inspect_click(int X, int Y)
{
    int w = width(), h = height(), x = (aw - w) / 2, y = (ah - h) / 2, row = y + 76 + nrows() * 17 + 24;
    if (X >= x + w - 56 && Y >= y + 8 && Y < y + 54)return 1;
    if (Y >= row - 4 && Y < row + 22) {
        if (X >= x + w - 100 && X < x + w - 66)inspect_nav(K_LEFT);
        if (X >= x + w - 59 && X < x + w - 25)inspect_nav(K_RIGHT);
    }
    return 0;
}
