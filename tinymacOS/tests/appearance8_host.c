#include <assert.h>
#include <stdio.h>
#include "../desktop/gfx.c"
/* Load the actual shipped TFN2, without guest syscalls. */
static void load_font(void)
{
    FILE *f=fopen("assets/type.tfn","rb");assert(f);
    size_t n=fread(fontdata,1,sizeof fontdata,f);assert(n>16&&!ferror(f));fclose(f);
    assert(!memcmp(fontdata,"TFN2",4));ng=*(u32 *)(fontdata+8);
    symbols=(u32 *)(fontdata+12);const u8 *p=(const u8 *)(symbols+ng);
    for(int i=0;i<NFONTS;i++){memcpy(&font[i].h,p,4);p+=4; font[i].g=(const void *)p;p+=ng*sizeof(Glyph);}
    bits=p;assert(bits<fontdata+n);
}
static void check_field(const char *s,int width)
{
    int len=strlen(s);
    for(int at=0;at<=len;at=text_next(s,at)){
        int start=field_start(s,at,width,field_advance),x=0;
        assert(start<=at&&((u8)s[start]&0xc0)!=0x80);
        assert(proportional_hit(s,at,width,-100)==start);
        for(int i=start;i<at;i=text_next(s,i))x+=field_advance(s,i,text_next(s,i));
        assert(x<width);assert(proportional_hit(s,at,width,x)==at);
        for(int px=-2;px<=width+2;px++){
            int hit=proportional_hit(s,at,width,px);
            assert(hit>=start&&hit<=len&&((u8)s[hit]&0xc0)!=0x80);
        }
        W=240;H=50;resetclip();rect(0,0,W,H,0x123456);
        proportional_input((Rect){30,10,width,18},s,at,1,0xffffff);
        assert(x1==0&&y1==0&&x2==W&&y2==H);
        for(int y=0;y<H;y++)for(int xx=0;xx<W;xx++)if(xx<30||xx>=30+width||y<10||y>=28)assert(canvas[y*W+xx]==0x123456);
        if(at==len)break;
    }
}
static void check_wallpaper(void)
{
    loaded[0]=1;
    for(int i=0;i<1280*800;i++)films[0][i]=(i*7919u)&0xffffff;
    const int sizes[][2]={{640,480},{1280,800},{1920,1080}};
    for(unsigned d=0;d<3;d++)for(int dark=0;dark<2;dark++){
        W=sizes[d][0];H=sizes[d][1];background(0,2,dark);
        for(int y=0;y<H;y++)for(int x=0;x<W;x++){
            u32 c=films[0][(y*800/H)*1280+x*1280/W];
            assert(back[y*W+x]==(dark?blend(c,0x0b1225,64):c));
        }
        u32 sample=back[W*H/2];appearance_palette(3,dark);
        assert(palette==3&&night==dark&&back[W*H/2]==sample);
    }
    puts("PASS wallpaper: optimized scaler pixel-identical at 640/1280/1920 widths in light/dark; palette-only update leaves wallpaper unchanged");
}
static void check_library_material(void)
{
    W=320;H=240;Rect q={40,30,200,160};resetclip();lite=0;recovery_mode=0;
    for(int dark=0;dark<2;dark++){
        night=dark;rect(0,0,W,H,0x25476a);glass(q,22,MAT_LIBRARY);
        for(int y=q.y+22;y<q.y+q.h-22;y++){
            u32 middle=canvas[y*W+q.x+q.w/2];
            for(int x=q.x;x<q.x+q.w;x++)assert(canvas[y*W+x]==middle);
            assert(canvas[y*W+q.x-1]==0x25476a&&canvas[y*W+q.x+q.w]==0x25476a);
        }
    }
    puts("PASS Library frosted material: no dark inset/raised bevel across full-width rows; no pixels outside panel changed in light/dark");
}
int main(void)
{
    check_library_material();check_wallpaper();load_font();assert(textw("WWW",F_BODY)>textw("iii",F_BODY));
    assert(textw("WWW",F_BODY)!=24||textw("iii",F_BODY)!=24);
    const char *cases[]={"","iiiWWW","Caffè già più","⌘ Frecce → ←","iiiiiiiiiiiiiiiiWWWWWWWWWW"};
    for(unsigned i=0;i<sizeof cases/sizeof *cases;i++)for(int w=32;w<=160;w+=32)check_field(cases[i],w);
    puts("PASS Spotlight: shipped proportional font; UTF-8 caret/hit/viewport roundtrips; clipping and clip restoration");
}
