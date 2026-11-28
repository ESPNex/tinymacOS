/* Real TextEdit functions, real temporary file bytes; only syscall transport is
 * adapted. Guest tests independently exercise the native guest filesystem. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../userspace/ui.h"
#include "../include/mono_wire.h"
static long hostcall(long,u64,u64,u64,u64);
#undef sc
#define sc(n,a,b,c,d) hostcall(n,(u64)(a),(u64)(b),(u64)(c),(u64)(d))
#include "body_font_fixture.h"
#include "../userspace/body_metrics.h"
#include "../userspace/textedit.c"
int aw=758,ah=480;SysInfo si;
static char target[PATHLEN];
static long hostcall(long n,u64 a,u64 b,u64 c,u64 d){(void)c;(void)d;assert(n==S_STAT);FILE*f=fopen((char*)a,"rb");if(!f)return -1;fseek(f,0,SEEK_END);*(Dirent*)b=(Dirent){.size=ftell(f)};fclose(f);return 0;}
int readfile(const char *p,char *b,int cap){FILE*f=fopen(p,"rb");if(!f)return -1;int n=fread(b,1,cap,f);assert(!ferror(f));fclose(f);return n;}
int writefile(const char *p,const char *b,int n){FILE*f=fopen(p,"wb");assert(f);int wrote=fwrite(b,1,n,f);assert(!fclose(f));return wrote;}
void filepanel_open(FilePanel*p,int s,const char*i){(void)p;(void)s;(void)i;assert(0);}
void ui_motion(int k){(void)k;}
void quit(void){assert(0);}
void addnum(char *p,u64 n,int cap){char s[32];number(s,n);append(p,s,cap);}
static void press(int k){key(&(Event){.type=EV_KEY,.a=k});viewport(1);}
int main(void)
{
    char temp[]="/var/tmp/tmc-textedit-roundtrip-XXXXXX";int fd=mkstemp(temp);assert(fd>=0);close(fd);strset(target,temp,sizeof target);
    static char expected[TEXTEDIT_LIMIT+1],found[TEXTEDIT_LIMIT+1];
    for(int newline=0;newline<2;newline++){
        int len=0;for(int i=0;i<24000;i++){const char*unit=newline&&i%53==0?"\n":"à€Z";int n=strlen(unit);memcpy(expected+len,unit,n);len+=n;}expected[len]=0;
        assert(writefile(target,expected,len)==len);assert(!load(target));assert(length==len&&!memcmp(doc,expected,len));
        int widths[]={758,460,1278,600};
        for(unsigned i=0;i<COUNT(widths);i++){
            aw=widths[i];viewport(0);scroll_document(-8);int top=firstline;viewport(0);assert(firstline==top);
            document_click(28+16,70);int original=cursor;press(0xe8);press(8);assert(cursor==original&&!memcmp(doc,expected,len));
            press(K_END);original=cursor;press('X');press(8);assert(cursor==original&&!memcmp(doc,expected,len));
            press(K_HOME);original=cursor;if(cursor<length){int scalar;int bytes;scalar=mono_decode(doc+cursor,length-cursor,&bytes);press(K_DEL);press(scalar);press(K_LEFT);assert(cursor==original&&!memcmp(doc,expected,len));}
            assert(saved());assert(readfile(target,found,sizeof found)==len&&!memcmp(found,expected,len));
        }
        printf("PASS real TextEdit disk round-trip: %d-byte UTF8 %s, click/scroll/4 resizes/visual Home-End/insert/backspace/delete+restore, exact saved bytes\n",len,newline?"with newlines":"without newlines");
    }
    assert(!unlink(temp));
}
