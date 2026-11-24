/* SPDX-License-Identifier: GPL-2.0-only */
#include "dma-include/tmc_dma_compat.h"
static TmcLinuxDmaStats tmc_linux_dma_counters;
#define DCOUNT(f) __atomic_add_fetch(&tmc_linux_dma_counters.f,1,__ATOMIC_RELAXED)
static void dma_live_add(size_t n)
{
 u64 v=__atomic_add_fetch(&tmc_linux_dma_counters.live_bytes,n,__ATOMIC_RELAXED),old=__atomic_load_n(&tmc_linux_dma_counters.highwater,__ATOMIC_RELAXED);
 while(old<v&&!__atomic_compare_exchange_n(&tmc_linux_dma_counters.highwater,&old,v,0,__ATOMIC_RELAXED,__ATOMIC_RELAXED)){}
}
void *dma_kmalloc(size_t n,gfp_t flags)
{if(!n||n>65536||(flags&~(GFP_ATOMIC|__GFP_ZERO))){DCOUNT(rejected);return NULL;}void *p=tmc_dma_os_alloc(n);if(!p){DCOUNT(failures);return NULL;}if(flags&__GFP_ZERO)memset(p,0,n);return p;}
void *dma_kzalloc(size_t n,gfp_t flags){return dma_kmalloc(n,flags|__GFP_ZERO);}
void dma_kfree(void *p){if(p)tmc_dma_os_free(p);}
long dma_strscpy(char *out,const char *in,size_t n)
{if(!n)return -7;size_t i=0;while(i+1<n&&in[i]){out[i]=in[i];i++;}out[i]=0;return in[i]?-7:(long)i;}
void dma_might_alloc(gfp_t f){if(f&~(GFP_ATOMIC|__GFP_ZERO))DCOUNT(rejected);}
void *dma_alloc_coherent(struct device *dev,size_t n,dma_addr_t *dma,gfp_t flags)
{
 if(!dev||!dma||!n||n>65536||(flags&~(GFP_ATOMIC|__GFP_ZERO))){DCOUNT(rejected);return NULL;}
 u64 handle=0;void *p=tmc_dma_os_coherent(n,&handle);
 if(!p){DCOUNT(failures);return NULL;}
 size_t alignment=PAGE_SIZE;while(alignment<n)alignment<<=1;
 if(((u64)p&(alignment-1))||(handle&(alignment-1))||handle>dev->mask||n-1>dev->mask-handle){tmc_dma_os_coherent_free(n,p,handle);DCOUNT(rejected);return NULL;}
 memset(p,0,n);*dma=handle;DCOUNT(coherent_allocations);dma_live_add(n);return p;
}
void dma_free_coherent(struct device *dev,size_t n,void *p,dma_addr_t dma)
{
 (void)dev;if(!p)return;tmc_dma_os_coherent_free(n,p,dma);DCOUNT(coherent_frees);__atomic_sub_fetch(&tmc_linux_dma_counters.live_bytes,n,__ATOMIC_RELAXED);
}
void dma_error(struct device *d,const char *fmt,...){(void)d;(void)fmt;DCOUNT(rejected);tmc_lx_os_error("Linux DMA pool: allocator diagnostic");}
int dma_warn(int condition){if(condition)dma_error(NULL,"warning");return condition;}
void print_hex_dump(const char *level,const char *prefix,int style,int rows,int groups,const void *buffer,size_t n,bool ascii)
{
 (void)level;(void)prefix;(void)style;(void)rows;(void)groups;(void)ascii;
 /* Bounded diagnostic: no kernel addresses or arbitrary DMA payload dump.
  * Record whether corrupt payload exists; allocation safety stays upstream. */
 const volatile u8 *p=buffer;unsigned checksum=0;for(size_t i=0;i<n&&i<64;i++)checksum^=p[i];
 (void)checksum;DCOUNT(rejected);tmc_lx_os_error("Linux DMA pool: poisoned memory corruption");
}
int device_create_file(struct device *d,struct device_attribute *attr)
{
 if(!d||!attr||!attr->show)return -EINVAL;u32 f=dma_lock(&d->resource_lock);int result=d->attribute?-EEXIST:0;
 if(!result){d->attribute=attr;DCOUNT(attributes);}dma_unlock(&d->resource_lock,f);return result;
}
void device_remove_file(struct device *d,struct device_attribute *attr)
{u32 f=dma_lock(&d->resource_lock);if(d->attribute==attr)d->attribute=NULL;dma_unlock(&d->resource_lock,f);}
/* Bounded formatter for actual original pools_show formats: strings, decimal
 * unsigned/size_t, left/right field widths, literal percent. No libc needed. */
