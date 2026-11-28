#include "k.h"
extern char _ktxt[], _kro[], _krw[], _kend[];
static u8 used[1 << 21];
static Spin kmem_lock, heap_lock;
static u64 *vm_new_l(void);
static void *kmalloc_l(size_t n);
static void kfree_l(void *p);
u64 totalpages, freepages, *kroot;
static int hint = 256;
static unsigned limit;
static u64 firmware_pages,userpages;
static void mark(unsigned n) {
    used[n >> 3] |= 1u << (n & 7);
}
static void clear(unsigned n) {
    used[n >> 3] &= ~(1u << (n & 7));
}
static u64 page_get_l(void) {
    if (!freepages)
        return 0;
    for (unsigned tries = 0; tries < limit; tries++) {
        if ((unsigned)hint >= limit)
            hint = 256;
        unsigned n = hint++;
        if (!(used[n >> 3] & (1u << (n & 7)))) {
            mark(n);
            freepages--;
            memset((void *)((u64)n << 12), 0, PAGE);
            return (u64)n << 12;
        }
    }
    return 0;
}
static void page_put_l(u64 p) {
    if (!p || p % PAGE || p / PAGE >= limit)
        panic("bad page release", p, 0);
    clear(p / PAGE);
    freepages++;
    if (p / PAGE < (unsigned)hint)
        hint = p / PAGE;
}
u64 page_get(void) {
    u32 fl;
    spin_lock(&kmem_lock, &fl);
    u64 r = page_get_l();
    spin_unlock(&kmem_lock, fl);
    return r;
}
void page_put(u64 p) {
    u32 fl;
    spin_lock(&kmem_lock, &fl);
    page_put_l(p);
    spin_unlock(&kmem_lock, fl);
}
/* Contiguous supervisor identity-map spans for large codec workspaces.
 * No new virtual mappings or changes to user page tables/TLB ownership.
 * Metadata and allocation marking are atomic under the existing page lock;
 * clearing the data happens after releasing that leaf lock. */
