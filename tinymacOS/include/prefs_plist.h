#pragma once
#include "base.h"
/* Property-list XML on disk, not a private blob with a .plist suffix.
 * This bounded schema reader accepts integer values and XML whitespace; it
 * deliberately rejects duplicate/missing keys, overflow and invalid values. */
static const char *const prefs_keys[]={"Theme","Wallpaper","Dock","ReduceMotion","Opaque","Dark"};
static inline int prefs_put(char *out,int cap,int *at,const char *s)
{
    int n=strlen(s);if(n>=cap-*at)return 0;memcpy(out+*at,s,n);*at+=n;out[*at]=0;return 1;
}
static inline int prefs_plist_write(char *out,int cap,const u8 values[6])
{
    int at=0;if(cap<1)return -1;out[0]=0;
    if(!prefs_put(out,cap,&at,"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n<plist version=\"1.0\">\n<dict>\n"))return -1;
    for(int i=0;i<6;i++){
        char digits[4];int v=values[i],n=0;if(v>=100)digits[n++]='0'+v/100;if(v>=10)digits[n++]='0'+v/10%10;digits[n++]='0'+v%10;digits[n]=0;
        if(!prefs_put(out,cap,&at,"  <key>")||!prefs_put(out,cap,&at,prefs_keys[i])||!prefs_put(out,cap,&at,"</key><integer>")||!prefs_put(out,cap,&at,digits)||!prefs_put(out,cap,&at,"</integer>\n"))return -1;
    }
    if(!prefs_put(out,cap,&at,"</dict>\n</plist>\n"))return -1;return at;
}
static inline const char *prefs_find(const char *s,const char *needle)
{
    size_t n=strlen(needle),left=strlen(s);for(;left>=n;s++,left--)if(!memcmp(s,needle,n))return s;return 0;
}
static inline const char *prefs_space(const char *s){while(*s==' '||*s=='\n'||*s=='\r'||*s=='\t')s++;return s;}
static inline int prefs_plist_read(const char *s,u8 values[6])
{
    const char *plist=prefs_find(s,"<plist version=\"1.0\">"),*end=prefs_find(s,"</dict>");
    if(!plist||!end||!prefs_find(end,"</plist>"))return 0;
    const char *p=prefs_find(plist,"<dict>");if(!p||p>end)return 0;p+=6;
    u8 v[6];unsigned seen=0;
    while((p=prefs_space(p))<end){
        if(end-p<5||memcmp(p,"<key>",5))return 0;p+=5;int k=-1;
        for(int i=0;i<6;i++){size_t n=strlen(prefs_keys[i]);if((size_t)(end-p)>=n+6&&!memcmp(p,prefs_keys[i],n)&&!memcmp(p+n,"</key>",6)){k=i;p+=n+6;break;}}
        if(k<0||(seen&(1u<<k)))return 0;p=prefs_space(p);
        if(end-p<9||memcmp(p,"<integer>",9))return 0;p=prefs_space(p+9);
        unsigned n=0,count=0;while(*p>='0'&&*p<='9'){if(++count>3)return 0;n=n*10+*p++-'0';}
        p=prefs_space(p);if(!count||n>255||end-p<10||memcmp(p,"</integer>",10))return 0;p+=10;
        if((k>=2&&n>1)||(k==0&&n>=8)||(k==1&&n>=13))return 0;v[k]=n;seen|=1u<<k;
    }
    if(seen!=63)return 0;memcpy(values,v,6);return 1;
}
