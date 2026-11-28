/* Actual compositor wincontent/cache, actual glass and sprite, no UI facsimile. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../desktop/desk.h"
#undef sc
static long hostcall(long,u64,u64,u64,u64);
#define sc(n,a,b,c,d) hostcall(n,(u64)(a),(u64)(b),(u64)(c),(u64)(d))
#include "../desktop/main.c"
const u8 icons[16]={0},settings_svg_masks[1]={0},systemicons[1]={0}; /* no C_ICON commands in this fixture */
static long hostcall(long n,u64 a,u64 b,u64 c,u64 d){(void)b;(void)c;(void)d;if(n==S_ALLOC)return (long)malloc(a);if(n==S_FREE){free((void*)a);return 0;}assert(!"unexpected syscall");return -1;}
static u32 original[640*480],reference[640*480],texture[300*240];
static void scene(u32 ghost,u32 actual){
    for(int y=0;y<H;y++)for(int x=0;x<W;x++)canvas[y*W+x]=x<310?ghost:actual;
    for(int i=0;i<W*H;i++)original[i]=canvas[i];
}
static void intact(void){for(int i=0;i<W*H;i++)assert(canvas[i]==original[i]);}
int main(void){
    W=640;H=480;resetclip();
    View v={0};v.box=(Rect){20,40,280,230};v.s.pid=42;v.s.seq=1;
    /* Native foreground stays fixed; sheet must sample behind parent as well. */
    v.n=2;v.cmd[0]=(Draw){.type=C_RECT,.x=20,.y=40,.w=35,.h=25,.ink=0x13579b};
    v.cmd[1]=(Draw){.type=C_GLASS,.x=90,.y=55,.w=130,.h=100,.radius=9};
    Rect target={360,160,168,138};
    for(int dark=0;dark<2;dark++)for(int reduced=0;reduced<2;reduced++){
        night=dark;lite=reduced;v.key=0;
        scene(0xff00ff,0x1a8c48);assert(cache_projected(&v,100,1,target));intact();
        for(int i=0;i<v.sw*v.sh;i++)texture[i]=v.surface[i];
        /* Replacing ONLY ghosts at the old location must not affect the window. */
        scene(0x0022ff,0x1a8c48);v.key=0;assert(cache_projected(&v,102,1,target));intact();
        for(int i=0;i<v.sw*v.sh;i++)assert(texture[i]==v.surface[i]);
        assert(v.surface[(TITLE+48)*v.sw+30]==color(0x13579b));
        if(!reduced){
            scene(0x0022ff,0x8b371c);assert(cache_projected(&v,104,1,target));intact();
            assert(v.surface[35*v.sw+140]!=texture[35*v.sw+140]);
            assert(v.surface[(TITLE+100)*v.sw+140]!=texture[(TITLE+100)*v.sw+140]);
        }
        /* Even with the SAME caller key, destination geometry invalidates. */
        scene(0x0022ff,0x1a8c48);Rect other={30,300,168,138};v.key=0;
        assert(cache_projected(&v,106,1,target));u32 at=v.surface[35*v.sw+140];
        assert(cache_projected(&v,106,1,other));intact();
        if(!reduced)assert(at!=v.surface[35*v.sw+140]);
        /* Overlapping source/destination: capture-before-fill, exact restoration. */
        Rect overlap={100,100,280,230};v.key=0;
        assert(cache_projected(&v,108,1,overlap));intact();
        sprite(v.surface,v.sw,v.sh,target,10,255,0,0);
        for(int i=0;i<W*H;i++)reference[i]=canvas[i];
        for(int y=0;y<H;y++)for(int x=0;x<W;x++)if(!inside(x,y,target))assert(canvas[y*W+x]==original[y*W+x]);
        /* Resetting projection must restore ordinary window optics. */
        scene(0xff00ff,0x1a8c48);v.key=0;assert(cache(&v,110,1));intact();u32 old=v.surface[35*v.sw+140];
        scene(0x0022ff,0x1a8c48);v.key=0;assert(cache(&v,112,1));intact();if(!reduced)assert(old!=v.surface[35*v.sw+140]);
    }
    /* Bounds/halo stress at every framebuffer edge and shrinking/expanding. */
    lite=0;night=0;
    Rect edges[]={{0,0,60,60},{610,440,80,80},{-30,-20,150,140},{50,0,550,470}};
    for(unsigned k=0;k<sizeof edges/sizeof *edges;k++){scene(0x336688,0x774433);v.key=0;assert(cache_projected(&v,120+k,1,edges[k]));intact();}
    dropbuf(&v.surface);
    puts("PASS actual Mission Control optics: old-location ghosts have zero effect; destination changes real window and child sheet glass; foreground retained; light/dark/reduce-transparency; geometry rekey; overlapping scratch restored; clipped sprite and edge/halo bounds; ordinary optics restored afterwards");
}
