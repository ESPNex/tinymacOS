/* Firmware-only Recovery keyboard.
 * USB uses OpenUsbKbDxe's Apple keymap, with OpenCore KeySupport OFF.
 * PS/2 uses the explicitly identified derived Ps2KeyboardDxe raw protocol.
 * No native driver, i8042 port access, UsbIo, or direct HID transaction here.
 */
#include "screen.h"
#include "firmware_keyboard_decode.h"
#include "../vendor/tmc-ps2-keymap/TmcRawPs2.h"
typedef struct RecKeymap RecKeymap;
struct RecKeymap {
    UINT64 revision;
    EFI_STATUS (EFIAPI *get)(RecKeymap *,UINT16 *,UINTN *,UINT16 *);
    VOID *contains;
};
static RecKeymap *keymap;
static TMC_RAW_PS2_PROTOCOL *raw[4];
static RecFirmwareSet1 ps2[4];
static unsigned nraw,caps,head,tail;
static int pending[128];
static u8 usbheld[256];
static unsigned repeat_key,repeat_mods;
static u64 ticks,repeat_at;
static void emit(unsigned usage,unsigned mods)
{
    if(usage==57){caps=!caps;return;}
    int c=usage==82?0x101:usage==81?0x102:it_character(usage,mods,caps);
    unsigned next=(head+1)%128;
    if(c&&next!=tail){pending[head]=c;head=next;}
}
static int ps2_path(EFI_BOOT_SERVICES *bs,EFI_HANDLE h)
{
    EFI_GUID guid=DEVICE_PATH_PROTOCOL;u8 *p=0;
    if(EFI_ERROR(bs->HandleProtocol(h,&guid,(void **)&p)))return 0;
    for(unsigned at=0;at<2048;){
        unsigned len=p[at+2]|p[at+3]<<8;
        if(len<4||len>2048-at||p[at]==127)break;
        if(p[at]==2&&p[at+1]==1&&len>=12){
            u32 hid;memcpy(&hid,p+at+4,4);if(hid==0x030341d0)return 1;
        }
        at+=len;
    }
    return 0;
}
static void rebind_ps2(EFI_BOOT_SERVICES *bs)
{
    EFI_GUID guid=EFI_DRIVER_BINDING_PROTOCOL_GUID,dp=DEVICE_PATH_PROTOCOL;
    EFI_GUID rawguid=TMC_RAW_PS2_PROTOCOL_GUID;
    EFI_HANDLE *handles=0,*paths=0,drivers[2]={0,0};UINTN count=0,np=0;
    if(!EFI_ERROR(bs->LocateHandleBuffer(ByProtocol,&guid,0,&count,&handles))){
        for(UINTN i=0;i<count;i++){
            EFI_DRIVER_BINDING_PROTOCOL *binding=0;
            if(!EFI_ERROR(bs->HandleProtocol(handles[i],&guid,(void **)&binding))&&binding&&binding->Version==TMC_RAW_PS2_DRIVER_VERSION){drivers[0]=handles[i];break;}
        }
        bs->FreePool(handles);
    }
    if(!drivers[0])return;
    if(EFI_ERROR(bs->LocateHandleBuffer(ByProtocol,&dp,0,&np,&paths)))return;
    for(UINTN i=0;i<np;i++){
        TMC_RAW_PS2_PROTOCOL *r=0;
        if(!ps2_path(bs,paths[i])||!EFI_ERROR(bs->HandleProtocol(paths[i],&rawguid,(void **)&r)))continue;
        /* This is a PNP0303 child only, never its host bus or mouse/storage.
         * No direct hardware takeover: reconnect it to the firmware binding.
         * On failure let firmware reconnect its previous driver normally.
         */
        if(EFI_ERROR(bs->DisconnectController(paths[i],0,0)))continue;
        if(EFI_ERROR(bs->ConnectController(paths[i],drivers,0,FALSE)))
            (void)bs->ConnectController(paths[i],0,0,FALSE);
    }
    bs->FreePool(paths);
}
int rec_keyboard_start(EFI_BOOT_SERVICES *bs)
{
    EFI_GUID apple={0x5b213447,0x6e73,0x4901,{0xa4,0xf1,0xb8,0x64,0xf3,0xb7,0xa1,0x72}};
    EFI_GUID rg=TMC_RAW_PS2_PROTOCOL_GUID;
    EFI_HANDLE *handles=0;UINTN count=0;
    (void)bs->LocateProtocol(&apple,0,(void **)&keymap);
    rebind_ps2(bs);
    if(!EFI_ERROR(bs->LocateHandleBuffer(ByProtocol,&rg,0,&count,&handles))){
        for(UINTN i=0;i<count&&nraw<4;i++){
            TMC_RAW_PS2_PROTOCOL *r=0;
            if(EFI_ERROR(bs->HandleProtocol(handles[i],&rg,(void **)&r))||!r||r->Revision!=TMC_RAW_PS2_PROTOCOL_REVISION||!r->Read||!r->Reset)continue;
            if(EFI_ERROR(r->Reset(r)))continue;
            rec_fw_set1_reset(&ps2[nraw]);raw[nraw++]=r;
        }
        bs->FreePool(handles);
    }
    return nraw+(keymap!=0);
}
int rec_keyboard_next(EFI_BOOT_SERVICES *bs,int *code)
{
    (void)bs;ticks++;
    for(unsigned i=0;i<nraw;i++)for(unsigned j=0;j<256;j++){
        UINT8 b;EFI_STATUS status=raw[i]->Read(raw[i],&b);
        if(EFI_ERROR(status)){
            if(status!=EFI_NOT_READY)rec_fw_set1_reset(&ps2[i]);
            break;
        }
        unsigned usage=rec_fw_set1(&ps2[i],b);
        if(usage)emit(usage,ps2[i].mods);
    }
    if(keymap){
        UINT16 keys[256],mods=0;UINTN n=256;u8 held[256]={0};
        EFI_STATUS status=keymap->get(keymap,&mods,&n,keys);
        if(!EFI_ERROR(status)&&n<=256){
            for(UINTN i=0;i<n;i++)if((keys[i]>>12)==7&&(keys[i]&0xfff)<256){
                unsigned k=keys[i]&0xfff;
                if(k<=3||held[k])continue;
                held[k]=1;
                if(!usbheld[k]){
                    emit(k,mods);
                    if(k!=57){repeat_key=k;repeat_at=ticks+42;}
                }
            }
            if(repeat_key&&!held[repeat_key])repeat_key=0;
            repeat_mods=mods;
            memcpy(usbheld,held,256);
            if(repeat_key&&ticks>=repeat_at){emit(repeat_key,repeat_mods);repeat_at=ticks+4;}
        }else{
            /* Do not perpetuate a held key across a failed/disconnected map. */
            repeat_key=0;memset(usbheld,0,sizeof(usbheld));
        }
    }
    if(head==tail)return 0;
    *code=pending[tail];tail=(tail+1)%128;return 1;
}