static int dma_emit(char *out,size_t at,const char *fmt,va_list ap)
{
 if(at>=PAGE_SIZE)return 0;size_t pos=at;
 #define EMIT(c) do{if(pos<PAGE_SIZE-1)out[pos++]=(c);}while(0)
 for(const char *s=fmt;*s;s++){
  if(*s!='%'){EMIT(*s);continue;}s++;if(*s=='%'){EMIT('%');continue;}
  int left=0;unsigned width=0;if(*s=='-'){left=1;s++;}while(*s>='0'&&*s<='9'){width=width*10+(*s++-'0');if(width>1024)return -EINVAL;}
  int z=*s=='z';if(z)s++;const char *text;char digits[32];size_t count=0;
  if(*s=='s'&&!z){text=va_arg(ap,const char *);if(!text)text="(null)";while(text[count]&&count<PAGE_SIZE)count++;}
  else if(*s=='u'){u64 value=z?va_arg(ap,size_t):va_arg(ap,unsigned);unsigned n=0;do{digits[n++]=(char)('0'+value%10);value/=10;}while(value);for(unsigned a=0;a<n/2;a++){char c=digits[a];digits[a]=digits[n-a-1];digits[n-a-1]=c;}text=digits;count=n;}
  else return -EINVAL;
  if(!left)for(size_t i=count;i<width;i++)EMIT(' ');for(size_t i=0;i<count;i++)EMIT(text[i]);if(left)for(size_t i=count;i<width;i++)EMIT(' ');
 }
 out[pos]=0;return pos-at;
 #undef EMIT
}
int dma_sysfs_emit(char *out,const char *fmt,...){va_list ap;va_start(ap,fmt);int r=dma_emit(out,0,fmt,ap);va_end(ap);return r;}
int dma_sysfs_emit_at(char *out,size_t offset,const char *fmt,...){va_list ap;va_start(ap,fmt);int r=dma_emit(out,offset,fmt,ap);va_end(ap);return r;}
struct DmaResource {struct DmaResource *next;void (*release)(struct device *,void *);u64 pad;u8 data[];};
void *devres_alloc(void (*release)(struct device *,void *),size_t n,gfp_t flags)
{if(!release||n>65536-sizeof(struct DmaResource))return NULL;struct DmaResource *r=dma_kzalloc(sizeof(*r)+n,flags);if(!r)return NULL;r->release=release;return r->data;}
void devres_add(struct device *d,void *p)
{struct DmaResource *r=container_of(p,struct DmaResource,data);u32 f=dma_lock(&d->resource_lock);r->next=d->resources;d->resources=r;dma_unlock(&d->resource_lock,f);DCOUNT(devres);}
void devres_free(void *p){if(p)dma_kfree(container_of(p,struct DmaResource,data));}
int devres_release(struct device *d,void (*release)(struct device *,void *),int (*match)(struct device *,void *,void *),void *data)
{
 u32 f=dma_lock(&d->resource_lock);struct DmaResource **link=&d->resources,*found=NULL;
 while(*link){struct DmaResource *r=*link;if(r->release==release&&(!match||match(d,r->data,data))){*link=r->next;found=r;break;}link=&r->next;}
 dma_unlock(&d->resource_lock,f);if(!found)return -ENOENT;found->release(d,found->data);dma_kfree(found);return 0;
}
void dma_release_resources(struct device *d)
{
 for(;;){u32 f=dma_lock(&d->resource_lock);struct DmaResource *r=d->resources;if(r)d->resources=r->next;dma_unlock(&d->resource_lock,f);if(!r)break;r->release(d,r->data);dma_kfree(r);}
}
#include "../../vendor/linux-v6.12/dmapool.c"
/* Checked native facade; drivers do not share private Linux struct device
 * objects with the older Boot/URB/HID units. No fake DMA address casts here. */