typedef struct {u64 magic,pages,requested;} KernelSpan;
#define KERNEL_SPAN_MAGIC 0x544d435350414e38ull
void*kernel_span_alloc(size_t bytes){
 if(!bytes||bytes>~(size_t)0-2*PAGE)return NULL;u64 wanted=(bytes+PAGE-1)/PAGE+1;
 u32 flags;spin_lock(&kmem_lock,&flags);u64 first=0;unsigned run=0;
 if(wanted<=freepages&&wanted<limit){for(unsigned n=256;n<limit;n++){
  if(used[n>>3]&(1u<<(n&7))){run=0;continue;}if(!run)first=n;
  if(++run==wanted){for(unsigned k=0;k<run;k++)mark(first+k);freepages-=wanted;if((unsigned)hint>=first&&(unsigned)hint<first+wanted)hint=first+wanted;break;}
 }}
 if(run!=wanted)first=0;spin_unlock(&kmem_lock,flags);if(!first)return NULL;
 KernelSpan*h=(void*)(first*PAGE);memset(h,0,wanted*PAGE);*h=(KernelSpan){KERNEL_SPAN_MAGIC,wanted,bytes};return (u8*)h+PAGE;
}
void kernel_span_free(void*p){
 if(!p)return;u64 address=(u64)p;if(address<PAGE||address%PAGE)panic("bad kernel span",address,0);
 KernelSpan*h=(void*)(address-PAGE);u32 flags;spin_lock(&kmem_lock,&flags);u64 first=(u64)h/PAGE;if(first<256||first>=limit)panic("kernel span outside RAM",address,0);u64 n=h->pages;
 if(h->magic!=KERNEL_SPAN_MAGIC||n<2||n>limit-first)panic("invalid kernel span",address,0);
 for(u64 k=0;k<n;k++)if(!(used[(first+k)>>3]&(1u<<((first+k)&7))))panic("duplicate kernel span release",address,0);
 h->magic=0;for(u64 k=0;k<n;k++)page_put_l((first+k)*PAGE);spin_unlock(&kmem_lock,flags);
}
static u64 table(void) {
    u64 p = page_get_l();
    if (!p)
        panic("kernel page tables exhausted", 0, 0);
    return p;
}
void mem_init(void) {
    u32 kfl;
    spin_lock(&kmem_lock, &kfl);
    memset(used, 255, sizeof used);
    for (u64 o = 0; o < boot.bytes; o += boot.step) {
        u8 *d = (u8 *)(boot.map + o);
        u32 type=*(u32 *)d;
        /* UEFI RAM types, not MMIO/reserved holes and not installed DIMM size.
         * Firmware/loader/runtime/ACPI pages are not silently called free. */
        if((type>=1&&type<=7)||type==9||type==10)
            firmware_pages+=*(u64 *)(d+24);
        if (type != 7)
            continue;
        u64 start = *(u64 *)(d + 8) >> 12, n = *(u64 *)(d + 24);
        for (u64 j = MAX(256ull, start); j < MIN(start + n, 16777216ull); j++) {
            clear(j);
            freepages++;
            limit = MAX(limit, j + 1);
        }
    }
    totalpages = freepages;
    if (totalpages < 4096)
        panic("not enough conventional RAM", totalpages, 0);
    kroot = (u64 *)table();
    u64 *pdpt = (u64 *)table();
    kroot[0] = (u64)pdpt | 3;
    for (int g = 0; g < 64; g++) {
        u64 *pd = (u64 *)table();
        pdpt[g] = (u64)pd | 3;
        for (int j = 0; j < 512; j++) {
            u64 a = ((u64)g << 30) + ((u64)j << 21);
            if (a + 0x200000 > (u64)_ktxt && a < (u64)_kend) {
                u64 *pt = (u64 *)table();
                pd[j] = (u64)pt | 3;
                for (int k = 0; k < 512; k++) {
                    u64 p = a + PAGE * k, flags = 3 | NX;
                    if (p >= (u64)_ktxt && p < (u64)_kro)
                        flags = 1;
                    else if (p >= (u64)_kro && p < (u64)_krw)
                        flags = 1 | NX;
                    pt[k] = p | flags;
                }
            } else if (a == 0)
                pd[j] = a | 0x83; // low 2 MiB: AP trampoline executes here
            else {
                /* GOP scanout is device memory, not write-back RAM. An explicit
                 * UC mapping avoids dirty CPU cache lines being mistaken for
                 * pixels already visible to the display controller. */
                u64 fb_end=boot.fb+(u64)boot.stride*boot.h*4;
                u64 cache=(boot.fb && a<fb_end && a+0x200000>boot.fb)?0x18:0;
                pd[j] = a | 0x83 | NX | cache;
            }
        }
    }
    u32 r[4];
    cpuid(0x80000001, r);
    if (!(r[3] & (1u << 20)))
        panic("NX required", 0, 0);
    wrmsr(0xc0000080, rdmsr(0xc0000080) | (1 << 11));
    asm volatile("mov %0,%%cr3"::"r"(kroot):"memory");
    u64 cr;
    asm volatile("mov %%cr0,%0":"=r"(cr));
    cr|=1<<16;
    asm volatile("mov %0,%%cr0"::"r"(cr):"memory");
    spin_unlock(&kmem_lock, kfl);
    logstr("[mm] CR3 installed; supervisor identity map, NX + WP; usable pages=");
    lognum(totalpages);
    logstr("\n");
}
/* Make a kernel identity range executable. Kext images live in the kmalloc
 * heap, which the identity map marks NX; split covering 2 MiB pages into
 * 4 KiB tables where needed and clear NX on the touched pages. */
