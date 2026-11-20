#include "k.h"
/* Reservation/commit layer for interpreter-mode runtime work. The virtual arena
 * never overlaps legacy S_ALLOC, loaded images or any task's stack. Mutations
 * are intentionally single-online-CPU only, NOT a claim of SMP TLB safety. */
static Spin uservm_lock;
void uservm_info(struct TmcVMInfo *i)
{
    *i=(struct TmcVMInfo){TMC_VM_VERSION,TMC_VM_PAGE,
        TMC_VM_SINGLE_CPU_ONLY|TMC_VM_NO_EXEC|(ncpu_online==1?TMC_VM_AVAILABLE:0),
        TMC_VM_SLOTS,TMC_VM_ARENA_START,TMC_VM_ARENA_END,TMC_VM_MAX_REGION,TMC_VM_MAX_ALIGNMENT};
}
static int valid_size(u64 n)
{
    return n && n<=TMC_VM_MAX_REGION && !(n&(PAGE-1));
}
static int permitted(u64 protection)
{
    return protection==0 || protection==TMC_VM_READ || protection==(TMC_VM_READ|TMC_VM_WRITE);
}
static i64 reserve(Process *p,u64 bytes,u64 alignment,u64 guard_pages)
{
    if(!valid_size(bytes) || alignment<PAGE || alignment>TMC_VM_MAX_ALIGNMENT ||
       (alignment&(alignment-1)) || guard_pages>TMC_VM_MAX_GUARD_PAGES)return TMC_VM_INVALID;
    int slot=-1;
    for(int i=0;i<TMC_VM_SLOTS;i++)if(!p->vm[i].bytes){slot=i;break;}
    if(slot<0)return TMC_VM_NOMEM;
    u64 guard=guard_pages*PAGE,pos=TMC_VM_ARENA_START;
    for(;;) {
        u64 address=(pos+guard+alignment-1)&~(alignment-1);
        u64 begin=address-guard,end=address+bytes+guard;
        if(end>TMC_VM_ARENA_END)return TMC_VM_NOMEM;
        int overlap=0;
        for(int i=0;i<TMC_VM_SLOTS;i++)if(p->vm[i].bytes) {
            u64 lo=p->vm[i].address-p->vm[i].guard;
            u64 hi=p->vm[i].address+p->vm[i].bytes+p->vm[i].guard;
            if(begin<hi && end>lo){pos=hi;overlap=1;break;}
        }
        if(overlap)continue;
        p->vm[slot].address=address;p->vm[slot].bytes=bytes;p->vm[slot].guard=guard;
        return (i64)address; /* no physical pages allocated at reservation */
    }
}
static i64 operate(Process *p,u64 op,u64 address,u64 bytes,u64 protection)
{
    if(op==TMC_VM_RESERVE)return reserve(p,address,bytes,protection);
    if(op<TMC_VM_COMMIT || op>TMC_VM_RELEASE || !valid_size(bytes) ||
       address<TMC_VM_ARENA_START || (address&(PAGE-1)) || address>TMC_VM_ARENA_END-bytes)
        return TMC_VM_INVALID;
    if((op==TMC_VM_DECOMMIT || op==TMC_VM_RELEASE) && protection)return TMC_VM_INVALID;
    if((op==TMC_VM_COMMIT || op==TMC_VM_PROTECT) && !permitted(protection))return TMC_VM_UNSUPPORTED;
    int slot=-1;
    for(int i=0;i<TMC_VM_SLOTS;i++)if(p->vm[i].bytes && address>=p->vm[i].address &&
       address+bytes<=p->vm[i].address+p->vm[i].bytes){slot=i;break;}
    if(slot<0)return TMC_VM_INVALID; /* guards and partial/outside ranges rejected */
    if(op==TMC_VM_RELEASE && (address!=p->vm[slot].address || bytes!=p->vm[slot].bytes))return TMC_VM_INVALID;
    /* Validate the entire range BEFORE mutation: no partial permission change. */
    if(op==TMC_VM_COMMIT || op==TMC_VM_PROTECT)for(u64 a=address;a<address+bytes;a+=PAGE) {
        u64 *e=vm_pte(p->root,a,0);int present=e&&(*e&1);
        if(op==TMC_VM_COMMIT && present)return TMC_VM_BUSY;
        if(op==TMC_VM_PROTECT && !present)return TMC_VM_NOT_COMMITTED;
    }
    if(op==TMC_VM_COMMIT) {
        u64 at=address;
        for(;at<address+bytes;at+=PAGE) {
            if(!vm_add(p->root,at,NX|2))break; /* page_get zeroes before exposure */
            if(!vm_access(p->root,at,protection))break;
        }
        if(at<address+bytes) {
            /* Also unmap the current page if access unexpectedly failed. */
            for(u64 a=address;a<=at;a+=PAGE)vm_unmap(p->root,a);
            return TMC_VM_NOMEM;
        }
        return 0;
    }
    if(op==TMC_VM_PROTECT) {
        for(u64 a=address;a<address+bytes;a+=PAGE)vm_access(p->root,a,protection);
        return 0;
    }
    for(u64 a=address;a<address+bytes;a+=PAGE)vm_unmap(p->root,a);
    if(op==TMC_VM_RELEASE)p->vm[slot].bytes=0;
    return 0;
}
i64 uservm_operation(Process *p,u64 op,u64 address,u64 bytes,u64 protection)
{
    if(ncpu_online!=1)return TMC_VM_UNSUPPORTED;
    u32 fl;spin_lock(&uservm_lock,&fl);
    i64 result=operate(p,op,address,bytes,protection);
    spin_unlock(&uservm_lock,fl);
    return result;
}
