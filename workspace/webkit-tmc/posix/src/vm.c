#include "tmc_vm.h"
#include "abi.h"
#include <errno.h>
static int error(long result)
{
    if(result>=0)return 0;
    switch(result) {
    case TMC_VM_INVALID:errno=EINVAL;break;
    case TMC_VM_NOMEM:case TMC_VM_NOT_COMMITTED:errno=ENOMEM;break;
    case TMC_VM_UNSUPPORTED:errno=ENOTSUP;break;
    case TMC_VM_BUSY:errno=EBUSY;break;
    default:errno=EIO;break;
    }
    return -1;
}
int tmc_vm_info(struct TmcVMInfo *info)
{
    if(!info){errno=EFAULT;return -1;}
    *info=(struct TmcVMInfo){0};
    if(error(tmc_syscall(S_VM,TMC_VM_QUERY,(uintptr_t)info,0,0)))return -1;
    if(info->version!=TMC_VM_VERSION || info->page_size!=TMC_VM_PAGE){errno=ENOTSUP;return -1;}
    return 0;
}
void *tmc_vm_reserve(size_t bytes,size_t alignment,unsigned guards)
{
    long result=tmc_syscall(S_VM,TMC_VM_RESERVE,bytes,alignment,guards);
    if(error(result))return 0;
    if((uint64_t)result<TMC_VM_ARENA_START || (uint64_t)result>=TMC_VM_ARENA_END ||
       ((uint64_t)result&(TMC_VM_PAGE-1))){errno=EIO;return 0;}
    return (void *)(uintptr_t)result;
}
static int operation(unsigned op,void *p,size_t bytes,unsigned protection)
{
    return error(tmc_syscall(S_VM,op,(uintptr_t)p,bytes,protection));
}
int tmc_vm_commit(void *p,size_t n,unsigned protection){return operation(TMC_VM_COMMIT,p,n,protection);}
int tmc_vm_protect(void *p,size_t n,unsigned protection){return operation(TMC_VM_PROTECT,p,n,protection);}
int tmc_vm_decommit(void *p,size_t n){return operation(TMC_VM_DECOMMIT,p,n,0);}
int tmc_vm_release(void *p,size_t n){return operation(TMC_VM_RELEASE,p,n,0);}
