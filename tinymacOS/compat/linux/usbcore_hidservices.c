#include <tmc_kernel.h>
#include <linux/semaphore.h>
int down_trylock(struct semaphore *s){u32 f=k_irq_save();int busy=!s->count;if(!busy)s->count--;k_irq_restore(f);return busy;}
void down(struct semaphore *s){BUG_ON(down_trylock(s));}
int down_interruptible(struct semaphore *s){down(s);return 0;}
void up(struct semaphore *s){u32 f=k_irq_save();BUG_ON(s->count==UINT_MAX);s->count++;k_irq_restore(f);}
int abs(int x){return x<0?-x:x;}
char *strrchr(const char *s,int c){const char *last=NULL;do{if(*s==(char)c)last=s;}while(*s++);return (char *)last;}
