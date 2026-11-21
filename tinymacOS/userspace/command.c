#include "path.h"
#include "sha256.h"
#include "sha1.h"
#include "date.h"
int strncmp(const char *, const char *, unsigned long); /* kernel/string.c */
// The executable is shared on disk; exec resolves its invoked path and supplies argv[0].
static ExecArgs A;
static char bytes[MAXFILE + 1], out[PIPEMAX + 1], pa[PATHLEN], pb[PATHLEN];
static int nout, full;
static Dirent ds[NSLOTS];
#define V(i) A.argv[i]
#define IS(s) eq(V(0),s)
static void emit(const void *s, int z)
{
    if (z > PIPEMAX - nout) {
        full = 1;
        return;
    }
    memcpy(out + nout, s, z);
    nout += z;
}
static void say(const char *s)
{
    emit(s, strlen(s));
}
static void ln(const char *s)
{
    say(s);
    say("\n");
}
static void num(u64 n)
{
    char z[24];
    number(z, n);
    say(z);
}
static void two(int n)
{
    if (n < 10)say("0");
    num(n);
}
static int fail(const char *s)
{
    ln(s);
    return 1;
}
static int path(char *p, int a)
{
    return a < A.argc ? expandpath(p, A.cwd, V(a)) : -1;
}
static int input(int a)
{
    if (a == A.argc || (a < A.argc && eq(V(a), "-"))) {
        memcpy(bytes, A.in, A.inlen);
        bytes[A.inlen] = 0;
        return A.inlen;
    }
    Dirent d;
    if (path(pa, a) || sc(S_STAT, pa, &d, 0, 0) || d.dir || d.size > MAXFILE)return -1;
    int n = readfile(pa, bytes, MAXFILE);
    if (n >= 0)bytes[n] = 0;
    return n;
}
static int cmp(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (u8) * a - (u8) * b;
}
static int contain(const char *s, int n, const char *q)
{
    int z = strlen(q);
    if (!z)return 1;
    for (int j = 0; j + z <= n; j++)if (!memcmp(s + j, q, z))return 1;
    return 0;
}
static int diskid(const char *s)
{
    if (!memcmp(s, "/dev/", 5))s += 5;
    if (memcmp(s, "disk", 4) || s[4] < '0' || s[4] > '7' || s[5])return -1;
    return s[4] - '0';
}
static int run(void)
{
    int n = A.argc;
    Dirent d, b;
    SysInfo t;
    sc(S_INFO, &t, 0, 0, 0);
    if(IS("bootverbose")) {
        if(n==1 || (n==2 && eq(V(1),"status"))) {
            ln((t.boot_flags&1)?"Verbose avvio corrente: attivo":"Verbose avvio corrente: disattivo");
            long next=sc(S_BOOTCONFIG,0,0,0,0);
            if(next>=0)ln((next&1)?"Prossimo avvio: verbose attivo":"Prossimo avvio: verbose disattivo");
            else ln("Preferenza persistente non disponibile su questo supporto.");
            return 0;
        }
        if((n==2||n==3) && eq(V(1),"log")) {
            unsigned page=0;if(n==3){const char *p=V(2);if(!*p)return fail("Pagina non valida.");for(;*p;p++){if(*p<'0'||*p>'9'||page>31)return fail("Pagina non valida.");page=page*10+*p-'0';}}if(page>31)return fail("Pagina log: 0..31.");
            long z=sc(S_BOOTCONFIG,2,bytes,512,page*512);if(z<0)return fail("Log non disponibile.");emit(bytes,z);return 0;
        }
        if(n==2 && (eq(V(1),"on")||eq(V(1),"off"))) {
            long z=sc(S_BOOTCONFIG,1,eq(V(1),"on"),0,0);
            if(z<0)return fail("Preferenza non scritta: serve un disco tinymacOS installato. Per avvio live premi V o passa -v.");
            ln("Preferenza verificata su disco; verra applicata al prossimo avvio.");return 0;
        }
        return fail("Uso: bootverbose status | on | off | log [pagina 0..31]");
    }
    if (IS("pwd")) {
        ln(A.cwd);
        return 0;
    }
    if (IS("whoami")) {
        ln(t.user);
        return 0;
    }
    if (IS("echo")) {
        int i = n > 1 && eq(V(1), "-n") ? 2 : 1, start = i;
        for (; i < n; i++) {
            if (i != start)say(" ");
            say(V(i));
        }
        if (start == 1)say("\n");
        return 0;
    }
    if (IS("which")) {
        int e = 0;
        for (int i = 1; i < n; i++) {
            if (!cmdpath(pa, A.cwd, V(i)))ln(pa);
            else e = 1;
        }
        return e || n < 2;
    }
    if (IS("basename") || IS("dirname")) {
        if (n != 2)return 2;
        strset(pa, V(1), PATHLEN);
        int z = strlen(pa);
        while (z > 1 && pa[z - 1] == '/')pa[--z] = 0;
        if (IS("basename"))ln(eq(pa, "/") ? "/" : leaf(pa));
        else {
            const char *q = leaf(pa);
            if (q == pa)ln(".");
            else {
                parentpath(pa);
                ln(pa);
            }
        }
        return 0;
    }
    if (IS("sw_vers")) {
        if (n == 1) {
            ln("ProductName:       tinymacOS");
            ln("ProductVersion:    10.1.2");
            ln("BuildVersion:      30B3");
        } else if (n == 2 && eq(V(1), "-productVersion"))ln("10.1.2");
        else if (n == 2 && eq(V(1), "-buildVersion"))ln("30B3");
        else if (n == 2 && eq(V(1), "-productName"))ln("tinymacOS");
        else return 2;
        return 0;
    }
    if (IS("uname")) {
        if (n == 1 || eq(V(1), "-s"))ln("tinymacOS");
        else if (eq(V(1), "-m"))ln("x86_64");
        else if (eq(V(1), "-r"))ln("10.1.2");
        else if (eq(V(1), "-a"))ln("tinymacOS tramontana 10.1.2 30B3 x86_64");
        else return 2;
        return 0;
    }
    if (IS("csrutil")) {
        if (n == 1 || (n == 2 && eq(V(1), "status"))) {
            ln((t.csr & 1) ? "System Integrity Protection: attiva."
                           : "System Integrity Protection: disattiva.");
            return 0;
        }
        if (n == 2 && (eq(V(1), "enable") || eq(V(1), "disable")))
            return fail("csrutil: solo la Shell di Recupero puo modificare SIP (riavviare premendo R).");
        return fail("Uso: csrutil status | csrutil enable | csrutil disable");
    }
    if (IS("kextstat")) {
        KextInfo k[8];
        int got = (int)sc(S_KEXT, 0, k, 8, 0);
        if (got < 0) return fail("kextstat: errore di sistema");
        if (!got) {
            ln("Nessuna estensione del kernel caricata.");
            return 0;
        }
        for (int i = 0; i < got; i++) {
            say(k[i].id);
            say(" (");
            say(k[i].ver);
            say(")  ");
            ln(k[i].name);
        }
        return 0;
    }
    if (IS("kextload") || IS("kextunload")) {
        if (n != 2) return fail(IS("kextload") ? "Uso: kextload /percorso/Bundle.kext"
                                               : "Uso: kextunload bundle-id");
        char err[128], bp[PATHLEN];
        err[0] = 0;
        if (expandpath(bp, A.cwd, V(1)))
            return fail(IS("kextload") ? "kextload: percorso non valido"
                                       : "kextunload: bundle-id non valido");
        int op = IS("kextload") ? 1 : 2;
        int r = (int)sc(S_KEXT, op, bp, err, 0);
        if (r < 0) {
            say(IS("kextload") ? "kextload: " : "kextunload: ");
            ln(err[0] ? err : "caricamento non riuscito");
            return 1;
        }
        ln(IS("kextload") ? "Estensione caricata." : "Estensione scaricata.");
        return 0;
    }
    if (IS("ifconfig")) {
        NetIfInfo ifs[4];
        int got = (int)sc(S_NET, 0, ifs, 4, 0);
        if (got < 0) return fail("ifconfig: errore di sistema");
        for (int i = 0; i < got; i++) {
            char z[40];
            ln(ifs[i].name);
            say("    indirizzo hw ");
            for (int m = 0; m < 6; m++) {
                if (m) say(":");
                z[0] = "0123456789abcdef"[ifs[i].mac[m] >> 4];
                z[1] = "0123456789abcdef"[ifs[i].mac[m] & 15];
                z[2] = 0;
                say(z);
            }
            if (ifs[i].up) {
                say("\n    inet ");
                z[0] = 0;
                int len = 0;
                for (int q = 0; q < 4; q++) {
                    char part[4];
                    number(part, (ifs[i].ip >> (q * 8)) & 0xff);
                    for (int w = 0; part[w]; w++) z[len++] = part[w];
                    if (q < 3) z[len++] = '.';
                }
                z[len] = 0;
                say(z);
                say(ifs[i].wifi ? "  (rete wireless simulata)" : "  (ethernet)");
            } else ln("    attiva: no");
            ln("");
        }
        if (!got) ln("Nessuna interfaccia di rete.");
        return 0;
    }
    if (IS("ping")) {
        if (n != 2) return fail("Uso: ping indirizzo");
        u32 ip = 0;
        int parts[4] = {0, 0, 0, 0}, np = 0;
        const char *v = V(1);
        u32 cur = 0;
        int dig = 0;
        for (int i = 0; ; i++) {   /* start at 0: skipping v[0] ate the 1st octet */

            char c = v[i];
            if (c >= '0' && c <= '9') { cur = cur * 10 + (u32)(c - '0'); dig = 1; if (cur > 255) return fail("ping: indirizzo non valido"); }
            else if (c == '.' && i && v[i - 1] >= '0' && v[i - 1] <= '9' && np < 3) { parts[np++] = (int)cur; cur = 0; dig = 0; }
            else if (c == 0 && dig) { parts[np++] = (int)cur; break; }
            else return fail("ping: indirizzo non valido (serve IPv4, es. 10.0.2.2)");
        }
        if (np != 4) return fail("ping: indirizzo non valido");
        for (int q = 0; q < 4; q++) ip |= (u32)parts[q] << (q * 8);
        PingResult pr;
        memset(&pr, 0, sizeof pr);
        pr.ip = ip;
        pr.count = 4;
        int r = (int)sc(S_NET, 1, &pr, 0, 0);
        if (r < 0) return fail("ping: rete non disponibile");
        for (u32 i = 0; i < pr.ok && i < pr.count; i++) {
            say("Risposta da ");
            say(V(1));
            say(": byte=32 tempo=");
            num(pr.rtt[i]);
            ln(" ms");
        }
        if (!pr.ok) return fail("ping: nessuna risposta (timeout)");
        say("Pacchetti: inviati ");
        num(pr.count);
        say(", ricevuti ");
        num(pr.ok);
        say(", persi ");
        num(pr.count - pr.ok);
        ln("");
        return 0;
    }
    if (IS("wifi")) {
        if (n >= 2 && eq(V(1), "status")) {
            WifiStatus w;
            memset(&w, 0, sizeof w);
            if (sc(S_NET, 2, &w, 0, 0) < 0) return fail("wifi: radio non disponibile");
            say(w.state ? "Connessa a " : "Non connessa");
            if (w.state) { ln(w.ssid); } else ln("");
            if (w.state) {
                char z[16];
                int len = 0;
                for (int q = 0; q < 4; q++) {
                    char part[4];
                    number(part, (w.ip >> (q * 8)) & 0xff);
                    for (int u = 0; part[u]; u++) z[len++] = part[u];
                    if (q < 3) z[len++] = '.';
                }
                z[len] = 0;
                say("    IP ");
                ln(z);
                say("    canale ");
                num(w.channel);
                ln("");
            }
            ln(w.note);
            return 0;
        }
        if (n >= 2 && eq(V(1), "scan")) {
            WifiAp aps[8];
            int got = (int)sc(S_NET, 2, aps, 1, 0);
            if (got < 0) return fail("wifi: radio non disponibile");
            for (int i = 0; i < got; i++) {
                say(aps[i].ssid);
                say("  canale ");
                num(aps[i].channel);
                say("  ");
                if (aps[i].rssi < 0) {
                    say("-");
                    num((u32)(-aps[i].rssi));
                } else num((u32)aps[i].rssi);
                say(" dBm ");
                ln(aps[i].secure ? "protetta" : "aperta");
            }
            ln("Nota: radio simulata (nessun hardware Wi-Fi in QEMU).");
            return 0;
        }
        if (n >= 3 && eq(V(1), "connect")) {
            int r = (int)sc(S_NET, 2, 0, 2, V(2));
            if (r < 0) return fail("wifi: rete non trovata (vedi wifi scan)");
            ln("Connessa.");
            return 0;
        }
        if (n == 2 && eq(V(1), "disconnect")) {
            int r = (int)sc(S_NET, 2, 0, 2, "");
            return r < 0 ? fail("wifi: errore") : 0;
        }
        ln("Uso: wifi status | wifi scan | wifi connect <ssid> | wifi disconnect");
        return 0;
    }
    if (IS("host")) {
        if (n != 2) return fail("Uso: host nome");
        u32 ip = 0;
        if ((int)sc(S_NET, 3, V(1), &ip, 0) < 0)
            return fail("host: risoluzione non riuscita");
        say(V(1));
        say(" -> ");
        num(ip & 0xff);
        say(".");
        num((ip >> 8) & 0xff);
        say(".");
        num((ip >> 16) & 0xff);
        say(".");
        num((ip >> 24) & 0xff);
        say("\n");
        return 0;
    }
    if (IS("fetch")) {
        if (n != 2) return fail("Uso: fetch http://host[:port]/percorso");
        const char *p = V(1);
        if (p[0] == 'h' && p[1] == 't' && p[2] == 't' && p[3] == 'p' &&
            p[4] == ':' && p[5] == '/' && p[6] == '/')
            p += 7;
        else if (p[0] == 'h' && p[1] == 't' && p[2] == 't' && p[3] == 'p' &&
                 p[4] == 's')
            return fail("fetch: HTTPS non disponibile (nessun TLS)");
        else
            return fail("Uso: fetch http://host[:port]/percorso");
        char host[128];
        int hi = 0;
        while (*p && *p != '/' && *p != ':' && hi < 127) host[hi++] = *p++;
        host[hi] = 0;
        if (!hi) return fail("fetch: host mancante");
        int port = 80;
        if (*p == ':') {
            p++;
            port = 0;
            while (*p >= '0' && *p <= '9' && port < 65536)
                port = port * 10 + (*p++ - '0');
            if (port < 1 || port > 65535) return fail("fetch: porta non valida");
        }
        const char *path = *p ? p : "/";
        int h = (int)sc(S_NET, 4, host, port, 0);
        if (h < 0) return fail("fetch: connessione non riuscita");
        char req[600];
        int ri = 0;
        int pl = (int)strlen(path);
        if (ri + 4 + pl + 200 + hi >= (int)sizeof req) {
            sc(S_NET, 7, 0, 0, h);
            return fail("fetch: percorso troppo lungo");
        }
        memcpy(req + ri, "GET ", 4); ri += 4;
        memcpy(req + ri, path, (size_t)pl); ri += pl;
        {
            const char *mid = " HTTP/1.0\r\nHost: ";
            int ml = (int)strlen(mid);
            memcpy(req + ri, mid, (size_t)ml); ri += ml;
        }
        memcpy(req + ri, host, (size_t)hi); ri += hi;
        {
            const char *tail = "\r\nUser-Agent: tinymacOS/10\r\nConnection: close\r\n\r\n";
            int tl = (int)strlen(tail);
            memcpy(req + ri, tail, (size_t)tl); ri += tl;
        }
        int sent = 0;
        while (sent < ri) {
            int w = (int)sc(S_NET, 5, req + sent, ri - sent, h);
            if (w <= 0) break;
            sent += w;
        }
        if (sent != ri) {
            sc(S_NET, 7, 0, 0, h);
            return fail("fetch: invio non riuscito");
        }
        char body[4096];
        int got = 0;
        u64 end = sc(S_TICKS, 0, 0, 0, 0) + 500;      /* 5 s */
        for (;;) {
            int r = (int)sc(S_NET, 6, body + got, (int)sizeof body - 1 - got, h);
            if (r < 0) break;                          /* chiuso dal peer */
            if (r > 0) {
                got += r;
                if (got >= (int)sizeof body - 1) break;
            } else {
                if (sc(S_TICKS, 0, 0, 0, 0) >= end) break;
                sc(S_SLEEP, 2, 0, 0, 0);
            }
        }
        sc(S_NET, 7, 0, 0, h);
        if (!got) return fail("fetch: nessun dato (timeout)");
        body[got] = 0;
        int nl = 0;
        while (nl < got && body[nl] != '\n') nl++;
        if (nl < got) nl++;
        say("HTTP -> ");
        emit(body, nl > 80 ? 80 : nl);
        say(" [");
        num((u64)got);
        ln(" byte]");
        if (nl < got) {
            int rest = got - nl;
            if (rest > 1800) rest = 1800;
            ln("--- corpo ---");
            emit(body + nl, rest);
            if (got - nl > 1800) ln("\n…[troncato]");
        }
        return 0;
    }
    if (IS("date")) {
        if (n > 1)return 2;
        say(dayname(weekday(t.year, t.month, t.day)));
        say(" ");
        num(t.day);
        say(" ");
        say(monthname(t.month));
        say(" ");
        num(t.year);
        say("  ");
        two(t.hour);
        say(":");
        two(t.minute);
        say(":");
        two(t.second);
        say("\n");
        return 0;
    }
    if (IS("ps")) {
        ProcInfo v[NPROC];
        int z = sc(S_PS, v, NPROC, 0, 0);
        ln("  PID   THREAD  COMMAND");
        for (int i = 0; i < z; i++) {
            say("  ");
            num(v[i].pid);
            say("     ");
            num(v[i].threads);
            say("       ");
            ln(v[i].name);
        }
        return 0;
    }
    if (IS("netsurf")) {
        const char *u = n >= 2 ? V(1) : 0;
        return sc(S_SPAWN, NETSURF, u, 0, 0) < 0;
    }
    if (IS("ls")) {
        int k = 1, all = 0, detail = 0;
        while (k < n && V(k)[0] == '-') {
            for (int j = 1; V(k)[j]; j++) {
                if (V(k)[j] == 'l')detail = 1;
                else if (V(k)[j] == 'a')all = 1;
                else if (V(k)[j] != '1')return 2;
            }
            k++;
        }
        if (k == n)strset(pa, A.cwd, PATHLEN);
        else if (k + 1 != n || path(pa, k))return 2;
        if (sc(S_STAT, pa, &d, 0, 0))return 1;
        int z = 0;
        if (d.dir)z = sc(S_LIST, pa, ds, NSLOTS, 0);
        else ds[z++] = d;
        if (z < 0)return 1;
        for (int j = 1; j < z; j++) {
            Dirent v = ds[j];
            int i = j;
            while (i && cmp(ds[i - 1].name, v.name) > 0) {
                ds[i] = ds[i - 1];
                i--;
            }
            ds[i] = v;
        }
        if (all && d.dir) {
            ln(".");
            ln("..");
        }
        for (int i = 0; i < z; i++) {
            if (!all && ds[i].name[0] == '.')continue;
            if (detail) {
                say(ds[i].dir ? "d" : "-");
                say(ds[i].ro ? "r--  " : "rw-  ");
                num(ds[i].size);
                say("  ");
            }
            ln(ds[i].name);
        }
        return 0;
    }
    if (IS("shasum")) {
        int i = 1, sha2 = 0;
        if (n > 1 && eq(V(1), "-a")) {
            if (n < 3 || (!eq(V(2), "1") && !eq(V(2), "256")))return 2;
            sha2 = eq(V(2), "256");
            i = 3;
        }
        do {
            SHA256 h;
            sha_init(&h);
            SHA1 one = {.state = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0}};
            int stream = i == n || eq(V(i), "-");
            if (stream) {
                if (sha2)sha_update(&h, A.in, A.inlen);
                else sha1_add(&one, A.in, A.inlen);
            } else {
                if (path(pa, i))return 1;
                int f = sc(S_OPEN, pa, 0, 0, 0), r;
                if (f < 0)return 1;
                while ((r = sc(S_READ, f, bytes, MAXFILE, 0)) > 0) {
                    if (sha2)sha_update(&h, bytes, r);
                    else sha1_add(&one, bytes, r);
                }
                sc(S_CLOSE, f, 0, 0, 0);
                if (r < 0)return 1;
            }
            u8 sum[32] = {0};
            char hex[65];
            if (sha2)sha_final(&h, sum);
            else sha1_end(&one, sum);
            sha_hex(sum, hex);
            if (!sha2)hex[40] = 0;
            say(hex);
            say("  ");
            ln(stream ? "-" : V(i));
            i++;
        } while (i < n);
        return 0;
    }
    if (IS("cat")) {
        int i = 1;
        do {
            int z = input(i++);
            if (z < 0)return 1;
            emit(bytes, z);
        } while (i < n);
        return 0;
    }
    if (IS("wc") || IS("grep") || IS("head") || IS("tail") || IS("sort")) {
        int k = 1, cut = 10;
        const char *q = "";
        if (IS("grep")) {
            if (n < 2)return 2;
            q = V(k++);
        }
        if (IS("wc") && k < n && V(k)[0] == '-')q = V(k++);
        if ((IS("head") || IS("tail")) && k + 1 < n && eq(V(k), "-n")) {
            k++;
            if (V(k)[0] < '0' || V(k)[0] > '9')return 2;
            cut = val(V(k++));
        }
        if (k + 1 < n)return 2;
        int z = input(k);
        if (z < 0)return 1;
        int w = 0, l = 0, prev = 0;
        for (int j = 0; j < z; j++) {
            if (bytes[j] == '\n')l++;
            int on = (u8)bytes[j] > 32;
            if (on && !prev)w++;
            prev = on;
        }
        if (IS("wc")) {
            if (!*q) {
                num(l);
                say("  ");
                num(w);
                say("  ");
                num(z);
            } else if (eq(q, "-l"))num(l);
            else if (eq(q, "-w"))num(w);
            else if (eq(q, "-c"))num(z);
            else return 2;
            if (k < n) {
                say(" ");
                say(V(k));
            }
            say("\n");
            return 0;
        }
        static u16 pos[MAXFILE];
        int nline = 0, hit = 0;
        int count = l + (z && bytes[z - 1] != '\n');
        for (int j = 0; j < z;) {
            int a = j;
            while (j < z && bytes[j] != '\n')j++;
            int end = j;
            if (j < z)j++;
            if (IS("sort")) {
                bytes[end] = 0;
                pos[nline] = a;
            } else if (IS("grep") ? contain(bytes + a, end - a, q) : IS("head") ? nline<cut: nline >= count - cut) {
                emit(bytes + a, j - a);
                hit++;
            }
            nline++;
        }
        if (IS("sort")) {
            for (int j = 1; j < nline; j++) {
                u16 v = pos[j];
                int i = j;
                while (i && cmp(bytes + pos[i - 1], bytes + v) > 0) {
                    pos[i] = pos[i - 1];
                    i--;
                }
                pos[i] = v;
            }
            for (int i = 0; i < nline; i++)ln(bytes + pos[i]);
        }
        return IS("grep") && !hit;
    }
    if (IS("diskutil")) {
        DiskInfo di;
        if (sc(S_DISK, 0, &di, 0, 0))return 1;
        if (n == 2 && eq(V(1), "list")) {
            for (int i = 0; i < di.count; i++) {
                say("/dev/disk");
                num(i);
                say(" (internal, physical): ");
                num(di.disk[i].sectors / 2048);
                ln(" MiB");
                if (di.disk[i].layout) {
                    ln("  1: EFI              Macintosh HD");
                    ln("  2: BTRFS subset     Macintosh HD - Dati");
                    if (di.disk[i].layout == 2) {
                        ln("  3: EFI              Recovery");
                        ln("  4: Restore          Sistema locale");
                    }
                } else ln("  Nessun volume tinymacOS montato");
            }
            if (!di.count)ln("Nessun disco collegato.");
            return 0;
        }
        if (n >= 3 && eq(V(1), "apfs")) {
            SnapInfo sn[8];
            FsInfo fi;
            EncInfo ei;
            int listing = eq(V(2), "list") || (n >= 4 && eq(V(2), "snapshot") && eq(V(3), "list"));
            if (listing) {
                if (sc(S_SNAP, 5, 0, &fi, 0))
                    return fail("Volume non disponibile.");
                sc(S_SNAP, 6, 0, 0, &ei);
                ln("Container cont0 — disco interno (BTRFS subset, struttura APFS)");
                say("  Volume \"Macintosh HD\": ");
                num(fi.used / 1024);
                say(" KB usati su ");
                num(fi.total / 1024);
                say(" KB · generazione ");
                num(fi.gen);
                ln("");
                say("  Crittografia: ");
                ln(ei.state ? (ei.locked ? "attiva (bloccata)" : "attiva (sbloccata)") : "disattiva");
                if (ei.state) {
                    say("  File protetti: ");
                    num(ei.files);
                    ln("");
                }
                int z = sc(S_SNAP, 0, 0, sn, 8);
                say("  Snapshot (");
                num(z < 0 ? 0 : z);
                ln("):");
                for (int i = 0; i < z; i++) {
                    say("    ");
                    say(sn[i].name);
                    say("  id ");
                    num(sn[i].id);
                    say("  ");
                    unsigned v = sn[i].date;
                    num(((v >> 25) & 127) + 1980);
                    say("-");
                    two((v >> 21) & 15);
                    say("-");
                    two((v >> 16) & 31);
                    say(" ");
                    two((v >> 11) & 31);
                    say(":");
                    two((v >> 5) & 63);
                    ln("");
                }
                return 0;
            }
            if (n >= 4 && eq(V(2), "snapshot")) {
                int op = eq(V(3), "create") ? 1 : eq(V(3), "restore") ? 2 : eq(V(3), "verify") ? 3 :
                         eq(V(3), "delete") ? 4 : 0;
                if (!op)return 1;
                int r = sc(S_SNAP, op, V(4), 0, 0);
                if (op == 3 && !r) {
                    ln("Snapshot valido: integrità crc32c verificata.");
                    return 0;
                }
                if (r)return fail("Operazione non riuscita.");
                ln(op == 1 ? "Snapshot creato." : op == 2 ? "Volume ripristinato." : "Snapshot eliminato.");
                return 0;
            }
            if (eq(V(2), "encryptVolume") && n == 4) {
                ln("Attivazione della crittografia del volume…");
                if (sc(S_SNAP, 6, 1, V(3), 0))
                    return fail("Crittografia non attivata (passphrase 4-63 caratteri, nessun volume già criptato).");
                ln("Volume criptato: i file personali sono protetti. Sblocca con apfs_unlockfv.");
                return 0;
            }
            if (eq(V(2), "decryptVolume") && n == 4) {
                if (sc(S_SNAP, 6, 2, V(3), 0))
                    return fail("Decrittografia non riuscita: passphrase errata.");
                ln("Volume decriptato: la crittografia è stata disattivata.");
                return 0;
            }
            if (eq(V(2), "lockVolume") && n == 3) {
                if (sc(S_SNAP, 6, 3, 0, 0))
                    return fail("Volume non bloccato: crittografia non attiva o già bloccato.");
                ln("Volume bloccato: i file protetti non sono leggibili fino allo sblocco.");
                return 0;
            }
            if (eq(V(2), "unlockVolume") && n == 4) {
                if (sc(S_SNAP, 6, 4, V(3), 0))
                    return fail("Sblocco non riuscito: passphrase errata.");
                ln("Il volume è stato sbloccato.");
                return 0;
            }
            return fail("diskutil apfs list | apfs snapshot list|create|restore|verify|delete <nome> | "
                        "apfs encryptVolume|decryptVolume|unlockVolume <pass> | apfs lockVolume");
        }
        if (n == 3) {
            int i = diskid(V(2));
            if (i < 0 || i >= di.count)return 1;
            if (eq(V(1), "info")) {
                ln(di.disk[i].model);
                num(di.disk[i].sectors / 2048);
                say(" MiB · ");
                ln(di.disk[i].layout ? "GPT" : "Nessun volume compatibile");
                return 0;
            }
            if (eq(V(1), "verifyVolume") || eq(V(1), "repairVolume")) {
                int r = sc(S_DISK, eq(V(1), "repairVolume") ? 1 : 4, i, 0, 0);
                ln(r ? "Verifica non riuscita." : "Il volume risulta valido.");
                return !!r;
            }
        }

        return fail("diskutil list | info diskN | verifyVolume diskN | repairVolume diskN");
    }
    if (IS("fsck_apfs")) {
        DiskInfo di;
        if (sc(S_DISK, 0, &di, 0, 0))return 1;
        int repair = 0;
        for (int i = 1; i < A.argc; i++)if (eq(V(i), "-y") || eq(V(i), "-f"))repair = 1;
        int dsk = di.current;
        if (dsk < 0 || dsk >= di.count || !di.disk[dsk].layout)
            return fail("fsck_apfs: nessun volume BTRFS/APFS montato su questo disco.");
        ln(repair ? "Riparazione del volume in corso\u2026" : "Verifica del volume in corso\u2026");
        int r = sc(S_DISK, repair ? 1 : 4, dsk, 0, 0);
        if (r)return fail("fsck_apfs: controllo non riuscito; riprova con -y o avvia Recupero.");
        ln(repair ? "Ripristino completato: il volume è valido." : "Il volume risulta valido.");
        return 0;
    }
    if (IS("mount_apfs")) {
        DiskInfo di;
        if (sc(S_DISK, 0, &di, 0, 0))return 1;
        int dsk = di.current;
        if (dsk >= 0 && dsk < di.count && di.disk[dsk].layout) {
            ln("Macintosh HD - Dati è gi\u00e0 montato su / (BTRFS subset, struttura APFS).");
            return 0;
        }
        return fail("mount_apfs: nessun volume da montare. Vedi «diskutil list».");
    }
    if (IS("newfs_apfs")) {
        ln("newfs_apfs: l'inizializzazione del disco richiede l'Installatore tinymacOS.");
        ln("Avvia «Installa tinymacOS» e usa Utility Disco → Inizializza disco.");
        return 1;
    }
    if (IS("apfs_unlockfv")) {
        EncInfo ei;
        if (sc(S_SNAP, 6, 0, 0, &ei))return 1;
        if (!ei.state)return fail("apfs_unlockfv: la crittografia del volume non è attiva.");
        if (!ei.locked) {
            ln("apfs_unlockfv: il volume è gi\u00e0 sbloccato.");
            return 0;
        }
        char pass[64];
        int pl = 0;
        if (A.argc >= 2) {
            strset(pass, V(1), sizeof pass);
            pl = (int)strlen(pass);
        } else if (A.inlen > 0) {
            pl = A.inlen < 63 ? A.inlen : 63;
            memcpy(pass, A.in, pl);
            while (pl > 0 && (pass[pl - 1] == '\n' || pass[pl - 1] == '\r'))pl--;
            pass[pl] = 0;
        }
        if (pl < 4)return fail("apfs_unlockfv: servono almeno 4 caratteri di passphrase.");
        if (sc(S_SNAP, 6, 4, pass, 0))return fail("apfs_unlockfv: passphrase errata.");
        ln("Il volume è stato sbloccato.");
        return 0;
    }
    if (IS("open")) {
        if (n == 3 && eq(V(1), "-a")) {
            for (int i = 1; i <= lastapp(t.live_cd); i++) {
                if (i == INSTALLER && !t.live_cd)continue; /* live-only app */
                if (eq(V(2), appname(i)) || eq(V(2), bundlename(i)))
                    return sc(S_SPAWN, i, i == NETSURF ? V(2) : 0, 0, 0) < 0;
            }
            return fail("Applicazione non trovata.");
        }
        if (n == 2 && (!strncmp(pa, "http://", 7) || !strncmp(pa, "https://", 8)))
            return sc(S_SPAWN, NETSURF, pa, 0, 0) < 0;
        if (n != 2 || path(pa, 1) || sc(S_STAT, pa, &d, 0, 0))return 1;
        char app[PATHLEN];
        for (int i = 1; i <= lastapp(t.live_cd); i++) {
            strset(app, apploc(i), PATHLEN);
            append(app, "/", PATHLEN);
            append(app, bundlename(i), PATHLEN);
            append(app, ".app", PATHLEN);
            if (eq(app, pa))return sc(S_SPAWN, i, 0, 0, 0) < 0;
        }
        return sc(S_SPAWN, d.dir ? FINDER : TEXTEDIT, pa, 0, 0) < 0;
    }
    if (n < 2 || path(pa, 1))return 2;
    if (IS("cp") || IS("mv")) {
        if (n != 3 || path(pb, 2) || sc(S_STAT, pa, &d, 0, 0))return 1;
        if (!sc(S_STAT, pb, &b, 0, 0) && b.dir) {
            char dest[PATHLEN];
            if (absolutepath(dest, pb, leaf(pa)))return 1;
            strset(pb, dest, PATHLEN);
        }
        if (eq(pa, pb))return fail("Origine e destinazione coincidono.");
        if (IS("mv"))return !!sc(S_MOVE, pa, pb, 0, 0);
        if (d.dir || d.size > MAXFILE)return 1;
        int z = readfile(pa, bytes, MAXFILE);
        return z < 0 || writefile(pb, bytes, z) != z;
    }
    for (int k = 1; k < n; k++) {
        if (path(pa, k))return 1;
        if (IS("mkdir")) {
            if (sc(S_CREATE, pa, 1, 0, 0) < 0)return 1;
        } else if (IS("touch")) {
            if (sc(S_TOUCH, pa, 0, 0, 0) < 0)return 1;
        } else if (IS("rm") || IS("rmdir")) {
            if (sc(S_STAT, pa, &d, 0, 0) || !!d.dir != IS("rmdir") || sc(S_REMOVE, pa, 0, 0, 0))return 1;
        } else return 2;
    }
    return 0;
}
void app_main(void)
{
    int r = 2;
    if (!sc(S_ARGV, &A, 0, 0, 0)) {
        r = run();
        if (full) {
            nout = 0;
            r = fail("Risultato troppo grande per questa sessione (8 KiB).");
        } else if (r && !nout && !(IS("grep") && r == 1)) {
            say(V(0));
            say(": ");
            ln(r == 2 ? "argomenti non validi" : "verifica percorso, permessi o spazio disponibile");
        }
        if (nout)sc(S_STDOUT, out, nout, 0, 0);
    }
    sc(S_RETURN, r, 0, 0, 0);
}
