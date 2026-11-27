#pragma once
#include "base.h"
/* C_MONO carries up to 99 one-byte glyph references, not UTF-8 bytes.
 * Reference 0 is invalid, 1..207 index the locked TFN2 repertoire. Keeping
 * Draw and NCMDS unchanged bounds full-window text without enlarging buffers.
 * Host tests compare this mapping against the actual production font atlas. */
static inline unsigned mono_code(unsigned glyph)
{
    static const unsigned extras[]={0x2026,0x2190,0x2192,0x2191,0x2193,0x2318,0x2022,0x2212,0x25b8,0x2315,0x2713,0x2019,0x2013,0x2014,0x232b,0x20ac};
    if(glyph<95)return glyph+32;
    if(glyph<191)return glyph-95+160;
    return glyph<207?extras[glyph-191]:'?';
}
static inline unsigned mono_reference(unsigned code)
{
    if(code>=32&&code<=126)return code-32+1;
    if(code>=160&&code<=255)return code-160+96;
    for(unsigned i=191;i<207;i++)if(mono_code(i)==code)return i+1;
    return '?'-32+1;
}
/* Consume at most 'available' bytes and never inspect beyond that range.
 * Invalid/truncated UTF-8 consumes one byte, rendered as the existing '?' glyph. */
static inline unsigned mono_decode(const char *s,int available,int *used)
{
    unsigned c=(u8)s[0],min=0;int n=1;
    *used=1;
    if(c<128)return c;
    if(c>=0xc2&&c<=0xdf){c&=31;n=2;min=128;}
    else if(c>=0xe0&&c<=0xef){c&=15;n=3;min=2048;}
    else if(c>=0xf0&&c<=0xf4){c&=7;n=4;min=65536;}
    else return '?';
    if(n>available)return '?';
    for(int i=1;i<n;i++){if(((u8)s[i]&0xc0)!=0x80)return '?';c=(c<<6)|((u8)s[i]&63);}
    if(c<min||c>0x10ffff||(c>=0xd800&&c<=0xdfff))return '?';
    *used=n;return c;
}