struct TmcLinuxDmaDevice {struct device core;unsigned pools;char name[48];};
struct TmcLinuxDmaPool {struct dma_pool *core;TmcLinuxDmaDevice *device;spinlock_t operations;};
TmcLinuxDmaDevice *tmc_linux_dma_device_create(const char *name,u64 mask)
{
 if(!name||mask<PAGE_SIZE-1){DCOUNT(rejected);return NULL;}TmcLinuxDmaDevice *d=dma_kzalloc(sizeof(*d),GFP_KERNEL);if(!d)return NULL;
 dma_strscpy(d->name,name,sizeof d->name);d->core.name=d->name;d->core.mask=mask;INIT_LIST_HEAD(&d->core.dma_pools);DCOUNT(devices);return d;
}
int tmc_linux_dma_device_destroy(TmcLinuxDmaDevice *d)
{
 if(!d)return -EINVAL;if(__atomic_load_n(&d->pools,__ATOMIC_ACQUIRE)||!list_empty(&d->core.dma_pools)||d->core.resources)return -EBUSY;
 dma_kfree(d);return 0;
}
int tmc_linux_dma_device_snapshot(TmcLinuxDmaDevice *d,char *out,size_t n)
{
 if(!d||!out||!n)return -EINVAL;char *buffer=dma_kzalloc(PAGE_SIZE,GFP_KERNEL);if(!buffer)return -ENOMEM;
 u32 f=dma_lock(&d->core.resource_lock);struct device_attribute *a=d->core.attribute;int r=a?a->show(&d->core,a,buffer):-ENOENT;
 if(r>=0){size_t copy=min((size_t)r,n-1);memcpy(out,buffer,copy);out[copy]=0;}
 dma_unlock(&d->core.resource_lock,f);dma_kfree(buffer);return r;
}
TmcLinuxDmaPool *tmc_linux_dma_pool_create(TmcLinuxDmaDevice *d,const char *name,size_t size,size_t alignment,size_t boundary)
{
 if(!d||!name||!size||size>65536||alignment>65536||boundary>65536){DCOUNT(rejected);return NULL;}
 TmcLinuxDmaPool *p=dma_kzalloc(sizeof(*p),GFP_KERNEL);if(!p)return NULL;
 p->core=dma_pool_create(name,&d->core,size,alignment,boundary);if(!p->core){dma_kfree(p);DCOUNT(failures);return NULL;}
 p->device=d;__atomic_add_fetch(&d->pools,1,__ATOMIC_ACQ_REL);DCOUNT(pools);return p;
}
void *tmc_linux_dma_pool_alloc(TmcLinuxDmaPool *p,u64 *dma)
{
 if(!p||!dma){DCOUNT(rejected);return NULL;}u32 f=dma_lock(&p->operations);void *b=dma_pool_alloc(p->core,GFP_ATOMIC|__GFP_ZERO,dma);
 if(b)DCOUNT(allocations);dma_unlock(&p->operations,f);return b;
}
static int dma_valid_block(struct dma_pool *p,void *v,u64 dma)
{
 struct dma_page *page;int valid=0;list_for_each_entry(page,&p->page_list,page_list){
  if((u64)v<(u64)page->vaddr||(u64)v-(u64)page->vaddr>=p->allocation)continue;
  size_t at=(u64)v-(u64)page->vaddr,phase=at%p->boundary;
  valid=dma==page->dma+at&&at+p->size<=p->allocation&&phase+p->size<=p->boundary&&!(phase%p->size);break;
 }
 if(!valid)return 0;for(struct dma_block *b=p->next_block;b;b=b->next_block)if(b==v)return 0;return 1;
}
int tmc_linux_dma_pool_free(TmcLinuxDmaPool *p,void *v,u64 dma)
{
 if(!p||!v)return -EINVAL;u32 f=dma_lock(&p->operations);int result=0;
 if(!dma_valid_block(p->core,v,dma)){DCOUNT(rejected);result=-EINVAL;}else{dma_pool_free(p->core,v,dma);DCOUNT(frees);}
 dma_unlock(&p->operations,f);return result;
}
int tmc_linux_dma_pool_destroy(TmcLinuxDmaPool *p)
{
 if(!p)return -EINVAL;u32 f=dma_lock(&p->operations);if(p->core->nr_active){dma_unlock(&p->operations,f);return -EBUSY;}
 dma_pool_destroy(p->core);__atomic_sub_fetch(&p->device->pools,1,__ATOMIC_ACQ_REL);dma_unlock(&p->operations,f);dma_kfree(p);return 0;
}
void tmc_linux_dma_pool_stats(TmcLinuxDmaStats *out)
{
 if(!out)return;
 #define COPY(f) out->f=__atomic_load_n(&tmc_linux_dma_counters.f,__ATOMIC_RELAXED)
 COPY(devices);COPY(pools);COPY(allocations);COPY(frees);COPY(coherent_allocations);COPY(coherent_frees);COPY(live_bytes);COPY(highwater);COPY(rejected);COPY(failures);COPY(attributes);COPY(devres);
 #undef COPY
}
