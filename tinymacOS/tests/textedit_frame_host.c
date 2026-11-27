/* Execute the real UI serializer and TextEdit paint against a captured syscall. */
#include <assert.h>
#include <stdio.h>
#include "../userspace/ui.h"
static long hostcall(long,u64,u64,u64,u64);
#undef sc
#define sc(n,a,b,c,d) hostcall(n,(u64)(a),(u64)(b),(u64)(c),(u64)(d))
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
    int widths[]={300,760,1278},heights[]={190,480,1024};
    for(int w=0;w<3;w++)for(int h=0;h<3;h++)for(int multibyte=0;multibyte<3;multibyte++){
        aw=widths[w];ah=heights[h];length=cursor=firstline=0;dialog=picker.active=0;
        const char *unit=multibyte==0?"a":multibyte==1?"à":"😀";
        int bytes=strlen(unit);
        for(int i=0;i<20000;i++){memcpy(doc+length,unit,bytes);length+=bytes;}
        doc[length]=0;paint();assert(published<NCMDS);
        int glyphs=0,runs=0;
        for(int i=0;i<published;i++)if(list[i].type==C_MONO){
            Draw *q=list+i;runs++;assert(q->w>0&&q->w<=99);
            assert(q->x>=28&&q->x+8*q->w<=aw-28);
            for(int k=0;k<q->w;k++)assert(mono_code((u8)q->text[k]-1)==(multibyte==0?'a':multibyte==1?0xe0:'?'));
            glyphs+=q->w;
        }
        assert(runs>0&&glyphs==visible_rows()*MAX(1,(aw-56)/8));
    }
    puts("PASS actual TextEdit full frames: 27 window/UTF-8 combinations through 1278x1024; every visible cell serialized below unchanged 144-command budget; wire repertoire equals production TFN2");
}
