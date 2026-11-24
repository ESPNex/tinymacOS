#include "ui.h"
/* Measure the same TFN2 advances as the compositor, without reading pixels or
 * hardcoding a particular family. Only About and Settings link this helper. */
#define METRIC_GLYPHS 256
#define METRIC_BYTES (12+4*METRIC_GLYPHS+NFONTS*(4+8*METRIC_GLYPHS))
static u8 table[METRIC_BYTES];
static int loaded, valid, count, faces;
static u16 get16(const u8 *p){return p[0]|(u16)p[1]<<8;}
static u32 get32(const u8 *p){return get16(p)|(u32)get16(p+2)<<16;}
static int readexact(int fd,u8 *p,int n)
{
    while(n){int z=sc(S_READ,fd,p,MIN(n,MAXFILE),0);if(z<=0||z>n)return 0;p+=z;n-=z;}
    return 1;
}
static int loadmetrics(void)
{
    if(loaded)return valid;
    loaded=1;
    int fd=sc(S_OPEN,"/System/Library/Fonts/Interface.tfn",0,0,0);
    if(fd<0)return 0;
    if(readexact(fd,table,12)&&!memcmp(table,"TFN2",4)){
        faces=get32(table+4);count=get32(table+8);
        if(faces>0&&faces<=NFONTS&&count>0&&count<=METRIC_GLYPHS){
            int bytes=4*count+faces*(4+8*count);
            valid=bytes<=(int)sizeof(table)-12&&readexact(fd,table+12,bytes);
        }
    }
    sc(S_CLOSE,fd,0,0,0);return valid;
}
int TextWidth(const char *s,int face)
{
    if(!s||!loadmetrics()||face<0||face>=faces)return -1;
    const u8 *glyphs=table+12+4*count+face*(4+8*count)+4;
    int width=0;
    while(*s){
        const u8 *p=(const u8 *)s;unsigned code=*p++;
        if(code>=0xc0&&code<0xe0&&(p[0]&0xc0)==0x80)code=((code&31)<<6)|(*p++&63);
        else if(code>=0xe0&&code<0xf0&&(p[0]&0xc0)==0x80&&p[1]&&(p[1]&0xc0)==0x80){code=((code&15)<<12)|((p[0]&63)<<6)|(p[1]&63);p+=2;}
        s=(const char *)p;
        int glyph=-1,question=-1;
        for(int j=0;j<count;j++){unsigned c=get32(table+12+4*j);if(c==code)glyph=j;if(c=='?')question=j;}
        if(glyph<0)glyph=question;
        if(glyph<0)return -1;
        unsigned advance=get16(glyphs+8*glyph);
        if(advance>4096||width>65536-(int)advance)return -1;
        width+=advance;
    }
    return width;
}