void kexec_x(u64 v, u64 len)
{
    if (!len || !v) return;
    u32 fl;
    spin_lock(&kmem_lock, &fl);
    for (u64 p = v & ~(PAGE - 1); p < v + len; p += PAGE) {
        u64 *pdpt = (u64 *)(kroot[0] & ADDR);
        u32 g = (u32)((p >> 30) & 63);
        if (!pdpt || !(pdpt[g] & 1)) break;
        u64 *pd = (u64 *)(pdpt[g] & ADDR);
        int j = (int)((p >> 21) & 511);
        if (!(pd[j] & 1)) break;
        if (pd[j] & 0x80) {                 /* 2 MiB page: split to 4 KiB */
            u64 *nt = (u64 *)page_get_l();
            if (!nt) break;
            u64 base = p & ~0x1fffffull;
            for (int k = 0; k < 512; k++)
                nt[k] = (base + (u64)k * PAGE) | 3 | NX;
            pd[j] = (u64)nt | 3;
        }
        u64 *pt = (u64 *)(pd[j] & ADDR);
        int k = (int)((p >> 12) & 511);
        pt[k] = (pt[k] & ~NX) | 3;          /* supervisor RW + execute */
        asm volatile("invlpg (%0)" :: "r"(p) : "memory");
    }
    spin_unlock(&kmem_lock, fl);
    tlb_shoot(0);                           /* drop stale NX on the other CPU */
}

u64 *vm_new(void) {
    u32 fl;
    spin_lock(&kmem_lock, &fl);
    u64 *r = vm_new_l();
    spin_unlock(&kmem_lock, fl);
    return r;
}
static u64 *vm_new_l(void) {
    u64 *r = (u64 *)page_get_l();
    if (r)
        r[0] = kroot[0];
    return r;
}
u64 *vm_pte(u64 *r, u64 a, int create) {
    if (!create) {
        // Read-only walks run lock-free: PTE stores are atomic 8-byte writes.
        return vm_pte_l(r, a, 0);
    }
    u32 fl;
    spin_lock(&kmem_lock, &fl);
    u64 *e = vm_pte_l(r, a, create);
    spin_unlock(&kmem_lock, fl);
    return e;
}
u64 *vm_pte_l(u64 *r, u64 a, int create) {
    if (!r || a < UBASE || a >= UEND)
        return 0;
    for (int z = 39; z >= 21; z -= 9) {
        int i = (a >> z) & 511;
        if (!(r[i] & 1)) {
            if (!create)
                return 0;
            u64 p = page_get_l();
            if (!p)
                return 0;
            r[i] = p | 7;
        }
        r = (u64 *)(r[i] & ADDR);
    }
    return &r[(a >> 12) & 511];
}
int vm_add(u64 *r, u64 a, u64 flags) {
    u32 fl;
    spin_lock(&kmem_lock, &fl);
    int ok = 0;
    u64 *e = vm_pte_l(r, a, 1);
    if (e && !*e) {
        u64 p = page_get_l();
        if (p) {
            *e = p | 5 | flags;
            userpages++;
            ok = 1;
        }
    }
    spin_unlock(&kmem_lock, fl);
    return ok;
}
/* Change only committed user-VM data pages; caller validates ownership/range.
 * No executable mappings. Caller serializes address-space mutations/uaccess. */
