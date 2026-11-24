/* Execute the actual userspace metric reader with bounded, partial file I/O. */
#include "../userspace/ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
static unsigned char *input;
static size_t input_size,position;
static int opened,closed,denied;
static long mockcall(long n,u64 a,u64 b,u64 c,u64 d)
{
    (void)d;
    if(n==S_OPEN){assert(eq((char *)a,"/System/Library/Fonts/Interface.tfn"));assert(!b);opened++;return denied?-1:7;}
    if(n==S_CLOSE){assert(a==7);closed++;return 0;}
    assert(n==S_READ&&a==7&&c<=MAXFILE);
    size_t len=MIN((size_t)c,MIN((size_t)37,input_size-position));
    memcpy((void *)b,input+position,len);position+=len;return len;
}
#undef sc
#define sc(n,a,b,c,d) mockcall(n,(u64)(a),(u64)(b),(u64)(c),(u64)(d))
#include "../userspace/fontmetrics.c"
int eq(const char *a,const char *b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static void reset(void){loaded=valid=count=faces=0;position=0;opened=closed=denied=0;}
int main(int argc,char **argv)
{
    assert(argc==3);FILE *f=fopen(argv[1],"rb");assert(f);fseek(f,0,SEEK_END);input_size=ftell(f);rewind(f);
    input=malloc(input_size);assert(input&&fread(input,1,input_size,f)==input_size);fclose(f);
    assert(TextWidth("tiny",F_DISPLAY)==atoi(argv[2]));assert(opened==1&&closed==1);
    assert(TextWidth("",F_DISPLAY)==0);assert(TextWidth("1920 × 1080",F_SMALL)>0);
    assert(TextWidth("✓",F_SMALL)>0);assert(TextWidth("macOS",F_DISPLAY_SEMIBOLD)>0);
    assert(TextWidth("x",-1)==-1&&TextWidth("x",NFONTS)==-1);assert(opened==1);
    reset();unsigned char saved=input[0];input[0]='X';assert(TextWidth("tiny",F_DISPLAY)==-1&&closed==1);input[0]=saved;
    reset();size_t length=input_size;input_size=10;assert(TextWidth("tiny",F_DISPLAY)==-1&&closed==1);input_size=length;
    reset();denied=1;assert(TextWidth("tiny",F_DISPLAY)==-1&&!closed);
    reset();unsigned char ng[4];memcpy(ng,input+8,4);memset(input+8,255,4);assert(TextWidth("tiny",F_DISPLAY)==-1&&closed==1);memcpy(input+8,ng,4);
    free(input);puts("PASS actual font metrics: loaded-family advance, UTF-8, one-open cache, partial I/O, bad/truncated/oversized header and denied-open fallback");
}
