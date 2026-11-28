#include <assert.h>
#include <stdio.h>
#include "../kernel/k.h"
static void host_lock(Spin*s,u32*f){assert(!s->v);s->v=1;*f=0;}
static void host_unlock(Spin*s,u32 f){(void)f;assert(s->v);s->v=0;}
#define spin_lock host_lock
#define spin_unlock host_unlock
#include "../kernel/sched.c"
static Process p;
int main(void)
{
 Event resize={.type=EV_RESIZE,.a=500,.b=300},key={.type=EV_KEY,.a='x'},click={.type=EV_CLICK,.a=45};
 assert(!post_event(0,&resize));
 for(int j=0;j<2000;j++){resize.a=500+j;assert(post_event(&p,&resize));}assert(p.head==1&&p.events[0].a==2499);
 assert(post_event(&p,&key)&&post_event(&p,&resize)&&post_event(&p,&click));assert(p.head==4&&p.events[1].type==EV_KEY&&p.events[2].type==EV_RESIZE&&p.events[3].type==EV_CLICK);
 for(int j=0;j<27;j++)assert(post_event(&p,&key));assert(!post_event(&p,&resize)&&p.head==31);
 p.tail=1;assert(post_event(&p,&resize)&&!p.head);resize.a=900;assert(post_event(&p,&resize)&&!p.head&&p.events[31].a==900);
 assert(!sched_lock.v);
 puts("PASS actual kernel resize IPC: 2000 adjacent updates coalesce; key/click ordering retained; full ring reports failure for compositor retry; queue wrap and final geometry correct");
}