int vm_access(u64 *r,u64 a,u64 protection) {
    if(protection!=0 && protection!=TMC_VM_READ &&
       protection!=(TMC_VM_READ|TMC_VM_WRITE))return 0;
    u32 fl;spin_lock(&kmem_lock,&fl);
    u64 *e=vm_pte_l(r,a,0);int ok=e&&(*e&1);
    if(ok) {
        *e=(*e&ADDR)|1|NX|(protection&TMC_VM_READ?4:0)|(protection&TMC_VM_WRITE?2:0);
        asm volatile("invlpg (%0)"::"r"(a):"memory");
    }
    if(ok)tlb_shoot(a); /* ACK before permission change can be reported complete. */
    spin_unlock(&kmem_lock,fl);return ok;
}
void vm_unmap(u64 *r, u64 a) {
    u32 fl;
    spin_lock(&kmem_lock, &fl);
    u64 *e = vm_pte_l(r, a, 0);
    if (e && (*e & 1)) {
        u64 detached=*e&ADDR;
        *e = 0;
        /* kmem_lock prevents table mutation while peers acknowledge. Waiters
         * service the mailbox even with IRQs off; no page is reused early. */
        tlb_shoot(a);
        userpages--;
        page_put_l(detached);
    }
    spin_unlock(&kmem_lock, fl);
}
int user_ok(u64 a, u64 n, int write) {
    if (a < UBASE || a >= UEND || n > UEND - a)
        return 0;
    if (!n)
        return 1;
    for (u64 p = a & ~4095ull; p < a + n; p += PAGE) {
        u64 *e = vm_pte_l(current()->root, p, 0);
        if (!e || (*e & (write ? 7 : 5)) != (u64)(write ? 7 : 5))
            return 0;
    }
    return 1;
}
static void drop(u64 p, int level) {  // kmem_lock held
    if (!level)userpages--;
    if (level) {
        u64 *t = (u64 *)p;
        for (int i = 0; i < 512; i++)
            if (t[i] & 1)
                drop(t[i]&ADDR, level - 1);
    }
    page_put_l(p);
}
void vm_drop(u64 *r) {
    if (!r)
        return;
    u32 fl;
    spin_lock(&kmem_lock, &fl);
    if (r[1] & 1)
        drop(r[1]&ADDR, 3);
    page_put_l((u64)r);
    spin_unlock(&kmem_lock, fl);
}
// Coalescing first-fit kernel heap; leaf lock (never calls other subsystems).
void *kmalloc(size_t n) {
    u32 fl;
    spin_lock(&heap_lock, &fl);
    void *r = kmalloc_l(n);
    spin_unlock(&heap_lock, fl);
    return r;
}
void kfree(void *p) {
    if (!p)
        return;
    u32 fl;
    spin_lock(&heap_lock, &fl);
    kfree_l(p);
    spin_unlock(&heap_lock, fl);
}
static void *kmalloc_l(size_t n);
static void kfree_l(void *p);
typedef struct H {
    size_t n;
    struct H *next;
    int free;
    long pad;
} H;
static u8 arena[8192 * 1024] ALIGNED(16);
static H *first;
int kmalloc_owns(const void*p){return (u64)p>=(u64)arena&&(u64)p<(u64)arena+sizeof arena;}
static void *kmalloc_l(size_t n) {
    n = (n + 15) & ~15ull;
    if (!first) {
        first = (H *)arena;
        first->n = sizeof arena - sizeof(H);
        first->free = 1;
    }
    for (H *h = first; h; h = h->next)
        if (h->free && h->n >= n) {
            if (h->n > n + sizeof(H) + 16) {
                H *t = (H *)((u8 *)(h + 1) + n);
                *t = (H) {
                    h->n - n - sizeof(H), h->next, 1, 0
                };
                h->next = t;
                h->n = n;
            }
            h->free = 0;
            return h + 1;
        }
    return 0;
}
static void kfree_l(void *p) {
    if (!p)
        return;
    ((H *)p - 1)->free = 1;
    for (H *h = first; h && h->next;) {
        if (h->free && h->next->free) {
            h->n += sizeof(H) + h->next->n;
            h->next = h->next->next;
        } else
            h = h->next;
    }
}

/* Allocator snapshot is independent of scheduler lock; never acquire sched_lock
 * from here. Firmware and managed totals are fixed after boot. Cache pages are
 * included in system/reserved backing; there is no separate cache attribution. */
void mem_info(SysInfo *s)
{
    u32 fl;spin_lock(&kmem_lock,&fl);
    s->total=totalpages*PAGE;s->free=freepages*PAGE;
    s->ram_bytes=firmware_pages*PAGE;
    s->reserved_bytes=s->ram_bytes>=s->total?s->ram_bytes-s->total:0;
    s->user_bytes=userpages*PAGE;
    spin_unlock(&kmem_lock,fl);
}
static u64 resident(u64 p,int level)
{
    if(!level)return PAGE;
    u64 n=0,*t=(u64 *)p;
    for(int i=0;i<512;i++)if(t[i]&1)n+=resident(t[i]&ADDR,level-1);
    return n;
}
u64 vm_resident(u64 *root)
{
    u32 fl;spin_lock(&kmem_lock,&fl);
    u64 n=root&&(root[1]&1)?resident(root[1]&ADDR,3):0;
    spin_unlock(&kmem_lock,fl);return n;
}
