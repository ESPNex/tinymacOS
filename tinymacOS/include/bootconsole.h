#pragma once
#include "handoff.h"
/* Tiny allocation-free GOP console shared by EFI loader and kernel. Grayscale
 * pixels work in RGB/BGR. Does not call firmware after ExitBootServices. */
typedef struct {unsigned col,row;} BootConsole;
static const unsigned char boot_font[]={
#include "../assets/panicfont.inc"
};
static inline void bootconsole_clear(const struct Boot *b,BootConsole *c)
{
    c->col=c->row=0;
    if(!b->fb)return;
    volatile uint32_t *p=(void *)b->fb;
    for(unsigned y=0;y<b->h;y++)for(unsigned x=0;x<b->w;x++)p[y*b->stride+x]=0;
}
static inline void bootconsole_put(const struct Boot *b,BootConsole *c,const char *s)
{
    if(!b->fb||b->w<32||b->h<32)return;
    unsigned cols=(b->w-16)/9,rows=(b->h-16)/16;
    volatile uint32_t *fb=(void *)b->fb;
    for(;*s;s++){
        unsigned ch=(unsigned char)*s;
        if(ch=='\r'){c->col=0;continue;}
        if(ch=='\n'){c->col=0;c->row++;}
        else if(ch>=32&&ch<=126){
            if(c->col>=cols){c->col=0;c->row++;}
            if(c->row>=rows){
                for(unsigned y=8;y<8+(rows-1)*16;y++)for(unsigned x=8;x<8+cols*9;x++)fb[y*b->stride+x]=fb[(y+16)*b->stride+x];
                for(unsigned y=8+(rows-1)*16;y<8+rows*16;y++)for(unsigned x=8;x<8+cols*9;x++)fb[y*b->stride+x]=0;
                c->row=rows-1;
            }
            unsigned x=8+c->col++*9,y=8+c->row*16;
            for(unsigned j=0;j<15;j++)for(unsigned i=0;i<8;i++)fb[(y+j)*b->stride+x+i]=(boot_font[(ch-32)*15+j]&(1u<<i))?0xeeeeee:0;
        }
    }
    __asm__ volatile("sfence":::"memory");
}
