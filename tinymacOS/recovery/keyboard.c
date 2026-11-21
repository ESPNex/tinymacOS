/* Recovery owns physical keyboard decoding too: firmware UnicodeChar is not
 * a physical key and depends on an unknowable firmware layout. Detach only
 * keyboard child drivers, not the host controller, mouse or boot storage.
 * Boot HID and i8042 use the same Italian map/PS2 decoder as the native OS. */
#include "screen.h"
#include "keyboard_it.h"
#define TMC_PS2_RECOVERY 1
#include "../drivers/ps2.c"
typedef struct PACK {u8 rt,request;u16 value,index,length;} KRequest;
typedef struct PACK {u8 len,type,number,alt,eps,cls,sub,proto,string;} KInterface;
typedef struct PACK {u8 len,type,address,attr;u16 packet;u8 interval;} KEndpoint;
typedef struct KUsb KUsb;
typedef EFI_STATUS (EFIAPI *KNotify)(VOID *,UINTN,VOID *,UINT32);
struct KUsb {
    EFI_STATUS (EFIAPI *control)(KUsb *,KRequest *,int,UINT32,VOID *,UINTN,UINT32 *);
    VOID *bulk;
    EFI_STATUS (EFIAPI *async)(KUsb *,UINT8,BOOLEAN,UINTN,UINTN,KNotify,VOID *);
    VOID *sync,*iso,*async_iso,*device_desc,*config_desc;
    EFI_STATUS (EFIAPI *interface_desc)(KUsb *,KInterface *);
    EFI_STATUS (EFIAPI *endpoint_desc)(KUsb *,UINT8,KEndpoint *);
};
static int codes[128],kh,kt,kcaps,ps2active;
static u8 previous[4][8],led_state[4]={255,255,255,255};
static KUsb *keyboards[4];static unsigned interfaces[4],nkeyboards;
static int repeat_key,repeat_mods;static u64 repeat_at;
static void input_key_usage(int usage,int modifiers)
{
    if(usage==57){kcaps=!kcaps;ps2_setleds(kcaps);return;}
    int c=usage==82?0x101:usage==81?0x102:it_character(usage,modifiers,kcaps);
    if(c){int next=(kh+1)%128;if(next!=kt){codes[kh]=c;kh=next;}}
}
static EFI_STATUS EFIAPI keyboard_report(VOID *data,UINTN n,VOID *context,UINT32 error)
{
    if(error||!data||n<8)return EFI_SUCCESS;
    const u8 *r=data;u8 *old=context;
    for(int i=2;i<8;i++)if(r[i]>=1&&r[i]<=3)return EFI_SUCCESS;
    for(int i=2;i<8;i++)if(r[i]){int seen=0;for(int j=2;j<8;j++)if(old[j]==r[i])seen=1;
        if(!seen)input_key_usage(r[i],r[0]);}
    if(r[2]!=repeat_key){repeat_key=r[2]==57?0:r[2];repeat_at=ticks+50;}
    repeat_mods=r[0];
    memcpy(old,r,8);return EFI_SUCCESS;
}
static int detach_keyboard(EFI_BOOT_SERVICES *bs,EFI_HANDLE h,EFI_GUID *protocol)
{
    EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *info=0;UINTN n=0;
    if(EFI_ERROR(bs->OpenProtocolInformation(h,protocol,&info,&n)))return 0;
    int ok=1;
    for(UINTN i=0;i<n;i++)if(info[i].Attributes&EFI_OPEN_PROTOCOL_BY_DRIVER)
        if(EFI_ERROR(bs->DisconnectController(h,info[i].AgentHandle,0)))ok=0;
    if(info)bs->FreePool(info);return ok;
}
int rec_keyboard_start(EFI_BOOT_SERVICES *bs)
{
    EFI_GUID usb={0x2b2f68d6,0x0cd2,0x44cf,{0x8e,0x8b,0xbb,0xa2,0x0b,0x1b,0x5b,0x75}};
    EFI_GUID dp=DEVICE_PATH_PROTOCOL;EFI_HANDLE *handles=0;UINTN n=0;int count=0;
    if(!EFI_ERROR(bs->LocateHandleBuffer(ByProtocol,&usb,0,&n,&handles))){
        for(UINTN i=0;i<n&&count<4;i++){
            KUsb *io=0;KInterface f;
            if(EFI_ERROR(bs->HandleProtocol(handles[i],&usb,(void **)&io))||!io||
               EFI_ERROR(io->interface_desc(io,&f))||f.cls!=3||f.sub!=1||f.proto!=1||f.alt||f.eps>16)continue;
            if(!detach_keyboard(bs,handles[i],&usb))continue;
            if(EFI_ERROR(bs->HandleProtocol(handles[i],&usb,(void **)&io)))continue;
            KRequest req={0x21,11,0,f.number,0};UINT32 err=0;
            if(EFI_ERROR(io->control(io,&req,2,250,0,0,&err))||err)continue;
            /* HID SET_IDLE duration is in 4 ms units. Refresh the full Boot
             * report every 32 ms, including key releases: change-only reports
             * can otherwise leave a stale state after firmware handoff.
             * Duplicate reports are filtered below; repeat remains local.
             * Optional on devices that STALL SET_IDLE (keep change reports). */
            req.request=10;req.value=8u<<8;
            (void)io->control(io,&req,2,250,0,0,&err);
            for(unsigned j=0;j<f.eps;j++){
                KEndpoint e;if(EFI_ERROR(io->endpoint_desc(io,j,&e))||(e.attr&3)!=3||!(e.address&128)||e.packet<8||e.packet>64)continue;
                if(!EFI_ERROR(io->async(io,e.address,TRUE,8,e.packet,keyboard_report,previous[count]))){keyboards[count]=io;interfaces[count]=f.number;count++;nkeyboards=count;break;}
            }
        }
        bs->FreePool(handles);
    }
    /* ACPI PNP0303 is a keyboard child. Never detach PNP0F13 (PS/2 mouse),
     * never reset i8042, and preserve the auxiliary clock/IRQ configuration. */
    if(!EFI_ERROR(bs->LocateHandleBuffer(ByProtocol,&dp,0,&n,&handles))){
        for(UINTN i=0;i<n&&!ps2active;i++){
            u8 *p=0;if(EFI_ERROR(bs->HandleProtocol(handles[i],&dp,(void **)&p)))continue;
            int keyboard=0;
            for(unsigned at=0;at<2048;){unsigned z=p[at+2]|p[at+3]<<8;if(z<4||z>2048-at||p[at]==127)break;
                if(p[at]==2&&p[at+1]==1&&z>=12){u32 hid;memcpy(&hid,p+at+4,4);if(hid==0x030341d0)keyboard=1;}at+=z;}
            if(!keyboard)continue;
            /* The ISA I/O driver owns this handle; disconnect only this child. */
            if(EFI_ERROR(bs->DisconnectController(handles[i],0,0)))continue;
            if(inb(0x64)==255||!cmd(0x20))continue;
            int cfg=receive(0);if(cfg<0)continue;cfg=(cfg|64)&~17;
            if(!cmd(0x60)||!data(cfg)||!cmd(0xae))continue;
            if(device(0,0xf5)&&device(0,0xf0)&&device(0,2)&&device(0,0xf4))ps2active=keyboard_on=1;
        }
        bs->FreePool(handles);
    }
    return count+ps2active;
}
int rec_keyboard_next(EFI_BOOT_SERVICES *bs,int *code)
{
    ticks++;
    if(ps2active)for(int i=0;i<64;i++){
        u8 status=inb(0x64);if(status==255||!(status&1)||(status&32))break;
        u8 b=inb(0x60);if(!(status&0xc0))keyboard_byte(b);
    }
    if(ps2active)keyboard_led_poll();
    if(repeat_key&&ticks>=repeat_at){input_key_usage(repeat_key,repeat_mods);repeat_at=ticks+5;}
    for(unsigned i=0;i<nkeyboards;i++)if(led_state[i]!=kcaps){
        u8 leds=kcaps?2:0;KRequest req={0x21,9,0x0200,interfaces[i],1};UINT32 err=0;
        (void)keyboards[i]->control(keyboards[i],&req,1,50,&leds,1,&err);led_state[i]=kcaps;
    }
    EFI_TPL old=bs->RaiseTPL(TPL_NOTIFY);int ready=kh!=kt;
    if(ready){*code=codes[kt];kt=(kt+1)%128;}
    bs->RestoreTPL(old);return ready;
}
