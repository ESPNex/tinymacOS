#include "path.h"
#include "display_size.h"
#include "disk_size_input.h"
#include "fileops.h"
#include "sha256.h"
#include "sha1.h"
#include "date.h"
#include "volume_export.h"
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
static int disk_uint(const char*s,u32*out)
{
    u64 n=0;if(!*s)return -1;
    for(;*s;s++){if(*s<'0'||*s>'9')return -1;n=n*10+*s-'0';if(n>TMC_DISK_MAX_SECTORS)return -1;}
    *out=n;return 0;
}
static int disk_partition_id(const char*s,int*disk,int*part)
{
    if(!memcmp(s,"/dev/",5))s+=5;
    if(memcmp(s,"disk",4)||s[4]<'0'||s[4]>'7'||s[5]!='s')return -1;
    u32 p;if(disk_uint(s+6,&p)||!p||p>128)return -1;
    *disk=s[4]-'0';*part=p;return 0;
}
static int remove_command(int directory)
{
    unsigned flags=directory?REMOVE_DIR_ONLY:REMOVE_FILE_ONLY;
    int k=1,force=0,status=0;
    if(!directory)for(;k<A.argc && V(k)[0]=='-';k++){
        const char *arg=V(k);
        if(eq(arg,"--")){k++;break;}
        if(!arg[1])break;
        for(int j=1;arg[j];j++){
            if(arg[j]=='r'||arg[j]=='R')flags=REMOVE_RECURSIVE;
            else if(arg[j]=='f')force=1;
            else return fail("Uso: rm [-rR] [-f] [--] percorso…");
        }
    }
    else if(k<A.argc && eq(V(k),"--"))k++;
    if(k==A.argc)return force?0:fail(directory?"Uso: rmdir [--] cartella…":"Uso: rm [-rR] [-f] [--] percorso…");
    for(;k<A.argc;k++){
        const char *arg=V(k),*last=arg;unsigned len=strlen(arg);
        while(len && arg[len-1]=='/')len--;
        for(unsigned j=0;j<len;j++)if(arg[j]=='/')last=arg+j+1;
        unsigned leaflen=arg+len-last;
        int r=-REMOVE_BAD_PATH;
        if(!len || (leaflen==1 && last[0]=='.') || (leaflen==2 && last[0]=='.' && last[1]=='.'))goto error;
        if(path(pa,k))goto error;
        Dirent st;
        if(sc(S_STAT,pa,&st,0,0)){
            if(force)continue;
            goto error;
        }
        r=sc(S_REMOVE,pa,flags,0,0);
        if(!r)continue;
error:
        say(directory?"rmdir: ":"rm: ");say(arg);say(": ");
        ln(r==-REMOVE_NOT_EMPTY?"cartella non vuota (usa rm -r per eliminarne il contenuto)":
           r==-REMOVE_READ_ONLY?"sola lettura":r==-REMOVE_WRONG_TYPE?(directory?"non è una cartella":"è una cartella: usa rm -r"):
           r==-REMOVE_IO?"errore di scrittura":"percorso non valido, protetto o inesistente");status=1;
    }
    return status;
}
#include "sysstat.h"
static int run(void)
{
    if(IS("fdesetup")){
        VaultRequest q={.disk=-1};VaultInfo vi={0};
        if(eq(V(1),"status")){sc(S_FILEVAULT,0,&q,&vi,0);ln(vi.error?"FileVault: volume non disponibile.":!vi.enabled?"FileVault is Off.":vi.locked?"FileVault is On. Volume locked.":vi.state==1?"FileVault encryption in progress.":vi.state==3?"FileVault decryption in progress.":"FileVault is On.");return vi.error?1:0;}
        if(A.argc!=2||!eq(V(1),"manage"))return fail("Uso: fdesetup status | manage");
        if(sc(S_SPAWN,SETTINGS,"@vault",0,0)<0)return fail("Impossibile aprire Impostazioni.");
        ln("FileVault aperto in Impostazioni.");return 0;
    }

    if(IS("sysstat"))return sysstat();
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
            ln("ProductVersion:    10.2.6");
            ln("BuildVersion:      30C1");
        } else if (n == 2 && eq(V(1), "-productVersion"))ln("10.2.6");
        else if (n == 2 && eq(V(1), "-buildVersion"))ln("30C1");
        else if (n == 2 && eq(V(1), "-productName"))ln("tinymacOS");
        else return 2;
        return 0;
    }
    if (IS("uname")) {
        if (n == 1 || eq(V(1), "-s"))ln("tinymacOS");
        else if (eq(V(1), "-m"))ln("x86_64");
        else if (eq(V(1), "-r"))ln("10.2.6");
        else if (eq(V(1), "-a"))ln("tinymacOS tramontana 10.2.6 30C1 x86_64");
        else return 2;
        return 0;
    }
    if (IS("csrutil")) {
        if (n == 1 || (n == 2 && eq(V(1), "status"))) {
            ln((t.csr & 1) ? "System Integrity Protection: attiva."
                           : "System Integrity Protection: disattiva.");
            return 0;
        }
        if (n == 2 && (eq(V(1), "enable") || eq(V(1), "disable"))){
            if(!t.recovery)return fail("csrutil: solo la Shell di Recupero puo modificare SIP (riavviare premendo R).");
            if(sc(S_BOOTCONFIG,3,eq(V(1),"enable"),0,0)<0)return fail("csrutil: nessun disco tinymacOS con configurazione scrivibile.");
            ln("Preferenza SIP verificata sul primo disco tinymacOS. Riavvia per applicarla.");return 0;
        }
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
        if(n>=2&&eq(V(1),"exportExFAT")){
            if((n!=4&&n!=5)||path(pa,2)||(n==5&&!eq(V(4),"--confirm")))return fail("Uso: diskutil exportExFAT /.snapshots/nome diskN [--confirm]");
            VolumeExport q={0};strset(q.source,pa,PATHLEN);strset(q.destination,V(3),PATHLEN);
            int result=sc(S_DISK,14,&q,0,0);ln(q.message);if(result!=1)return 1;
            say("Nuova partizione, LBA: ");num(q.part.start);say("; byte: ");num(q.bytes);ln("");
            if(n==4)return 0;q.part.confirm=DISK_EDIT_CONFIRM;result=sc(S_DISK,14,&q,0,0);ln(q.message);if(!result){say("Volume exFAT: ");ln(q.destination);}return !!result;
        }
        static DiskInfo di;
        if (sc(S_DISK, 0, &di, 0, 0))return 1;
        if(n>=2&&eq(V(1),"importBackup")){
            if((n!=5&&n!=6)||(n==6&&!eq(V(5),"--confirm")))return fail("diskutil importBackup diskS diskD <checkpoint> [--confirm] (solo Recupero)");
            DiskImport q={.source=diskid(V(2)),.destination=diskid(V(3))};
            if(q.source<0||q.destination<0||strlen(V(4))>=sizeof q.checkpoint)return fail("Dischi o nome checkpoint non validi.");
            strset(q.checkpoint,V(4),sizeof q.checkpoint);
            int r=sc(S_DISK,12,&q,0,0);ln(q.message);if(r)return 1;
            say("Spazio di staging: ");num(q.staging_blocks*16384);ln(" byte.");
            if(n==5)return 0;
            q.confirm=DISK_IMPORT_CONFIRM;r=sc(S_DISK,13,&q,0,0);ln(q.message);return !!r;
        }
        if ((n==2||n==3)&&eq(V(1),"list")) {
            int only=n==3?diskid(V(2)):-1;if(n==3&&only<0)return fail("Identificatore disco non valido.");
            for(int i=0;i<di.count;i++){
                if(only>=0&&i!=only)continue;
                DiskMap*m=&di.disk[i].map;
                say("/dev/disk");num(i);say(" (internal, physical): ");char capacity[40];display_size(capacity,sizeof capacity,(u64)di.disk[i].sectors*512);say(capacity);say(" · ");ln(partition_scheme(m->scheme));
                for(int j=0;j<m->count;j++){DiskPartition*p=m->part+j;
                    say("  disk");num(i);say("s");num(p->number);say("  ");say(filesystem_name(p->fs));say("  ");display_size(capacity,sizeof capacity,(u64)p->sectors*512);say(capacity);say("  LBA ");num(p->start);say("  ");ln(p->name);
                }
                for(int j=0;j<m->gaps;j++){say("  Spazio non allocato  ");display_size(capacity,sizeof capacity,(u64)m->gap[j].sectors*512);say(capacity);say("  LBA ");num(m->gap[j].start);ln("");}
                if(!m->safe){say("  Protetto: ");ln(m->reason);}
            }
            if(!di.count)ln("Nessun disco collegato.");return 0;
        }
        if(n>=2&&(eq(V(1),"deletePartition")||eq(V(1),"addPartition")||eq(V(1),"resizeVolume"))){
            int create=eq(V(1),"addPartition"),del=eq(V(1),"deletePartition");
            int args=create?6:del?3:4,apply=n==args+1&&eq(V(args),"--confirm");
            if(n!=args&&!apply)return fail("Uso: diskutil deletePartition diskNsM [--confirm] | resizeVolume diskNsM MB [--confirm] | addPartition diskN LBA MB nome [--confirm]");
            DiskEdit q={.op=create?DISK_EDIT_CREATE:del?DISK_EDIT_DELETE:DISK_EDIT_RESIZE};
            if(create){
                q.disk=diskid(V(2));u64 mb;
                if(q.disk<0||q.disk>=di.count||disk_lba(V(3),&q.start)||disk_size_mb(V(4),&mb))return fail("Disco, LBA o dimensione non validi.");
                q.size=mb;strset(q.name,V(5),sizeof q.name);
            }else{
                if(disk_partition_id(V(2),&q.disk,&q.number)||q.disk>=di.count)return fail("Identificatore partizione non valido.");
                DiskPartition*p=0;DiskMap*m=&di.disk[q.disk].map;
                for(int j=0;j<m->count;j++)if(m->part[j].number==q.number)p=m->part+j;
                if(!p)return fail("Partizione non trovata.");
                q.start=p->start;q.sectors=p->sectors;
                if(!del){u64 mb;if(disk_size_mb(V(3),&mb))return fail("Dimensione MB non valida.");q.size=mb;}
            }
            int r=sc(S_DISK,10,&q,0,0);ln(q.message);if(r)return 1;
            if(!del){char size[40];disk_size_mb_exact(size,sizeof size,q.size);say("Dimensione effettiva: ");say(size);ln(" MB (allineata per difetto)");}
            if(!apply){ln("Solo anteprima: nessuna scrittura. Esegui un backup; aggiungi --confirm per applicare.");return 0;}
            q.confirm=DISK_EDIT_CONFIRM;r=sc(S_DISK,11,&q,0,0);ln(q.message);return !!r;
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
                const char*format="Filesystem nativo";if(di.current>=0&&di.current<di.count){DiskMap*m=&di.disk[di.current].map;for(int j=0;j<m->count;j++)if(m->part[j].start==di.disk[di.current].region_start[1]&&filesystem_native(m->part[j].fs))format=filesystem_name(m->part[j].fs);}
                say("Contenitore cont0 — ");ln(format);
                say("  Volume \"Macintosh HD\": ");
                char usage[96];display_usage(usage,sizeof usage,fi.used,fi.total);say(usage);
                say(" · generazione ");
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
                    ln("Snapshot valido: integrità verificata.");
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
            const char*id=V(2);if(!memcmp(id,"/dev/",5))id+=5;
            int partno=-1,i=-1;
            if(!memcmp(id,"disk",4)&&id[4]>='0'&&id[4]<='7'){
                i=id[4]-'0';if(id[5]){if(id[5]!='s'||!id[6])return 1;partno=0;for(int k=6;id[k];k++){if(id[k]<'0'||id[k]>'9'||partno>128)return 1;partno=partno*10+id[k]-'0';}}
            }
            if(i<0||i>=di.count)return 1;
            DiskMap*m=&di.disk[i].map;
            if(eq(V(1),"info")){
                if(partno>=0){
                    DiskPartition*p=0;for(int j=0;j<m->count;j++)if(m->part[j].number==partno)p=m->part+j;
                    if(!p)return fail("Partizione non trovata.");
                    ln(p->name);say("Filesystem: ");ln(filesystem_name(p->fs));say("Inizio LBA: ");num(p->start);say(" · settori: ");num(p->sectors);ln("");
                    ln("Riconoscimento in sola lettura. RAW non significa spazio libero.");
                }else{
                    ln(di.disk[i].model);char capacity[40];display_size(capacity,sizeof capacity,(u64)di.disk[i].sectors*512);say(capacity);say(" · ");ln(partition_scheme(m->scheme));ln(m->reason);
                }
                return 0;
            }
            if(partno>=0)return fail("SOS delle partizioni esterne non supportato: nessuna scrittura.");
            if (eq(V(1), "verifyVolume") || eq(V(1), "repairVolume")) {
                int r = sc(S_DISK, eq(V(1), "repairVolume") ? 1 : 4, i, 0, 0);
                ln(r ? "Verifica non riuscita." : "Il volume risulta valido.");
                return !!r;
            }
        }

        return fail("diskutil list | info diskN | verifyVolume diskN | repairVolume diskN | deletePartition | addPartition | resizeVolume (senza --confirm: anteprima)");
    }
    if (IS("fsck_apfs")) {
        static DiskInfo di;
        if (sc(S_DISK, 0, &di, 0, 0))return 1;
        int repair = 0;
        for (int i = 1; i < A.argc; i++)if (eq(V(i), "-y") || eq(V(i), "-f"))repair = 1;
        int dsk = di.current;
        if (dsk < 0 || dsk >= di.count || !di.disk[dsk].layout)
            return fail("fsck_apfs: nessun volume nativo montato su questo disco.");
        ln(repair ? "Riparazione del volume in corso\u2026" : "Verifica del volume in corso\u2026");
        int r = sc(S_DISK, repair ? 1 : 4, dsk, 0, 0);
        if (r)return fail("fsck_apfs: controllo non riuscito; riprova con -y o avvia Recupero.");
        ln(repair ? "Ripristino completato: il volume è valido." : "Il volume risulta valido.");
        return 0;
    }
    if (IS("mount_apfs")) {
        static DiskInfo di;
        if (sc(S_DISK, 0, &di, 0, 0))return 1;
        int dsk = di.current;
        if (dsk >= 0 && dsk < di.count && di.disk[dsk].layout) {
            const char*format="Filesystem nativo";DiskMap*m=&di.disk[dsk].map;for(int j=0;j<m->count;j++)if(m->part[j].start==di.disk[dsk].region_start[1]&&filesystem_native(m->part[j].fs))format=filesystem_name(m->part[j].fs);
            say("Macintosh HD - Dati è già montato su / — ");ln(format);
            return 0;
        }
        return fail("mount_apfs: nessun volume da montare. Vedi «diskutil list».");
    }
    if (IS("newfs_apfs")) {
        ln("newfs_apfs: l'inizializzazione del disco richiede l'Installatore tinymacOS.");
        ln("Avvia «" INSTALLER_TITLE "» e usa Utility Disco → Inizializza disco.");
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
    if (IS("rm") || IS("rmdir"))return remove_command(IS("rmdir"));
    if(IS("pbcopy")||IS("pbpaste")){
        static Clipboard q;
        if(n!=1)return fail("Uso: comando | pbcopy oppure pbpaste");
        if(IS("pbcopy")){if(A.inlen>=CLIP_BYTES)return fail("Testo troppo lungo per gli appunti (262.1 kB).");memset(&q,0,sizeof q);q.kind=CLIP_TEXT;memcpy(q.data,A.in,A.inlen);if(strlen(q.data)!=A.inlen)return fail("Gli appunti accettano testo senza byte NUL.");return sc(S_CLIPBOARD,1,&q,0,0)<0;}
        if(sc(S_CLIPBOARD,0,&q,0,0)<0||q.kind!=CLIP_TEXT)return fail("Nessun testo negli appunti.");emit(q.data,strlen(q.data));return 0;
    }
    if (n < 2 || path(pa, 1))return 2;
    if (IS("cp") || IS("mv")) {
        int recursive=IS("cp")&&n==4&&(eq(V(1),"-r")||eq(V(1),"-R")),at=recursive?2:1;
        if(n!=at+2||path(pa,at)||path(pb,at+1)||sc(S_STAT,pa,&d,0,0))return fail("Uso: cp [-R] origine destinazione | mv origine destinazione");
        if(!sc(S_STAT,pb,&b,0,0)&&b.dir){char dest[PATHLEN];if(absolutepath(dest,pb,leaf(pa)))return 1;strset(pb,dest,PATHLEN);}
        if(eq(pa,pb))return fail("Origine e destinazione coincidono.");
        if(IS("mv"))return sc(S_MOVE,pa,pb,0,0)<0?fail("Spostamento non riuscito: destinazione esistente, permessi o ciclo."):0;
        if(d.dir&&!recursive)return fail("Per copiare cartelle usa cp -R.");
        int r=fileop_copy(pa,pb);return r<0?fail(r==FILEOP_EXISTS?"Destinazione esistente: nessun file sovrascritto.":r==FILEOP_CYCLE?"Copia rifiutata: origine uguale o cartella discendente.":"Copia non completata: verifica spazio e permessi."):0;
    }
    for (int k = 1; k < n; k++) {
        if (path(pa, k))return 1;
        if (IS("mkdir")) {
            if (sc(S_CREATE, pa, 1, 0, 0) < 0)return 1;
        } else if (IS("touch")) {
            if (sc(S_TOUCH, pa, 0, 0, 0) < 0)return 1;
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
            r = fail("Risultato troppo grande per questa sessione (8192 byte).");
        } else if (r && !nout && !(IS("grep") && r == 1)) {
            say(V(0));
            say(": ");
            ln(r == 2 ? "argomenti non validi" : "verifica percorso, permessi o spazio disponibile");
        }
        if (nout)sc(S_STDOUT, out, nout, 0, 0);
    }
    sc(S_RETURN, r, 0, 0, 0);
}
