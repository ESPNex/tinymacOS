#pragma once
#include "base.h"
/* One native layout: Italian ISO (it), HID usages are physical positions.
 * Four levels: base, Shift, AltGr, Shift+AltGr. No US fallback/switch. */
static inline int it_upper(int c)
{
    if(c>='a'&&c<='z')return c-32;
    if((c>=0xe0&&c<=0xf6)||(c>=0xf8&&c<=0xfe))return c-32;
    return c;
}
static inline int it_character(unsigned k,unsigned modifiers,int caps)
{
    int shift=!!(modifiers&0x22),gr=!!(modifiers&0x40),c=0;
    if(k>=4&&k<=29){c='a'+k-4;if(gr)return k==8?(shift?0xa2:0x20ac):0;return (shift^!!caps)?it_upper(c):c;}
    if(k>=30&&k<=39){
        static const u16 base[]={'1','2','3','4','5','6','7','8','9','0'};
        static const u16 upper[]={'!','"',0xa3,'$','%','&','/','(',')','='};
        if(gr){if(k==32)return shift?0:0xb3;if(k==33)return shift?0:0xbc;if(k==34)return 0x20ac;if(k==36)return '{';if(k==37)return '[';if(k==38)return ']';if(k==39)return '}';return 0;}
        return (shift?upper:base)[k-30];
    }
    if(k>=40&&k<=56){
        static const u16 base[]={10,27,8,9,' ','\'',0xec,0xe8,'+',0xf9,0xf9,0xf2,0xe0,'\\',',','.','-'};
        static const u16 upper[]={10,27,8,9,' ','?', '^',0xe9,'*',0xa7,0xa7,0xe7,0xb0,'|',';',':','_'};
        static const u16 alt[]={0,0,0,0,0,'`','~','[',']',0,0,'@','#',0,0,0,0};
        if(gr){if(k==47&&shift)return '{';if(k==48&&shift)return '}';return alt[k-40];}
        c=(shift?upper:base)[k-40];return caps?it_upper(c):c;
    }
    if(k==100)return shift?'>':'<';
    if(k>=84&&k<=88)return "/*-+\n"[k-84];
    if(k>=89&&k<=97)return '1'+k-89;
    if(k==98)return '0';
    if(k==99)return ',';
    return 0;
}
