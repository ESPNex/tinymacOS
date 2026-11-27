#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../userspace/ui.h"
static long hostcall(long,u64,u64,u64,u64);
#undef sc
#define sc(n,a,b,c,d) hostcall(n,(u64)(a),(u64)(b),(u64)(c),(u64)(d))
#define count ui_count
#include "../userspace/ui.c"
#undef count
#include "../userspace/terminal.c"
static int unlocks,answer,modern;
static char received[65];
static long hostcall(long n,u64 a,u64 b,u64 c,u64 d)
{
    (void)d;
    if(n==S_WDRAW)return 0;
    if(n==S_FILEVAULT){
        assert(a==0||a==4);VaultRequest*q=(VaultRequest*)b;VaultInfo*v=(VaultInfo*)c;
        assert(q->disk==(si.recovery?0:-1));*v=(VaultInfo){.format=modern?3:0};
        if(a==4){assert(modern);strset(received,q->password,sizeof received);unlocks++;return answer;}
        return modern?0:-1;
    }
    if(n==S_SNAP){assert(a==6&&b==4);strset(received,(char*)c,sizeof received);unlocks++;return answer;}
    assert(!"secret prompt attempted an unexpected syscall");return -1;
}
static void press(int k){key(&(Event){.a=k});}
static Draw marker(void){for(int i=0;i<ui_count;i++)if(list[i].type==C_CARET)return list[i];assert(0);return (Draw){0};}
static int zeroed(void){for(unsigned i=0;i<sizeof secret_input;i++)if(secret_input[i])return 0;return 1;}
int main(void)
{
    aw=728;ah=398;si.user[0]=0;strset(input,"apfs_unlockfv",sizeof input);cur=strlen(input);enter();assert(secret_mode&&!job&&nh==1);
    paint();Draw empty=marker();int before_nt=nt;
    /* Every UTF-8 insertion/deletion has identical wire bytes and marker geometry. */
    Draw original[NCMDS];int commands=ui_count;memcpy(original,list,commands*sizeof(Draw));
    const int keys[]={'a',0xe8,0x20ac,'Z',8,K_LEFT,K_RIGHT,K_HOME,K_END,K_DEL,'Q'};
    for(unsigned j=0;j<COUNT(keys);j++){
        press(keys[j]);paint();Draw now=marker();assert(now.x==empty.x&&now.y==empty.y);
        assert(ui_count==commands&&!memcmp(original,list,commands*sizeof(Draw))&&nt==before_nt);
    }
    assert(eq(secret_input,"aè€Q"));
    clipboard_action(ACT_COPY);clipboard_action(ACT_CUT);clipboard_action(ACT_PASTE);drop_path("/password-path");
    assert(eq(secret_input,"aè€Q")&&nh==1);
    press('\n');assert(!secret_mode&&zeroed()&&unlocks==1&&eq(received,"aè€Q"));
    assert(eq(history[0],"apfs_unlockfv")&&nh==1);
    for(int i=0;i<nt;i++)assert(!strstr(screen[i],"aè€Q"));
    strset(input,"diskutil apfs unlockVolume",sizeof input);cur=strlen(input);enter();assert(secret_mode);press('X');key(&(Event){.a='c',.b=1});assert(!secret_mode&&zeroed()&&unlocks==1&&last==130);
    strset(input,"apfs_unlockfv",sizeof input);cur=strlen(input);enter();press('Y');press(27);assert(!secret_mode&&zeroed()&&unlocks==1);
    strset(input,"apfs_unlockfv",sizeof input);cur=strlen(input);enter();for(int i=0;i<200;i++)press(0xe8);assert(strlen(secret_input)==64);answer=-1;press('\n');assert(last==1&&zeroed()&&unlocks==2);
    modern=1;answer=0;si.recovery=1;
    strset(input,"apfs_unlockfv",sizeof input);cur=strlen(input);enter();press(0xe8);press('X');press('8');press('9');press('0');press('1');press('2');press('3');press('\n');assert(last==0&&unlocks==3&&zeroed()&&eq(received,"èX890123"));
    /* Regression: the visible text of shared inputmodals must sit INSIDE the
     * field drawn by modal(), including at narrow window sizes. */
    int widths[]={300,760,1278};
    for(unsigned j=0;j<COUNT(widths);j++){
        aw=widths[j];ah=480;begin();inputmodal("Titolo","abc","Conferma",0);Draw caret=marker();
        int w=MIN(aw-46,430),x=(aw-w)/2,y=(ah-143)/2;
        assert(caret.x==x+25+24&&caret.y==y+52);
        assert(caret.x>=x+16&&caret.x<x+w-16&&caret.y>=y+43&&caret.y+caret.h<=y+78);
    }
    puts("PASS real Terminal no-echo: command aliases, invariant full Draw wire/position for UTF8 typing/deletion, no history/output/clipboard/drop leakage, bounded overflow, exact legacy/FS2 backend credential, success/failure/Ctrl-C/Escape wiping; shared modal caret inside actual field");
}
