/* Executes the shipping EFI variable algorithm with a deterministic store.
 * These are firmware API tests, not physical-firmware or Windows boot proof. */
#include <efi.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "base.h"
#include "../boot/bootdefault.h"
static struct {CHAR16 name[64];EFI_GUID guid;u8 data[2048];UINTN bytes;UINT32 attrs;int used;} vars[16];
static int writes,deny_backup,deny_order;
static int nameeq(const CHAR16*a,const CHAR16*b){while(*a&&*a==*b)a++,b++;return *a==*b;}
static int indexof(CHAR16*n,EFI_GUID*g){for(int i=0;i<16;i++)if(vars[i].used&&nameeq(n,vars[i].name)&&!memcmp(g,&vars[i].guid,16))return i;return -1;}
static EFI_STATUS EFIAPI get(CHAR16*n,EFI_GUID*g,UINT32*a,UINTN*size,void*data){
 int i=indexof(n,g);if(i<0)return EFI_NOT_FOUND;
 if(*size<vars[i].bytes||!data){*size=vars[i].bytes;return EFI_BUFFER_TOO_SMALL;}
 *size=vars[i].bytes;if(a)*a=vars[i].attrs;memcpy(data,vars[i].data,*size);return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI set(CHAR16*n,EFI_GUID*g,UINT32 a,UINTN size,void*data){
 if((deny_backup&&nameeq(n,L"tinymacOS-previousBootOrder"))||(deny_order&&nameeq(n,L"BootOrder")))return EFI_WRITE_PROTECTED;
 writes++;int i=indexof(n,g);
 if(!size){if(i>=0)vars[i].used=0;return EFI_SUCCESS;}
 if(i<0)for(int j=0;j<16;j++)if(!vars[j].used){i=j;break;}
 assert(i>=0&&size<=2048);size_t j=0;do{vars[i].name[j]=n[j];}while(n[j++]&&j<64);
 vars[i].used=1;vars[i].guid=*g;vars[i].attrs=a;vars[i].bytes=size;memcpy(vars[i].data,data,size);return EFI_SUCCESS;
}
static EFI_RUNTIME_SERVICES rs={.GetVariable=get,.SetVariable=set};
static EFI_GUID global=EFI_GLOBAL_VARIABLE,vendor=TMC_BOOT_VENDOR;
static u8 path[]={4,4,10,0,'a',0,0,0,0,0,0x7f,0xff,4,0};
static void reset(void){memset(vars,0,sizeof vars);writes=deny_backup=deny_order=0;u16 order[]={9,10};u8 ms[]={0x57,0x49,0x4e,0x44,0x4f,0x57,0x53};set(L"BootOrder",&global,7,sizeof order,order);set(L"Boot0009",&global,7,sizeof ms,ms);set(L"Boot000A",&global,7,sizeof ms,ms);writes=0;}
static void original(void){int i=indexof(L"Boot0009",&global);assert(i>=0&&vars[i].bytes==7&&!memcmp(vars[i].data,"WINDOWS",7));i=indexof(L"Boot000A",&global);assert(i>=0&&vars[i].bytes==7&&!memcmp(vars[i].data,"WINDOWS",7));}
static void order_is(u16 a,u16 b,u16 c){int i=indexof(L"BootOrder",&global);assert(i>=0);u16*p=(void*)vars[i].data;assert(p[0]==a&&p[1]==b);if(c!=65535)assert(vars[i].bytes==6&&p[2]==c);else assert(vars[i].bytes==4);}
int main(void){
 reset();assert(!EFI_ERROR(boot_set_default(&rs,path,sizeof path)));original();order_is(0,9,10);int i=indexof(L"tinymacOS-previousBootOrder",&vendor);assert(i>=0&&vars[i].bytes==12);u16*p=(void*)(vars[i].data+8);assert(p[0]==9&&p[1]==10);
 puts("PASS dedicated OpenCore Boot0000 comes first; Microsoft/fallback variables and original BootOrder backup are byte-identical");
 int before=writes;assert(!EFI_ERROR(boot_set_default(&rs,path,sizeof path)));assert(writes==before);original();puts("PASS repeated registration is idempotent and does not rewrite NVRAM");
 reset();deny_backup=1;assert(EFI_ERROR(boot_set_default(&rs,path,sizeof path)));assert(writes==0);original();order_is(9,10,65535);puts("PASS backup denial prevents both entry creation and BootOrder changes");
 reset();deny_order=1;assert(EFI_ERROR(boot_set_default(&rs,path,sizeof path)));original();order_is(9,10,65535);assert(indexof(L"Boot0000",&global)<0);puts("PASS BootOrder failure removes only the newly created native entry, preserving Microsoft");
 reset();assert(EFI_ERROR(boot_set_default(&rs,path,0)));assert(!writes);puts("PASS invalid load-option path is rejected without writes");return 0;
}
