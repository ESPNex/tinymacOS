#include <assert.h>
#include <stdio.h>
#include "../include/display_size.h"
static void check(u64 n,const char *expected){char s[64];display_size(s,sizeof s,n);assert(eq(s,expected));}
int main(void)
{
    check(0,"0 B");check(999,"999 B");check(1000,"1.0 kB");
    check(1024,"1.0 kB");check(999999,"999.9 kB");
    check(1000000,"1.0 MB");check(1048576,"1.0 MB");
    check(1000000000ull,"1.0 GB");check(10737418240ull,"10.7 GB");
    check(1000000000000ull,"1.0 TB");check(999999999999999ull,"999.9 TB");
    check(~(u64)0,"18.4 EB");
    for(int cap=1;cap<=32;cap++){
        unsigned char s[40];memset(s,0xa5,sizeof s);
        display_size((char*)s,cap,~(u64)0);
        assert(s[cap-1]==0||strlen((char*)s)<(unsigned)cap);
        for(unsigned i=cap;i<sizeof s;i++)assert(s[i]==0xa5);
    }
    puts("PASS decimal sizes: B/kB/MB/GB/TB/PB/EB, SI numeric conversion, truncation at boundaries, UINT64_MAX, bounded destinations");
}
