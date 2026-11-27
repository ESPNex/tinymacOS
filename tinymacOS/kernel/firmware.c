#include "k.h"
#include "firmware_boot.h"
/* No arbitrary variable access is exposed as a syscall. This service accepts
 * only the kernel-built identity of a just-committed installer destination. */
static Spin runtime_gate;
extern u64 firmware_invoke(u64,u64,u64,const void*,u64);
int firmware_install_default(const void*request){
 if(boot.firmware_magic!=FW_RUNTIME_MAGIC||!boot.firmware_default||!boot.firmware_cr3||!boot.firmware_stack||boot.firmware_cr3%4096||boot.firmware_cr3>=(1ull<<36)||boot.firmware_stack%16)return 0;
 if(cpu_id_now()!=0)return -1;
 u8 path[FW_BOOT_PATH_BYTES];int bytes=firmware_boot_path(request,path);if(bytes<0)return -1;
 u32 flags;spin_lock(&runtime_gate,&flags);
 u8 fx[512] ALIGNED(16),clean[512] ALIGNED(16);u64 cr0;
 asm volatile("mov %%cr0,%0":"=r"(cr0));
 if(cr0&12){spin_unlock(&runtime_gate,flags);return -1;}
 fx_new(clean);
 asm volatile("fxsave64 %0":"=m"(fx));
 /* EFI must not inherit user-selected exception masks/rounding or XMM data. */
 asm volatile("fxrstor64 %0"::"m"(clean));
 /* Separate retained 128KiB firmware stack; no application mapping or
  * executable permission is added to the kernel heap. */
 u64 result=firmware_invoke(boot.firmware_cr3,boot.firmware_default,boot.firmware_stack,path,bytes);
 asm volatile("fxrstor64 %0"::"m"(fx));
 spin_unlock(&runtime_gate,flags);
 logstr(result?"[firmware] install-time OpenCore preference refused/unverified\n":"[firmware] install-time OpenCore primary verified\n");
 return result?-1:1;
}
