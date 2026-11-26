/* Execute the actual shared file chooser with mocked filesystem syscalls. */
#include "../userspace/filepanel.h"
#include <stdio.h>
#include <assert.h>
static struct {const char *path;int dir,ro,size;} nodes[]={
    {"/",1,1,0},{HOME,1,0,0},{DOCS,1,0,0},{DOCS "/Folder",1,0,0},
    {DOCS "/existing.txt",0,0,4},{DOCS "/locked.txt",0,1,4},
    {"/System",1,1,0},{"/System/config",0,1,4},{DOCS "/huge.txt",0,0,TEXTEDIT_LIMIT+1}};
static int writes,creates,spawned,many;
static char created[PATHLEN];
static long mockcall(long n,u64 a,u64 b,u64 c,u64 d)
{
    if(many&&n==S_STAT&&!memcmp((char*)a,DOCS "/item-",sizeof(DOCS "/item-")-1)){Dirent*out=(void*)b;*out=(Dirent){.size=4};strset(out->name,leaf((char*)a),48);return 0;}
    if(many&&n==S_LIST&&eq((char*)a,DOCS)){int count=0;for(int i=d;i<1100&&count<(int)c;i++){Dirent*out=(Dirent*)b+count++;*out=(Dirent){.id=i+1,.size=4};sprintf(out->name,"item-%04d",i);}return count;}
    if(n==S_STAT){for(unsigned i=0;i<COUNT(nodes);i++)if(eq((char *)a,nodes[i].path)){Dirent *out=(void *)b;memset(out,0,sizeof *out);out->dir=nodes[i].dir;out->ro=nodes[i].ro;out->size=nodes[i].size;strset(out->name,leaf(nodes[i].path),48);return 0;}return -1;}
    if(n==S_LIST){int total=0;for(unsigned i=1;i<COUNT(nodes);i++){char parent[PATHLEN];strset(parent,nodes[i].path,PATHLEN);parentpath(parent);if(eq(parent,(char *)a)&&total<(int)c){if(d){d--;continue;}Dirent *out=(Dirent *)b+total++;memset(out,0,sizeof *out);out->dir=nodes[i].dir;out->ro=nodes[i].ro;out->size=nodes[i].size;strset(out->name,leaf(nodes[i].path),48);}}return total;}
    if(n==S_CREATE){assert(b==1);creates++;strset(created,(char *)a,PATHLEN);return 0;}
    if(n==S_SPAWN){assert(a==FINDER);spawned++;return 8;}
    if(n==S_WRITE||n==S_OPEN){writes++;assert(!"Chooser must not create/truncate/write document content");}
    return 0;
}
#undef sc
#define sc(n,a,b,c,d) mockcall(n,(u64)(a),(u64)(b),(u64)(c),(u64)(d))
#include "../userspace/ui.c"
#include "../userspace/filebrowser.c"
#include "../userspace/filepanel.c"
int eq(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
void strset(char*d,const char*s,int n){int i=0;if(n<1)return;for(;i<n-1&&s[i];i++)d[i]=s[i];d[i]=0;}
void append(char*d,const char*s,int n){int at=strlen(d);if(at<n)strset(d+at,s,n-at);}
void number(char*d,u64 n){sprintf(d,"%llu",(unsigned long long)n);}
int val(const char*s){int n=0;while(*s>='0'&&*s<='9')n=n*10+*s++-'0';return n;}
static int key(FilePanel*p,int k){Event e={.type=EV_KEY,.a=k};return filepanel_event(p,&e);}
int main(void)
{
    static FilePanel p;aw=758;ah=480;
    filepanel_open(&p,1,DOCS "/new.txt");assert(p.active&&eq(p.cwd,DOCS));assert(key(&p,'\n')==1&&eq(p.result,DOCS "/new.txt")&&!writes);
    filepanel_open(&p,1,DOCS "/existing.txt");assert(key(&p,'\n')==0&&p.overwrite&&p.active);assert(key(&p,27)==0&&!p.overwrite&&p.active);assert(key(&p,'\n')==0&&p.overwrite);assert(key(&p,'\n')==1&&!p.active&&!writes);
    filepanel_open(&p,1,DOCS "/locked.txt");assert(key(&p,'\n')==0&&!p.overwrite&&p.message[0]);
    filepanel_open(&p,1,"/System/new.txt");assert(key(&p,'\n')==0&&!p.overwrite&&p.message[0]);
    filepanel_open(&p,0,DOCS "/existing.txt");p.focus=1;strset(p.name,"existing.txt",PATHLEN);assert(key(&p,'\n')==1&&eq(p.result,DOCS "/existing.txt"));
    filepanel_open(&p,0,DOCS "/existing.txt");p.focus=1;strset(p.name,"missing.txt",PATHLEN);assert(key(&p,'\n')==0&&p.active);strset(p.name,"huge.txt",PATHLEN);assert(key(&p,'\n')==0&&p.active);
    assert(navigate(&p,"Folder")&&eq(p.cwd,DOCS "/Folder"));assert(navigate(&p,"..")&&eq(p.cwd,DOCS));assert(!navigate(&p,"/missing"));
    filepanel_open(&p,1,DOCS "/new.txt");p.replace=1;key(&p,0xe8);key(&p,'.');key(&p,'t');key(&p,'x');key(&p,'t');assert(eq(p.name,"è.txt"));key(&p,K_HOME);key(&p,K_DEL);assert(eq(p.name,".txt"));assert(key(&p,27)==-1&&!p.active&&!writes);
    filepanel_open(&p,1,DOCS "/new.txt");p.newfolder=1;p.focus=3;strset(p.folder,"../bad",48);assert(key(&p,'\n')==0&&creates==0);strset(p.folder,"Nuova",48);key(&p,'\n');assert(creates==1&&eq(created,DOCS "/Nuova"));
    assert(browser_finder(DOCS)>=0&&spawned==1);assert(browser_finder(DOCS "/existing.txt")<0);
    filepanel_open(&p,1,DOCS "/new.txt");begin();filepanel_draw(&p);assert(count<NCMDS);assert(list[0].type==C_GLASS&&list[0].style==GLASS_DIALOG);
    many=1;filepanel_open(&p,0,DOCS "/item-0000");assert(p.count==64&&p.more&&p.pagebase==0);
    p.selected=63;key(&p,K_DOWN);assert(p.pagebase==64&&p.selected==0&&eq(p.name,"item-0064"));
    p.offset=p.count-p.rows;filepanel_event(&p,&(Event){.type=EV_SCROLL,.a=-1});assert(p.pagebase==128);
    filepanel_event(&p,&(Event){.type=EV_CLICK,.a=p.x+p.w-65,.b=p.y+15});assert(p.pagebase==64);
    while(panel_page(&p,-1));int seen=0;
    do{for(int i=0;i<p.count;i++){char expected[48];sprintf(expected,"item-%04d",seen++);assert(eq(expected,p.entries[i].name));}}while(panel_page(&p,1));
    assert(seen==1100&&p.pagebase==1088&&!p.more&&p.count==12);
    p.selected=11;selected_name(&p);assert(key(&p,'\n')==1&&eq(p.result,DOCS "/item-1099")&&!writes);
    puts("PASS real file panel: open/save validation, overwrite/cancel, readonly/missing/oversized, UTF8 caret, navigation, new folder, shared Finder, 1100 files via bounded pages, no document writes before acceptance");
}
