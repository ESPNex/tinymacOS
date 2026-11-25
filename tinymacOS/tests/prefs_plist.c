#include "prefs_plist.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    char xml[1024];u8 v[6]={7,12,1,0,1,0},out[6]={0};
    int n=prefs_plist_write(xml,sizeof xml,v);assert(n>100&&n<(int)sizeof xml);
    assert(prefs_plist_read(xml,out)&&!memcmp(v,out,6));
    assert(prefs_plist_write(xml,20,v)==-1);
    assert(!prefs_plist_read("<plist version=\"1.0\"><dict><key>Theme</key><integer>9999999999</integer></dict></plist>",out));
    assert(!prefs_plist_read("<plist version=\"1.0\"><dict></dict></plist>",out));
    assert(!prefs_plist_read("xxxx",out));
    prefs_plist_write(xml,sizeof xml,v);puts(xml);
    return 0;
}
