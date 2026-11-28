/* Execute the real UI serializer and TextEdit paint against a captured syscall. */
#include <assert.h>
#include <stdio.h>
#include "../userspace/ui.h"
static long hostcall(long,u64,u64,u64,u64);
#undef sc
#define sc(n,a,b,c,d) hostcall(n,(u64)(a),(u64)(b),(u64)(c),(u64)(d))
#include "body_font_fixture.h"
#include "../userspace/ui.c"
#include "../userspace/textedit.c"
static int published;
static long hostcall(long n,u64 a,u64 b,u64 c,u64 d){(void)a;(void)c;(void)d;assert(n==S_WDRAW);published=b;return 0;}
void filepanel_draw(FilePanel *p){(void)p;assert(0);}
int main(void)
{
    FILE *f=fopen("assets/type.tfn","rb");assert(f);unsigned header[3],codes[207];
    assert(fread(header,4,3,f)==3&&header[2]==207);
    assert(fread(codes,4,207,f)==207);fclose(f);
    for(unsigned i=0;i<207;i++){assert(codes[i]==mono_code(i));assert(mono_reference(codes[i])==i+1);}
    for(unsigned i=0;i<207;i++){char scalar[5];int z=text_encode(mono_code(i),scalar);scalar[z]=0;assert(BodyWidth(scalar,z)>0);}
    int reads=fixture_measures;for(int i=0;i<1000;i++)assert(BodyWidth("WiWi à€\t",12)>0);assert(fixture_measures==reads);
    int widths[]={300,760,1278},heights[]={190,480,1024};
    for(int w=0;w<3;w++)for(int h=0;h<3;h++)for(int multibyte=0;multibyte<5;multibyte++){
        aw=widths[w];ah=heights[h];length=cursor=firstline=0;dialog=picker.active=0;
        const char *unit=multibyte==0?"a":multibyte==1?"à":multibyte==2?"😀":multibyte==3?"i":"Ì";
        int bytes=strlen(unit);
        for(int i=0;i<20000;i++){memcpy(doc+length,unit,bytes);length+=bytes;}
        doc[length]=0;paint();assert(published<NCMDS);
        int glyphs=0,runs=0;
        for(int i=0;i<published;i++)if(list[i].type==C_BODY){
            Draw *q=list+i;runs++;assert(q->w>0&&q->w<=99);
            assert(q->x>=28&&q->x+BodyWidth(unit,bytes)*q->w<=aw-28);
            for(int k=0;k<q->w;k++)assert(mono_code((u8)q->text[k]-1)==(multibyte==0?'a':multibyte==1?0xe0:multibyte==2?'?':multibyte==3?'i':0xcc));
            glyphs+=q->w;
        }
        assert(runs>0&&glyphs==visible_rows()*MAX(1,(aw-56)/BodyWidth(unit,bytes)));
    }
    puts("PASS actual TextEdit full frames: 45 window/UTF-8 combinations through 1278x1024; every visible cell serialized below bounded 256-command budget (including narrow i/Ì glyphs); wire repertoire equals production TFN2");
}
