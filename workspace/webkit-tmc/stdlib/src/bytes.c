/* Only the extra C byte helpers required by the selected std/dtoa consumers.
 * memcpy/memmove/memcmp/memset/strlen come from the native kernel/string.c TU.
 * Host harness renames these helpers rather than interposing on ASan/glibc. */
#include <stddef.h>
size_t strnlen(const char*s,size_t n){size_t i=0;while(i<n&&s[i])i++;return i;}
void *memchr(const void*p,int c,size_t n)
{const unsigned char*s=p;for(size_t i=0;i<n;i++)if(s[i]==(unsigned char)c)return (void*)(s+i);return 0;}
