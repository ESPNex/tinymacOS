#define TMC_INPUT_TEST 1
#include "../drivers/input.c"
#include "text_input.h"
#include <string.h>
u64 ticks;
static void key(int usage,int mods,int cp,int ctrl)
{
    Event e;input_key_usage(usage,mods);assert(input_pop(&e));
    assert(e.type==EV_KEY&&e.a==cp&&e.b==ctrl);assert(!input_pop(&e));
}
int main(void)
{
    assert(boot.w==1280);
    key(31,0x02,'"',0);key(32,0x20,0xa3,0);key(35,2,'&',0);key(36,2,'/',0);
    key(45,0,'\'',0);key(46,0,0xec,0);key(47,0,0xe8,0);key(47,2,0xe9,0);
    key(48,0,'+',0);key(49,0,0xf9,0);key(50,0,0xf9,0);key(51,0,0xf2,0);key(52,0,0xe0,0);
    key(53,0,'\\',0);key(53,2,'|',0);key(100,0,'<',0);key(100,2,'>',0);
    key(47,64,'[',0);key(48,64,']',0);key(47,66,'{',0);key(48,66,'}',0);
    key(51,64,'@',0);key(52,64,'#',0);key(8,64,0x20ac,0);key(34,64,0x20ac,0);
    key(6,1,'c',1);key(6,16,'c',1);key(20,8,'q',1);
    input_key_usage(57,0);key(4,0,'A',0);key(4,2,'a',0);key(52,0,0xc0,0);input_key_usage(57,0);
    char buf[64]="";int at=0;
    const int cps[]={0xe0,0xe8,0xe9,0xec,0xf2,0xf9,0x20ac};
    for(unsigned i=0;i<COUNT(cps);i++)assert(text_insert(buf,sizeof buf,&at,cps[i]));
    assert(!strcmp(buf,"àèéìòù€"));assert(text_field_length(buf,sizeof buf)==15&&text_columns(buf,15)==7);
    at=text_prev(buf,at);assert(at==12);text_delete(buf,at);assert(!strcmp(buf,"àèéìòù"));
    text_backspace(buf,&at);assert(at==10&&!strcmp(buf,"àèéìò"));
    at=2;assert(text_insert(buf,sizeof buf,&at,0x20ac));assert(!strcmp(buf,"à€èéìò"));
    text_backspace(buf,&at);assert(at==2&&!strcmp(buf,"àèéìò"));
    char tiny[3]="";at=0;assert(text_insert(tiny,3,&at,0xe0));assert(!text_insert(tiny,3,&at,'a'));assert(!text_insert(tiny,3,&at,0x20ac));
    assert(text_field_length("\xc0\xaf",3)==-1);assert(text_field_length("\xed\xa0\x80",4)==-1);
    assert(text_field_length("\xf4\x90\x80\x80",5)==-1);assert(text_field_length("\xc3",2)==-1);
    assert(text_field_length("a\n",3)==-1);assert(!text_printable(128));assert(!text_printable(K_DEL));
    puts("PASS sole Italian physical map, Shift/AltGr/accent/euro/ISO102, independent shortcuts, UTF-8 insertion/delete/caret/field validation");
}
