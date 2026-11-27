/* TMCFS2/FV003 only. Unsupported data is never decrypted or converted. */
#include "../kernel/k.h"
#include "fs2_volume.h"
int volume_locked(void){return fs2_volume_locked(volume_current());}
int volume_cipher_io(int d,u64 l,u32 n,void*p,int write){
 if(!n)return 0;if(fs2_volume_sector_guard(d,l,n))return FV_STATE;
 return sata_io(d,l,n,p,write)?FV_IO:write?sata_flush(d):0;
}
int filevault_control(int op,VaultRequest*q,VaultInfo*out){
 u32 flags;rec_lock(&fs_gate,&flags);int result=FV_STATE,d=q->disk<0?volume_current():q->disk;
 if(!fs2_volume_control(op,d,q,out,&result))*out=(VaultInfo){.error=FV_STATE};
 rec_unlock(&fs_gate,flags);return result;
}
void volume_forget(int d){fs2_volume_forget(d);}
