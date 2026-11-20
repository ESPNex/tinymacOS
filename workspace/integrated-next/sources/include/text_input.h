#pragma once
#include "base.h"
/* Event.a carries a Unicode scalar, not a byte. Reserved key codes 128..159
 * remain controls. Buffer limits are bytes, caret/deletion are UTF-8 scalars. */
static inline int text_printable(int c){return (c>=32&&c<=126)||(c>=160&&c<=0x10ffff&&!(c>=0xd800&&c<=0xdfff));}
static inline int text_encode(int c,char out[4])
{
    if(!text_printable(c)&&c!='\n'&&c!='\t')return 0;
    if(c<128){out[0]=c;return 1;}
    if(c<2048){out[0]=0xc0|(c>>6);out[1]=0x80|(c&63);return 2;}
    if(c<65536){out[0]=0xe0|(c>>12);out[1]=0x80|((c>>6)&63);out[2]=0x80|(c&63);return 3;}
    out[0]=0xf0|(c>>18);out[1]=0x80|((c>>12)&63);out[2]=0x80|((c>>6)&63);out[3]=0x80|(c&63);return 4;
}
static inline int text_prev(const char *s,int at){if(at>0){at--;while(at>0&&((u8)s[at]&0xc0)==0x80)at--;}return at;}
static inline int text_next(const char *s,int at){if(s[at]){at++;while(((u8)s[at]&0xc0)==0x80)at++;}return at;}
static inline int text_insert(char *s,int capacity,int *at,int c)
{
    char b[4];int z=text_encode(c,b),n=strlen(s);if(!z||*at<0||*at>n||n+z>=capacity)return 0;
    memmove(s+*at+z,s+*at,n-*at+1);memcpy(s+*at,b,z);*at+=z;return 1;
}
static inline void text_backspace(char *s,int *at){int p=text_prev(s,*at);memmove(s+p,s+*at,strlen(s+*at)+1);*at=p;}
static inline void text_delete(char *s,int at){int p=text_next(s,at);memmove(s+at,s+p,strlen(s+p)+1);}
static inline int text_append_key(char *s,int capacity,int c){int at=strlen(s);if(c==8){text_backspace(s,&at);return 1;}return text_printable(c)?text_insert(s,capacity,&at,c):0;}
static inline int text_field_length(const char *s,int capacity)
{
    int i=0;
    while(i<capacity&&s[i]){
        unsigned c=(u8)s[i++],need=0,min=0;
        if(c>=0xc2&&c<=0xdf){c&=31;need=1;min=128;}
        else if(c>=0xe0&&c<=0xef){c&=15;need=2;min=2048;}
        else if(c>=0xf0&&c<=0xf4){c&=7;need=3;min=65536;}
        else if(c>=128)return -1;
        while(need--){if(i>=capacity||((u8)s[i]&0xc0)!=0x80)return -1;c=(c<<6)|((u8)s[i++]&63);}
        if(c<min||!text_printable(c))return -1;
    }
    return i<capacity?i:-1;
}
/* Byte boundaries <-> fixed-cell columns, never split a multibyte scalar. */
static inline int text_columns(const char *s,int end){int n=0;for(int i=0;i<end&&s[i];i=text_next(s,i))n++;return n;}
static inline int text_offset(const char *s,int start,int end,int columns){while(columns-->0&&start<end)start=text_next(s,start);return MIN(start,end);}
