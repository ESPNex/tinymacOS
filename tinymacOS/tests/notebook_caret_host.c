#include <assert.h>
#include <stdio.h>
#include "../userspace/ui.h"
#ifndef STICKY
#define STICKY 0
#endif
static long hostcall(long,u64,u64,u64,u64);
#undef sc
#define sc(n,a,b,c,d) hostcall(n,(u64)(a),(u64)(b),(u64)(c),(u64)(d))
#include "body_font_fixture.h"
#include "../userspace/ui.c"
#include "../userspace/notebook.h"
static long hostcall(long n,u64 a,u64 b,u64 c,u64 d){(void)a;(void)b;(void)c;(void)d;assert(n==S_WDRAW);return 0;}
static int caret_count(void){int n=0;for(int i=0;i<count;i++)n+=list[i].type==C_CARET;return n;}
int main(void)
{
    aw=688;ah=387;id=1;focus=confirm=0;body[cur=0]=0;
    paint();assert(caret_count()==1&&count<NCMDS);
    strset(body,"à€ nota",sizeof body);cur=strlen(body);paint();assert(caret_count()==1);
    focus=1;search[0]=0;paint();assert(caret_count()==1);
    strset(search,"àbc",sizeof search);paint();assert(caret_count()==1);
    confirm=1;paint();assert(!caret_count());confirm=focus=0;
    assert(BodyWidth("WWWW",4)>BodyWidth("iiii",4));
    strset(body,"Wilà € WWW iii",sizeof body);cur=0;editor(1,0,0);
    int at=text_next(body,0),px=BodyWidth(body,at);
    assert(editor(0,rail()+20+px,75)==at);
    int widths[]={460,688,1278};
    for(unsigned j=0;j<COUNT(widths);j++){aw=widths[j];cur=strlen(body);paint();assert(caret_count()==1&&count<NCMDS);int glyphs=0;
      for(int k=0;k<count;k++){Draw*q=list+k;if(q->type==C_BODY){assert(q->style==F_BODY);glyphs+=q->w;}assert(q->type!=C_MONO||q->y<72);}
      assert(glyphs==text_columns(body,strlen(body)));}

    printf("PASS actual %s paint: one caret in empty/nonempty editor or search, none behind confirmation; within command budget\n",STICKY?"Memo":"Note");
}
