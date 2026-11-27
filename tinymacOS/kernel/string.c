#include "base.h"
void *memset(void *d, int c, size_t n) {
    u8 *p = d;
    while (n--)
        *p++ = c;
    return d;
}
void *memcpy(void *d, const void *s, size_t n) {
    void *r = d;
    size_t q = n >> 3, t = n & 7;
    __asm__ volatile("rep movsq":"+D"(d), "+S"(s), "+c"(q)::"memory");
    __asm__ volatile("rep movsb":"+D"(d), "+S"(s), "+c"(t)::"memory");
    return r;
}
void *memmove(void *d, const void *s, size_t n) {
    if ((uintptr_t)d<=(uintptr_t)s)
        return memcpy(d, s, n);
    while (n) {
        n--;
        ((u8 *)d)[n]=((const u8 *)s)[n];
    }
    return d;
}
int memcmp(const void *a, const void *b, size_t n) {
    for (size_t i=0; i<n; i++)
        if (((u8 *)a)[i]!=((u8 *)b)[i])
            return ((u8 *)a)[i]-((u8 *)b)[i];
    return 0;
}
int strncmp(const char *a, const char *b, size_t n) {
    for (; n; n--, a++, b++) {
        unsigned char ca = (unsigned char)*a, cb = (unsigned char)*b;
        if (ca != cb) return ca < cb ? -1 : 1;
        if (!ca) return 0;
    }
    return 0;
}
size_t strlen(const char *s) {
    size_t n=0;
    while (s[n])
        n++;
    return n;
}
int eq(const char *a, const char *b) {
    while (*a&&*a==*b) {
        a++;
        b++;
    }
    return *a==*b;
}
void strset(char *d, const char *s, int n) {
    if (n<1)
        return;
    int i=0;
    for (; i<n-1&&s[i]; i++)
        d[i]=s[i];
    d[i]=0;
}
void append(char *d, const char *s, int n) {
    int p=strlen(d);
    if (p<n)
        strset(d+p, s, n-p);
}
void number(char *d, u64 n) {
    char t[24];
    int i=0;
    do {
        t[i++]='0'+n%10;
        n/=10;
    } while (n);
    for (int k=0; k<i; k++)
        d[k]=t[i-1-k];
    d[i]=0;
}
int val(const char *s) {
    int n=0;
    while (*s>='0'&&*s<='9'&&n<10000000)
        n=n*10+*s++-'0';
    return n;
}

char *strchr(const char*s,int c){do{if((unsigned char)*s==(unsigned char)c)return (char*)s;}while(*s++);return 0;}
