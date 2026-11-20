#pragma once
#include "ui.h"
static inline int cmdpath(char *out, const char *cwd, const char *name)
{
    Dirent d;
    if (!name || !*name)return -1;
    for (const char *q = name; *q; q++)if (*q == '/') {
            return absolutepath(out, cwd, name) || sc(S_STAT, out, &d, 0, 0) || d.dir ? -1 : 0;
        }
    const char *dirs[] = {"/bin", "/usr/bin", "/usr/sbin", "/sbin"};
    for (int i = 0; i < 4; i++) {
        if (!absolutepath(out, dirs[i], name) && !sc(S_STAT, out, &d, 0, 0) && !d.dir)return 0;
    }
    return -1;
}
static inline int expandpath(char *out, const char *cwd, const char *s)
{
    if (s && s[0] == '~' && (!s[1] || s[1] == '/')) {
        if (!s[1]) {
            strset(out, HOME, PATHLEN);
            return 0;
        }
        return absolutepath(out, HOME, s + 2);
    }
    return absolutepath(out, cwd, s);
}
