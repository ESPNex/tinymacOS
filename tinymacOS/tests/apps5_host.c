/* Execute the three production apps and the real Draw serializer. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../userspace/ui.h"
#include "../include/mono_wire.h"
static long hostcall(long,u64,u64,u64,u64);
#undef sc
#define sc(n,a,b,c,d) hostcall(n,(u64)(a),(u64)(b),(u64)(c),(u64)(d))
#define op ui_draw_op
#define readfile syscall_readfile
#define writefile syscall_writefile
#include "../userspace/ui.c"
#undef readfile
#undef writefile
#undef op
#if APP_CASE==1
#include "../userspace/textedit.c"
static char fixture[32],written[TEXTEDIT_LIMIT+1];static int fixture_len,written_len;
static long hostcall(long n,u64 a,u64 b,u64 c,u64 d){(void)a;(void)c;(void)d;if(n==S_STAT){*(Dirent*)b=(Dirent){.size=fixture_len};return 0;}if(n==S_CLIPBOARD)return 0;assert(n==S_WDRAW);return 0;}
int readfile(const char*p,char*b,int cap){(void)p;assert(cap>=fixture_len);memcpy(b,fixture,fixture_len);return fixture_len;}
int writefile(const char*p,const char*b,int n){(void)p;memcpy(written,b,n);written[n]=0;written_len=n;return n;}
void filepanel_open(FilePanel*p,int s,const char*i){(void)p;(void)s;(void)i;assert(0);}
void filepanel_draw(FilePanel*p){(void)p;assert(0);}
static void setdoc(const char*s){strset(doc,s,sizeof doc);length=strlen(doc);cursor=firstline=upstream=scroll_drag=0;dirty=0;}
static void press(int k,int command){key(&(Event){.type=EV_KEY,.a=k,.b=command});viewport(key_follows);}
static void check_paint(void){
 paint();assert(count<NCMDS);int row=firstline,offset=starts[row],glyphs=0;
 for(int i=0;i<count;i++)if(list[i].type==C_MONO){Draw*q=list+i;int target=firstline+(q->y-70)/20;
  if(target!=row){assert(offset==endline(row));row=target;offset=starts[row];}
  assert(q->x>=28&&q->x+q->w*8<=aw-28);
  for(int j=0;j<q->w;j++){int z;unsigned c=mono_decode(doc+offset,length-offset,&z);assert((u8)q->text[j]==mono_reference(c));offset+=z;glyphs++;}
 }
 assert(glyphs>0&&offset==endline(row));
}
int main(void){
 aw=104;ah=215;setdoc("uno due tre");layout();assert(wrap_columns()==6&&nlines==3&&starts[1]==4&&starts[2]==8&&!strcmp(doc,"uno due tre"));
 aw=88;setdoc("abcdef");layout();assert(starts[1]==4);press(K_END,0);assert(cursor==4&&upstream&&cursorline()==0);press('X',0);assert(!strcmp(doc,"abcdXef"));press(8,0);assert(!strcmp(doc,"abcdef"));
 document_click(1000,70);assert(cursor==4&&upstream&&cursorline()==0);press(K_DOWN,0);assert(cursor==6&&cursorline()==1);press(K_UP,0);assert(cursor==2&&cursorline()==0);
 setdoc("àèéòù€");layout();press(K_END,0);assert(cursor==8&&upstream&&cursorline()==0);press('Z',0);assert(!strcmp(doc,"àèéòZù€"));press(8,0);assert(!strcmp(doc,"àèéòù€"));
 static char original[TEXTEDIT_LIMIT+1];int pos=0;for(int i=0;i<1800;i++){const char*s=i%7?"àè WORD fgj ":"LOOOOOOOOOOOOOOOOOOOONG€ ";int z=strlen(s);memcpy(original+pos,s,z);pos+=z;}original[pos]=0;
 int widths[]={88,216,758,1278},heights[]={130,190,480,1024};
 for(unsigned a=0;a<COUNT(widths);a++)for(unsigned b=0;b<COUNT(heights);b++){
  aw=widths[a];ah=heights[b];setdoc(original);layout();assert(length==pos&&!memcmp(doc,original,pos));
  for(int j=1;j<nlines;j++){assert(starts[j]>starts[j-1]&&((u8)doc[starts[j]]&0xc0)!=0x80);assert(text_columns(doc+starts[j-1],endline(j-1)-starts[j-1])<=wrap_columns());}
  check_paint();scroll_document(-4);int top=firstline,cur=cursor;paint();assert(firstline==top&&cursor==cur&&!dirty);
  press('c',1);assert(firstline==top&&!key_follows); /* Copy is not caret navigation. */
  int tx=aw-15,topY=thumb_y();scrollbar_event(&(Event){.type=EV_SELECT,.a=tx,.b=topY+1,.c=1,.d=1});assert(scroll_drag);
  scrollbar_event(&(Event){.type=EV_SELECT,.a=tx,.b=2000000000,.c=0});assert(!scroll_drag&&firstline==scroll_limit()&&cursor==cur&&!dirty);
  check_paint();scroll_document(2147483647);assert(!firstline);scroll_document(-2147483647-1);assert(firstline==scroll_limit());
  press(K_UP,1);assert(!cursor&&!firstline);press(K_DOWN,1);assert(cursor==length);assert(saved()&&written_len==pos&&!memcmp(written,original,pos));
 }
 aw=758;ah=480;setdoc("intact");fixture_len=3;fixture[0]='a';fixture[1]=0;fixture[2]='b';assert(load("binary")<0&&!strcmp(doc,"intact"));
 puts("PASS actual TextEdit: whole-word/scalar wrap, every serialized glyph retained, correct End/click affinity, UTF-8 edits, 16 resize/scroll matrices, wheel/track/thumb, copy preserves viewport, full saved bytes, embedded-NUL refusal");
}
#elif APP_CASE==2
#include "../userspace/monitor.c"
static SysInfo f;static ProcInfo ps[NPROC];static int got=2,info_error,ps_error;
static long hostcall(long n,u64 a,u64 b,u64 c,u64 d){(void)b;(void)c;(void)d;if(n==S_INFO){if(info_error)return -1;*(SysInfo*)a=f;return 0;}if(n==S_PS){if(ps_error)return -1;memcpy((void*)a,ps,got*sizeof*ps);return got;}assert(n==S_WDRAW);return 0;}
static int contains(const char*s){for(int i=0;i<count;i++)if(list[i].type==C_TEXT&&!strcmp(list[i].text,s))return 1;return 0;}
static void step(int busycores){f.ticks+=100;for(int c=0;c<8;c++)if(f.cpu_online_mask&(1u<<c)){f.core_ticks[c]+=100;if(c<busycores)f.core_busy[c]+=100;}for(int j=0;j<got;j++){ps[j].sample_ticks+=100*f.ncpu;if(j==0)ps[j].cpu_ticks+=100;}sample();}
int main(void){
 aw=818;ah=620;f=(SysInfo){.total=128*1024*1024,.free=32*1024*1024,.ram_bytes=256*1024*1024,.reserved_bytes=128*1024*1024,.user_bytes=12*1024*1024,.ncpu=4,.cpu_online_mask=15};
 for(int c=0;c<4;c++){f.core_ticks[c]=100;f.core_busy[c]=10;}ps[0]=(ProcInfo){.pid=7,.sample_ticks=400,.cpu_ticks=10,.resident_bytes=8*1024*1024};ps[1]=(ProcInfo){.pid=9,.sample_ticks=400};
 sample();assert(stats_valid&&memory_valid&&process_valid&&!cpu_valid&&!rowvalid[0]);paint();assert(contains("—")&&!contains("0%"));
 step(1);assert(cpu_valid&&cpu==25&&rowvalid[0]&&rowcpu[0]==25&&!rowcpu[1]);paint();assert(contains("25%")&&contains("0%"));
 char expected[32];display_size(expected,sizeof expected,f.total-f.free);assert(contains(expected));display_size(expected,sizeof expected,ps[0].resident_bytes);assert(contains(expected));
 step(4);assert(cpu==100);resume();assert(!cpu_valid&&!rowvalid[0]);step(1);assert(cpu_valid);
 ps_error=1;step(1);assert(!process_valid&&!n&&cpu_valid);paint();assert(contains("Processi · dati non disponibili"));ps_error=0;step(1);assert(process_valid&&!rowvalid[0]);step(1);assert(rowvalid[0]);
 info_error=1;sample();assert(!stats_valid&&!cpu_valid&&!memory_valid&&!n);paint();assert(contains("Telemetria non disponibile")&&contains("Dati RAM non disponibili"));info_error=0;sample();assert(!cpu_valid);step(1);assert(cpu_valid);
 f.free=f.total+4096;sample();assert(!memory_valid);paint();assert(contains("Dati RAM non disponibili"));f.free=32*1024*1024;
 f.cpu_online_mask=1;f.ncpu=1;sample();assert(!cpu_valid);step(1);assert(cpu==100&&rowcpu[0]==100);f.core_ticks[0]=1;f.core_busy[0]=0;sample();assert(!cpu_valid&&!rowvalid[0]);
 puts("PASS actual Monitor: real delta CPU normalized 1/4 cores, exact allocator/resident rendering, baseline/reset/pause/process failure marked unavailable (not invented zero), memory invariants, recovery after errors");
}
#elif APP_CASE==3
#include "../userspace/calculator.c"
static long hostcall(long n,u64 a,u64 b,u64 c,u64 d){(void)a;(void)b;(void)c;(void)d;assert(n==S_WDRAW);return 0;}
int main(void){
 ah=377;int widths[]={300,594,595,605,1000};
 for(unsigned k=0;k<COUNT(widths);k++){
  aw=widths[k];paint();int sheets=0;
  for(int j=0;j<count;j++)if(list[j].type==C_GLASS){Draw*q=list+j;if(++sheets==2){assert(q->x==padw()+8&&q->x+q->w==aw-6&&q->y==4&&q->h==ah-12);}}
  assert(sheets==(aw>=595?2:1)&&count<NCMDS);
 }
 clear();key('7');key('+');key('5');key('=');assert(!strcmp(entry,"12")&&!strcmp(hist[0],"7 + 5 = 12"));key('=');assert(!strcmp(entry,"17"));
 clear();key('8');key('/');key('0');key('=');assert(fail&&!strcmp(entry,"Errore"));
 puts("PASS actual Calculator: history glass +8px gap and unchanged right inset across thresholds/sizes; no extra sheet in compact view; arithmetic/repeat/error/history unchanged");
}
#endif
