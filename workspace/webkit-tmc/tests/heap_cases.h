/* Shared actual heap checks; host and guest use DIFFERENT VM/lock transports. */
#include "tmc_heap.h"
#include <stdint.h>
#include <errno.h>
#ifndef HEAP_SAMPLE
#define HEAP_SAMPLE() ((void)0)
#endif
static unsigned heap_failure;
#define HC(x) do{if(!(x)){unsigned z=0;__atomic_compare_exchange_n(&heap_failure,&z,__LINE__,0,__ATOMIC_RELAXED,__ATOMIC_RELAXED);}}while(0)
static uint32_t rng(uint32_t *s){*s=*s*1664525u+1013904223u;return *s;}
static void fill(void *p,size_t n,unsigned v){unsigned char *b=p;for(size_t i=0;i<n;i++)b[i]=(unsigned char)(v+i*37);}
static int intact(const void *p,size_t n,unsigned v){const unsigned char*b=p;for(size_t i=0;i<n;i++)if(b[i]!=(unsigned char)(v+i*37))return 0;return 1;}
static void heap_random(uint32_t seed,unsigned rounds)
{
 struct Item{void*p;size_t n;unsigned v;} slots[32]={{0}};
 for(unsigned j=0;j<rounds;j++){
  if(!(j%16))HEAP_SAMPLE();
  struct Item *s=slots+(rng(&seed)%32);
  if(s->p)HC(intact(s->p,s->n,s->v));
  unsigned action=rng(&seed)%3;
  if(s->p&&action==0){tmc_heap_free(s->p);*s=(struct Item){0};continue;}
  size_t n=(rng(&seed)%8192)+1;void *p=s->p?tmc_heap_realloc(s->p,n):tmc_heap_alloc(n);
  HC(p&& !((uintptr_t)p&15));if(!p)continue;
  if(s->p)HC(intact(p,s->n<n?s->n:n,s->v));
  s->p=p;s->n=n;s->v=rng(&seed);fill(p,n,s->v);
 }
 for(unsigned i=0;i<32;i++)if(slots[i].p){HC(intact(slots[i].p,slots[i].n,slots[i].v));tmc_heap_free(slots[i].p);}
}
static void heap_sequential(void)
{
 errno=91;tmc_heap_free(0);HC(errno==91);
 void *a=tmc_heap_alloc(0),*b=tmc_heap_calloc(0,32);HC(a&&b&&a!=b);tmc_heap_free(a);tmc_heap_free(b);
 errno=0;HC(!tmc_heap_calloc(SIZE_MAX,2)&&errno==ENOMEM);
 size_t alignments[]={8,16,64,4096,2097152};
 for(unsigned i=0;i<5;i++){size_t al=alignments[i];void*p=tmc_heap_aligned_alloc(al,al);HC(p&&!((uintptr_t)p&(al-1)));if(p){fill(p,al,8);HC(intact(p,al,8));tmc_heap_free(p);}}
 errno=0;HC(!tmc_heap_aligned_alloc(0,32)&&errno==EINVAL);HC(!tmc_heap_aligned_alloc(3,33));HC(!tmc_heap_aligned_alloc(32,17));
 a=tmc_heap_alloc(512);b=tmc_heap_alloc(512);HC(a&&b);if(!a||!b)return;
 fill(a,512,17);fill(b,512,99);tmc_heap_free(a);a=tmc_heap_calloc(1,256);HC(a);if(a)for(unsigned i=0;i<256;i++)HC(!((unsigned char*)a)[i]);
 HC(intact(b,512,99));errno=0;HC(!tmc_heap_realloc(b,SIZE_MAX)&&errno==ENOMEM);HC(intact(b,512,99));
 errno=123;tmc_heap_free(a);HC(errno==123);HC(!tmc_heap_realloc(b,0));
 /* Realloc grows into its freed neighbour, then shrinks/splits, without copying. */
 a=tmc_heap_alloc(512);b=tmc_heap_alloc(2048);void*c=tmc_heap_alloc(512);HC(a&&b&&c);
 if(a&&b&&c){fill(a,512,42);tmc_heap_free(b);b=tmc_heap_realloc(a,2048);HC(b==a&&intact(b,512,42));a=tmc_heap_realloc(b,32);HC(a==b&&intact(a,32,42));tmc_heap_free(a);tmc_heap_free(c);}
 heap_random(7891,5000);
 struct tmc_heap_stats st=tmc_heap_statistics();HC(!st.live&&!st.requested&&!st.spans&&!st.reserved);
}
struct HeapJob{unsigned seed,rounds;void *transfer;};
static void *heap_worker(void *arg)
{
 struct HeapJob *j=arg;HC(intact(j->transfer,1024,j->seed));tmc_heap_free(j->transfer);j->transfer=0;
 heap_random(j->seed,j->rounds);return arg;
}
