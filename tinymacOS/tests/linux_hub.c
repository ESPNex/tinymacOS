/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Original Linux hub reset/debounce, scripted EP0 + deterministic elapsed time.
 * No replacement reset state machine and no physical-hardware claim. */
#include <assert.h>
#include <stdio.h>
#include <pthread.h>
#include <sched.h>
#include "../compat/linux/usb_hub.c"
struct Fixture {unsigned mode,reads,writes,clears,resets,polls,ms;int start_writer;};
static struct Fixture *active;
static pthread_t writer;
static unsigned writer_entered,writer_done;
static void *cf_writer(void *p)
{
 (void)p;__atomic_store_n(&writer_entered,1,__ATOMIC_RELEASE);tmc_linux_usb_cf_begin();
 __atomic_store_n(&writer_done,1,__ATOMIC_RELEASE);tmc_linux_usb_cf_end();return 0;
}
void usb_delay_ms(unsigned n)
{
 assert(active);active->ms+=n;
 if(active->start_writer&&active->resets){
  active->start_writer=0;assert(!pthread_create(&writer,0,cf_writer,0));
  while(!__atomic_load_n(&writer_entered,__ATOMIC_ACQUIRE))sched_yield();
  assert(!__atomic_load_n(&writer_done,__ATOMIC_ACQUIRE));
 }
}
static int control(void *arg,unsigned rt,unsigned request,unsigned value,unsigned port,u8 *data,unsigned n)
{
 struct Fixture *f=arg;assert(f==active&&port==3);
 if(rt==0xa3){
  assert(!request&&!value&&data&&n==4);f->reads++;unsigned st=1,ch=0;
  if(f->mode==1)st=0;
  if(f->mode==2)st=f->reads%2;
  if(f->mode==3)return 3;
  if(f->mode==4)return -5;
  if(f->mode==5)st|=8;
  if(f->resets){
   f->polls++;st=1|2|1024;
   if(f->mode==6)st=1|16;
   if(f->mode==7)st=0;
   if(f->mode==8&&f->resets==1)ch=1;
   if(f->polls<3&&f->mode!=7)st=1|16;
  }
  data[0]=st;data[1]=st>>8;data[2]=ch;data[3]=ch>>8;return 4;
 }
 assert(rt==0x23&&!n&&!data);f->writes++;
 if(request==3){assert(value==4);f->resets++;f->polls=0;if(f->mode==9)return -19;}
 else {assert(request==1&&(value==16||value==20||value==25||value==29));f->clears++;}
 return 0;
}
static int run(struct Fixture *f,unsigned *status)
{active=f;return tmc_linux_hub_port_init(control,f,3,3,status);}
int main(void)
{
 unsigned status=0xdead;struct Fixture f={0};
 assert(!run(&f,&status)&&status==(1|2|1024)&&f.resets==1&&f.ms>=180&&f.clears);
 f=(struct Fixture){.mode=1};assert(run(&f,&status)==-ENOTCONN&&!f.ms&&!f.resets);
 f=(struct Fixture){.mode=2};assert(run(&f,&status)==-ETIMEDOUT&&f.ms==2000&&!f.resets);
 f=(struct Fixture){.mode=3};assert(run(&f,&status)==-EIO&&!f.resets);
 f=(struct Fixture){.mode=4};assert(run(&f,&status)==-EIO&&!f.resets);
 f=(struct Fixture){.mode=5};assert(run(&f,&status)==-EIO&&!f.resets);
 f=(struct Fixture){.mode=6};int result=run(&f,&status);assert(result==-EBUSY&&f.resets==5&&f.ms==3930);
 f=(struct Fixture){.mode=7};assert(run(&f,&status)==-ENOTCONN&&f.resets==1);
 f=(struct Fixture){.mode=8};assert(!run(&f,&status)&&f.resets==2&&f.clears>=2);
 f=(struct Fixture){.mode=9};assert(run(&f,&status)==-ENODEV&&f.resets==1);
 assert(tmc_linux_hub_port_init(control,&f,0,3,&status)==-EINVAL);
 assert(tmc_linux_hub_port_init(control,&f,16,3,&status)==-EINVAL);
 f=(struct Fixture){0};active=&f;assert(!tmc_linux_hub_port_init(control,&f,3,5,&status)&&f.resets==1&&f.clears>=3);
 assert(tmc_linux_hub_port_init(control,&f,3,6,&status)==-EINVAL);
 f=(struct Fixture){.start_writer=1};assert(!run(&f,&status));assert(!pthread_join(writer,0)&&writer_done);
 assert(!__atomic_load_n(&ehci_cf_port_reset_rwsem,__ATOMIC_ACQUIRE));
 for(unsigned i=0;i<1024;i++){f=(struct Fixture){0};assert(!run(&f,&status));}
 puts("PASS original Linux hub.c debounce/reset: 100ms stable window, connection bounce/2000ms timeout, strict actual status length, disconnected/overcurrent/error paths, 5 reset attempts, reconnect retry, recovery delay, real CONFIGFLAG reader/writer exclusion, 1024 port lifecycles; not whole hub class/hotplug");
}
