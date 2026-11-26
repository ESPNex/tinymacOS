/* SPDX-License-Identifier: MIT
 * Correctness-first shared heap over native guarded VM reservations. Metadata
 * is in-band; not a hardened allocator. No capacity-sized side tables. */
#include "tmc_heap.h"
#include "tmc_vm.h"
#include "tmc_sync.h"
#include <errno.h>
#include <stdint.h>
#define ALIGN 16u
#define SPAN_MIN (256u*1024u)
#define COOKIE ((uintptr_t)0x746d636865617031ull)
typedef struct Block {size_t bytes,requested;struct Block *prev,*next;unsigned used;unsigned reserved;uint64_t padding;} Block;
typedef struct {Block *block;uintptr_t cookie;} Prefix;
typedef struct {void *base;size_t bytes;Block *first;} Span;
_Static_assert(sizeof(Block)%ALIGN==0,"heap block alignment");
_Static_assert(sizeof(Prefix)==ALIGN,"heap prefix alignment");
static Span spans[TMC_VM_SLOTS];
static size_t cache_limit,reserve_calls,release_calls,released_spans,cache_hits;
static struct tmc_mutex mutex=TMC_MUTEX_INITIALIZER;
static void lock(void){if(tmc_mutex_lock(&mutex))__builtin_trap();}
static void unlock(void){if(tmc_mutex_unlock(&mutex))__builtin_trap();}
static int add(size_t a,size_t b,size_t *r){if(a>SIZE_MAX-b)return -1;*r=a+b;return 0;}
static int round_up(size_t n,size_t a,size_t *r){size_t v;if(add(n,a-1,&v))return -1;*r=v&~(a-1);return 0;}
static void split(Block *b,size_t bytes)
{
 if(b->bytes-bytes<sizeof(Block)+sizeof(Prefix)+ALIGN)return;
 Block *tail=(void*)((unsigned char*)b+bytes);
 *tail=(Block){.bytes=b->bytes-bytes,.prev=b,.next=b->next};
 if(tail->next)tail->next->prev=tail;
 b->next=tail;b->bytes=bytes;
}
static void merge_next(Block *b)
{
 Block *n=b->next;if(!n||n->used)return;
 b->bytes+=n->bytes;b->next=n->next;if(b->next)b->next->prev=b;
}
static void *take(Block *b,size_t n,size_t alignment)
{
 size_t at,end;
 if(round_up((uintptr_t)b+sizeof(Block)+sizeof(Prefix),alignment,&at)||
    add(at-(uintptr_t)b,n?n:1,&end)||round_up(end,ALIGN,&end)||end>b->bytes)return 0;
 split(b,end);b->used=1;b->requested=n;
 Prefix *p=(void*)(at-sizeof(Prefix));p->block=b;p->cookie=COOKIE^at^(uintptr_t)b;
 return (void*)at;
}
static int empty(const Span *s){return s->first&&!s->first->used&&!s->first->prev&&!s->first->next;}
static int release_span(Span *s)
{
 release_calls++;if(tmc_vm_release(s->base,s->bytes))return -1;
 released_spans++;*s=(Span){0};return 0;
}
static size_t reclaim_empty(void)
{
 size_t count=0;for(unsigned i=0;i<TMC_VM_SLOTS;i++)if(spans[i].base&&(!spans[i].first||empty(spans+i)))if(!release_span(spans+i))count++;return count;
}
static void *allocate(size_t n,size_t alignment)
{
 size_t need;
 if(add(n?n:1,sizeof(Block)+sizeof(Prefix),&need)||add(need,alignment-1,&need)||
    round_up(need,TMC_VM_PAGE,&need)||need>TMC_VM_MAX_REGION){errno=ENOMEM;return 0;}
 for(unsigned i=0;i<TMC_VM_SLOTS;i++)for(Block *b=spans[i].first;b;b=b->next)
  if(!b->used){int cached=empty(spans+i);void *p=take(b,n,alignment);if(p){if(cached)cache_hits++;return p;}}
 unsigned slot=0;while(slot<TMC_VM_SLOTS&&spans[slot].base)slot++;
 if(slot==TMC_VM_SLOTS){
  if(!reclaim_empty()){errno=ENOMEM;return 0;}
  slot=0;while(slot<TMC_VM_SLOTS&&spans[slot].base)slot++;
 }
 if(need<SPAN_MIN)need=SPAN_MIN;
 reserve_calls++;void *base=tmc_vm_reserve(need,TMC_VM_PAGE,1);
 if(!base&&reclaim_empty()){reserve_calls++;base=tmc_vm_reserve(need,TMC_VM_PAGE,1);}
 if(!base)return 0;
 if(tmc_vm_commit(base,need,TMC_VM_READ|TMC_VM_WRITE)){
  int error=errno;
  /* If release fails, keep ownership so this reservation cannot be forgotten.
   * Retry release on a later allocation, without dereferencing the reservation. */
  spans[slot]=(Span){base,need,0};(void)release_span(spans+slot);
  errno=error;return 0;
 }
 Block *b=base;*b=(Block){.bytes=need};spans[slot]=(Span){base,need,b};
 void *p=take(b,n,alignment);if(!p)__builtin_trap();return p;
}
static Block *lookup(void *p,Span **owner)
{
 uintptr_t at=(uintptr_t)p;
 for(unsigned i=0;i<TMC_VM_SLOTS;i++){
  Span *s=spans+i;uintptr_t base=(uintptr_t)s->base;
  if(!s->first||at<base+sizeof(Block)+sizeof(Prefix)||at>=base+s->bytes)continue;
  if(at&(ALIGN-1))__builtin_trap();
  Prefix *prefix=(void*)(at-sizeof(Prefix));
  for(Block *b=s->first;b;b=b->next)if(b==prefix->block){
   if(!b->used||at<(uintptr_t)b+sizeof(Block)+sizeof(Prefix)||
      at>=(uintptr_t)b+b->bytes||prefix->cookie!=(COOKIE^at^(uintptr_t)b))__builtin_trap();
   *owner=s;return b;
  }
  __builtin_trap();
 }
 __builtin_trap();
}
static void release_block(Block *b,Span *s)
{
 b->used=0;b->requested=0;merge_next(b);
 if(b->prev&&!b->prev->used){b=b->prev;merge_next(b);}
 if(!b->prev&&!b->next){
  size_t cached=0;for(unsigned i=0;i<TMC_VM_SLOTS;i++)if(spans+i!=s&&empty(spans+i))cached+=spans[i].bytes;
  if(s->bytes<=cache_limit&&cached<=cache_limit-s->bytes)return;
  (void)release_span(s);
 }
}
static void reclaim_uncommitted(void)
{
 /* A failed commit followed by failed release leaves a tracked reservation,
  * not a dereferenceable heap block. Releasing it is safe and idempotent. */
 for(unsigned i=0;i<TMC_VM_SLOTS;i++)if(spans[i].base&&!spans[i].first)
  (void)release_span(spans+i);
}
void *tmc_heap_alloc(size_t n)
{
 lock();reclaim_uncommitted();void *p=allocate(n,ALIGN);unlock();return p;
}
void *tmc_heap_aligned_alloc(size_t alignment,size_t n)
{
 if(alignment<sizeof(void*)||(alignment&(alignment-1))||alignment>TMC_VM_MAX_ALIGNMENT||n%alignment){errno=EINVAL;return 0;}
 if(alignment<ALIGN)alignment=ALIGN;
 lock();reclaim_uncommitted();void *p=allocate(n,alignment);unlock();return p;
}
void tmc_heap_free(void *p)
{
 if(!p)return;
 int error=errno;lock();Span *s=0;Block *b=lookup(p,&s);release_block(b,s);unlock();errno=error;
}
void *tmc_heap_calloc(size_t count,size_t bytes)
{
 if(bytes&&count>SIZE_MAX/bytes){errno=ENOMEM;return 0;}
 size_t n=count*bytes;unsigned char *p=tmc_heap_alloc(n);if(p)for(size_t i=0;i<n;i++)p[i]=0;return p;
}
void *tmc_heap_realloc(void *p,size_t n)
{
 if(!p)return tmc_heap_alloc(n);
 if(!n){tmc_heap_free(p);return 0;}
 lock();Span *s=0;Block *b=lookup(p,&s);size_t end,old=b->requested;
 if(!add((uintptr_t)p-(uintptr_t)b,n,&end)&&!round_up(end,ALIGN,&end)){
  if(end>b->bytes&&b->next&&!b->next->used&&end<=b->bytes+b->next->bytes)merge_next(b);
  if(end<=b->bytes){split(b,end);if(b->next&&!b->next->used)merge_next(b->next);b->requested=n;unlock();return p;}
 }
 void *q=allocate(n,ALIGN);
 if(q){size_t length=old<n?old:n;for(size_t i=0;i<length;i++)((unsigned char*)q)[i]=((unsigned char*)p)[i];release_block(b,s);}
 unlock();return q;
}
struct tmc_heap_stats tmc_heap_statistics(void)
{
 struct tmc_heap_stats r={0};lock();
 r.cache_limit=cache_limit;r.reserve_calls=reserve_calls;r.release_calls=release_calls;r.released_spans=released_spans;r.cache_hits=cache_hits;
 for(unsigned i=0;i<TMC_VM_SLOTS;i++)if(spans[i].base){r.spans++;r.reserved+=spans[i].bytes;if(empty(spans+i))r.cached+=spans[i].bytes;
  for(Block *b=spans[i].first;b;b=b->next)if(b->used){r.live++;r.requested+=b->requested;}}
 unlock();return r;
}

int tmc_heap_set_cache_limit(size_t bytes)
{
 if(bytes>SPAN_MIN){errno=EINVAL;return -1;}
 int saved=errno;lock();cache_limit=bytes;
 size_t retained=0;int error=0;
 for(unsigned i=0;i<TMC_VM_SLOTS;i++)if(empty(spans+i)){
  if(spans[i].bytes<=bytes-retained)retained+=spans[i].bytes;
  else if(release_span(spans+i)&&!error)error=errno;
 }
 unlock();errno=error?error:saved;return error?-1:0;
}
int tmc_heap_trim(void)
{
 int saved=errno,error=0;lock();
 for(unsigned i=0;i<TMC_VM_SLOTS;i++)if(spans[i].base&&(!spans[i].first||empty(spans+i)))
  if(release_span(spans+i)&&!error)error=errno;
 unlock();errno=error?error:saved;return error?-1:0;
}
