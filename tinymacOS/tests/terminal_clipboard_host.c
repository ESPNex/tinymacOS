/* Actual Terminal editing/selection/clipboard code, with syscall-only stubs. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../userspace/ui.h"
#undef sc
static long hostcall(long,u64,u64,u64,u64);
#define sc(n,a,b,c,d) hostcall(n,(u64)(a),(u64)(b),(u64)(c),(u64)(d))
#include "../userspace/terminal.c"

static int kills;
static long hostcall(long n,u64 a,u64 b,u64 c,u64 d){(void)c;(void)d;if(n==S_CLIPBOARD)return clipboard_access(a,(void*)b);if(n==S_KILL){kills++;return 0;}assert(!"unexpected terminal syscall");return -1;}
int main(void){
 aw=780;ah=400;static Clipboard q;clipboard_clear();memset(&q,0,sizeof q);assert(!clipboard_access(0,&q)&&!q.kind);
 q.kind=CLIP_FILE;strcpy(q.data,"relative");assert(clipboard_access(1,&q)<0);strcpy(q.data,"/Users/User/Documents/proof");q.cut=1;assert(!clipboard_access(1,&q));q.kind=CLIP_TEXT;assert(clipboard_access(1,&q)<0);q.cut=0;strcpy(q.data,"shared");assert(!clipboard_access(1,&q));
 strcpy(input,"typed command");cur=strlen(input);clipboard_action(ACT_COPY);memset(&q,0,sizeof q);assert(!clipboard_access(0,&q)&&q.kind==CLIP_TEXT&&!strcmp(q.data,"typed command"));clipboard_action(ACT_CUT);assert(!input[0]&&!cur);clipboard_action(ACT_PASTE);assert(!strcmp(input,"typed command"));
 q.kind=CLIP_TEXT;q.cut=0;strcpy(q.data,"echo one\necho two\r\033\t");assert(!clipboard_access(1,&q));input[cur=0]=0;clipboard_action(ACT_PASTE);assert(!strcmp(input,"echo one echo two   ")&&!job);
 memset(q.data,'X',INPUT);q.data[INPUT]=0;assert(!clipboard_access(1,&q));strcpy(input,"intact");cur=6;clipboard_action(ACT_PASTE);assert(!strcmp(input,"intact")&&cur==6);
 input[cur=0]=0;drop_path("/Users/User/Documents/a b\"$\\.txt");assert(!strcmp(input,"\"/Users/User/Documents/a b\\\"\\$\\\\.txt\" ")&&!job);
 nt=scroll=0;line("first output",12,0);line("second output",13,0);Event begin={.a=14,.b=12,.c=1,.d=1};select_event(&begin);Event end={.a=14+6*8,.b=31,.c=0};select_event(&end);assert(selection&&!selecting);clipboard_action(ACT_COPY);assert(!clipboard_access(0,&q)&&!strcmp(q.data,"first output\nsecond"));clipboard_action(ACT_CUT);assert(!strcmp(screen[0],"first output"));
 selection=0;job=99;Event ctrlc={.a='c',.b=1};key(&ctrlc);assert(kills==1);job=0;strcpy(input,"shortcuts");cur=strlen(input);Event copy={.a='C',.b=1,.c=1};key(&copy);input[cur=0]=0;Event paste={.a='V',.b=1,.c=1};key(&paste);assert(!strcmp(input,"shortcuts")&&kills==1);
 clipboard_clear();assert(!clipboard_access(0,&q)&&!q.kind&&!q.data[0]);
 puts("PASS actual Terminal: shared clipboard type/cut validation, full input copy/cut/paste, output mouse-range copy, output cut refusal, Ctrl+Shift+C/V vs preserved Ctrl+C, all-or-nothing overflow, multiline/control-character paste cannot execute, escaped path drop cannot execute, clipboard clearing");return 0;
}
