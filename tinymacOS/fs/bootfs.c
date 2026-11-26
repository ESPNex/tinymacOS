#include "../kernel/k.h"
#define BASE 1024
#define COUNT_SYS 384
// Read-only namespace. Each file points only to an actual boot object retained as
// EfiLoaderData, never to live supervisor pages, heaps, stacks, or a user pointer.
typedef struct {
    int parent, dir;
    const u8 *data;
    u32 len;
    char name[48];
} BNode;
static BNode tree[COUNT_SYS];
static int count;
static int add(int par, const char *name, int dir, const u8 *p, u32 n)
{
    if (count == COUNT_SYS || strlen(name) > 47 || !name[0])return -1;
    int i = count++;
    tree[i] = (BNode) {
        par, dir, p, n, {0}
    };
    strset(tree[i].name, name, 48);
    return i;
}
static int child(int par, const char *name)
{
    for (int i = 1; i < count; i++)if (tree[i].parent == par && eq(tree[i].name, name))return i;
    return -1;
}
void bootfs_init(void)
{
    const u8 *v = (void *)boot.volume;
    u64 z = boot.volume_len;
    if (z < 16 || memcmp(v, "TMVOL2", 7))panic("system volume", 0, 0);
    u32 n = ((const u32 *)v)[2];
    if (!n || n > COUNT_SYS - 4 || ((const u32 *)v)[3] != 64 || 16 + 64 * n > z)panic("catalog", 0, 0);
    for (u32 i = 0; i < n; i++) {
        const u32 *e = (void *)(v + 16 + 64 * i);
        const char *name = (const char *)(e + 4);
        if (name[47] || e[1] > 1 || (i && e[0] >= i) || e[2] > z || e[3] > z - e[2] || (!e[1] &&
                e[2] < 16 + 64 * n))panic("system entry", 0, 0);
        if (!i) {
            tree[0] = (BNode) {
                .parent = -1, .dir = 1
            };
            count = 1;
        } else if (add(e[0], name, e[1], v + e[2], e[3]) != (int)i)panic("system entry", 0, 0);
    }
    for (int i = 0; i <= (boot.recovery?0:NWALL); i++) {
        char path[PATHLEN] = "/System/Library/Fonts/Interface.tfn";
        if (i) {
            strset(path, "/System/Library/Desktop Pictures/", PATHLEN);
            append(path, wallname(i - 1), PATHLEN);
            append(path, ".qoi", PATHLEN);
        }
        const u8 *b;
        u32 s;
        if (!bootfile(path, &b, &s) || s < 16 || s > (i ? 4 : 3) * (1u << 20))panic("resource", 0, 0);
    }
    add(bootfs_lookup(KCDIR) - BASE, "KernelCache", 0, (void *)boot.image, boot.image_len);
    add(bootfs_lookup(CORE) - BASE, "boot.efi", 0, (void *)boot.efi, boot.efi_len);
}
void execpath(int a, char *p)
{
    if (a == DESKTOP) {
        strset(p, CORE "/WindowServer", PATHLEN);
        return;
    }
    strset(p, apploc(a), PATHLEN);
    append(p, "/", PATHLEN);
    append(p, bundlename(a), PATHLEN);
    append(p, ".app/Contents/MacOS/", PATHLEN);
    append(p, bundlename(a), PATHLEN);
}
int bootfile(const char *p, const u8 **b, u32 *z)
{
    int i = bootfs_lookup(p) - BASE;
    if (i < 0 || i >= count || tree[i].dir)return 0;
    *b = tree[i].data;
    *z = tree[i].len;
    return 1;
}
// -2 = other backend; -1 = missing here. Normalize repeated separators and dot components.
int bootfs_lookup(const char *path)
{
    if (!count || *path != '/')return -2;
    char q[PATHLEN];
    int used = 0;
    while (*path) {
        while (*path == '/')path++;
        if (!*path)break;
        const char *p = path;
        while (*path && *path != '/')path++;
        int n = path - p;
        if (n == 1 && *p == '.')continue;
        if (n == 2 && p[0] == '.' && p[1] == '.') {
            while (used && q[used - 1] != '/')used--;
            if (used)used--;
            continue;
        }
        if (n > 47 || used + 1 + n >= PATHLEN)return -1;
        q[used++] = '/';
        memcpy(q + used, p, n);
        used += n;
    }
    q[used] = 0;
    int par = 0;
    if (!used)return -2;
    char *p = q;
    while (*p) {
        char *start = ++p;
        while (*p && *p != '/')p++;
        char saved = *p;
        *p = 0;
        int old = par;
        par = child(par, start);
        *p = saved;
        if (par < 0)return old ? -1 : -2;
    }
    return BASE + par;
}
static int indexof(int id)
{
    int i = id - BASE;
    return i >= 0 && i < count ? i : -1;
}
int bootfs_stat(int id, Dirent *d)
{
    int i = indexof(id);
    if (i < 0)return -1;
    BNode *n = tree + i;
    *d = (Dirent) {
        id, i ? BASE + n->parent : 1, n->dir, 1, n->len, {0}
    };
    strset(d->name, n->name, 48);
    return 0;
}
int bootfs_list(int id, Dirent *d, int cap, int skip)
{
    int a = indexof(id), n = 0;
    if (a < 0 || !tree[a].dir)return -1;
    for (int i = 1; i < count && n < cap; i++)if (tree[i].parent == a) {
            if (skip)skip--;
            else bootfs_stat(i + BASE, d + n++);
        }
    return n;
}
int bootfs_open(Process *p, int id, int write)
{
    int i = indexof(id);
    if (i < 0 || tree[i].dir || write)return -1;
    for (int j = 0; j < 12; j++)if (!p->fd[j].id) {
            p->fd[j] = (FD) {
                id, 0, 0, 0
            };
            return j;
        }
    return -1;
}
int bootfs_read(FD *f, void *b, int n)
{
    int i = indexof(f->id);
    if (i < 0 || tree[i].dir || f->at > tree[i].len)return -1;
    n = MIN((u32)n, tree[i].len - f->at);
    if (n)memcpy(b, tree[i].data + f->at, n);
    f->at += n;
    return n;
}
