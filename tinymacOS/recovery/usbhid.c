/* Recovery USB pointer ownership: detach only a Boot-mouse interface child,
 * never its host, hub, keyboard or boot storage. Non-Boot unclaimed HID uses
 * descriptor parsing. Error recovery runs at application TPL, not callbacks. */
#ifdef TMC_REC_USB_TEST
#include "../tests/recovery_usb_support.h"
#else
#include "screen.h"
#endif
#include "hidtablet.h"

typedef struct PACK {
    u8 rt, req;
    u16 value, index, len;
} UsbRequest;
typedef struct PACK {
    u8 len, kind, number, alt, eps, cls, sub, proto, str;
} UsbInterface;
typedef struct PACK {
    u8 len, kind, addr, attr;
    u16 packet;
    u8 interval;
} UsbEndpoint;
typedef struct EfiUsb EfiUsb;
typedef EFI_STATUS (EFIAPI *Notify)(VOID *, UINTN, VOID *, UINT32);
/* UEFI specification, EFI_USB_IO_PROTOCOL (prefix through GetEndpointDescriptor). */
struct EfiUsb {
    EFI_STATUS (EFIAPI *control)(EfiUsb *, UsbRequest *, int, UINT32, VOID *, UINTN, UINT32 *);
    VOID *bulk;
    EFI_STATUS (EFIAPI *async)(EfiUsb *, UINT8, BOOLEAN, UINTN, UINTN, Notify, VOID *);
    VOID *sync, *iso, *async_iso, *device_desc, *config_desc;
    EFI_STATUS (EFIAPI *interface_desc)(EfiUsb *, UsbInterface *);
    EFI_STATUS (EFIAPI *endpoint_desc)(EfiUsb *, UINT8, UsbEndpoint *);
};
static TabletLayout layout;
static TabletState queue[32];
static volatile unsigned head, tail;
static int relative, rx, ry, last_buttons;
static EfiUsb *active_io;
static int submitted;
static unsigned active_ep,active_packet,active_interval,cooldown,consecutive_errors;
static volatile UINT32 pending_error;
int rec_pointer_relative(void){return relative || layout.relative;}
static EFI_STATUS EFIAPI received(VOID *data, UINTN n, VOID *unused, UINT32 error)
{
    (void)unused;
    TabletState s;int oldx=rx,oldy=ry;
    if(error){pending_error=error;cooldown=8;return EFI_SUCCESS;}
    if (!data || n > 64) return EFI_SUCCESS;
    consecutive_errors=0;
    if (relative) {
        if (n < 3) return EFI_SUCCESS;
        if(!pointer_boot_read(&layout,data,n,rw,rh,&s))return EFI_SUCCESS;
    } else {
        if (!pointer_read(&layout, data, n, rw, rh, &s)) return EFI_SUCCESS;
    }
    /* Idle/duplicate reports are not activity and must not starve firmware
     * SimplePointer / AbsolutePointer fallbacks in the main loop. */
    int rel=rec_pointer_relative();
    if((rel ? !s.x&&!s.y : s.x==oldx&&s.y==oldy) && s.buttons==last_buttons && !s.wheel)return EFI_SUCCESS;
    if(!rel){rx=s.x;ry=s.y;}
    last_buttons=s.buttons;
    unsigned h = head, last = (h + 31) % 32;
    if (h != tail && s.buttons == queue[last].buttons) {
        if(rel){
            s.x=MAX(-1048576,MIN(1048576,(i64)queue[last].x+s.x));
            s.y=MAX(-1048576,MIN(1048576,(i64)queue[last].y+s.y));
            s.wheel=MAX(-1048576,MIN(1048576,(i64)queue[last].wheel+s.wheel));
        }
        queue[last]=s;
    }
    else {
        unsigned next = (h + 1) % 32;
        if (next == tail) tail = (tail + 1) % 32;
        queue[h] = s;
        head = next;
    }
    return EFI_SUCCESS;
}
static int get_desc(EfiUsb *io, u8 rt, u16 type, u16 iface, void *dst, u16 n)
{
    UsbRequest q = {rt, 6, type, iface, n};
    UINT32 err = 0;
    memset(dst, 0, n);
    return !EFI_ERROR(io->control(io, &q, 0, 250, dst, n, &err)) && !err;
}
/* Check readback even after an acknowledged SET. Unsupported GET may use a
 * successful SET, but an explicit different/short mode must never be ignored. */
static int protocol(EfiUsb *io, unsigned iface, unsigned expected)
{
    UsbRequest req={0x21,11,expected,iface,0};UINT32 err=0;
    int set=!EFI_ERROR(io->control(io,&req,2,250,0,0,&err))&&!err;
    u8 current=255;req=(UsbRequest){0xa1,3,0,iface,1};err=0;
    int get=!EFI_ERROR(io->control(io,&req,0,250,&current,1,&err))&&!err;
    return get ? current==expected : set;
}
static int probe(EfiUsb *io)
{
    UsbInterface f;
    if(EFI_ERROR(io->interface_desc(io,&f))||f.cls!=3||f.alt||!f.eps||f.eps>16||f.proto==1)return 0;
    int boot=f.sub==1&&f.proto==2;u8 desc[1024];unsigned wanted=0;
    if(get_desc(io,0x80,0x200,0,desc,9)){
        unsigned len=desc[2]|desc[3]<<8;
        if(len>=9&&len<=sizeof desc&&get_desc(io,0x80,0x200,0,desc,len)){
            int matching=0;
            for(unsigned at=0;at+2<=len;){const u8 *p=desc+at;unsigned z=p[0];
                if(z<2||z>len-at)return 0;
                if(p[1]==4)matching=z>=9&&p[2]==f.number&&!p[3];
                if(matching&&p[1]==0x21&&z>=6){
                    if(p[5]>(z-6)/3)return 0;
                    for(unsigned j=0;j<p[5];j++)if(p[6+3*j]==0x22)wanted=p[7+3*j]|p[8+3*j]<<8;
                }at+=z;
            }
        }
    }
    int report=wanted&&wanted<=sizeof desc&&get_desc(io,0x81,0x2200,f.number,desc,wanted);
    for(unsigned j=0;j<f.eps;j++){
        UsbEndpoint e;
        if(EFI_ERROR(io->endpoint_desc(io,j,&e))||(e.attr&3)!=3||!(e.addr&128)||!(e.addr&15))continue;
        unsigned packet=e.packet&2047;if(packet<3||packet>64)continue;
        relative=0;memset(&layout,0,sizeof layout);
        /* A Boot label is not a packet layout. Use the descriptor like
         * Linux usbhid; fallback only after explicit Boot negotiation. Keep
         * the parsed layout for the Report-ID framing guard even in Boot. */
        int parsed=report&&pointer_layout(desc,wanted,packet,&layout);
        if(parsed && (!boot || protocol(io,f.number,1)))relative=0;
        else {if(!boot||!protocol(io,f.number,0))continue;relative=1;}
        unsigned interval=MAX(1,MIN(255,e.interval));
        if(!EFI_ERROR(io->async(io,e.addr,TRUE,interval,packet,received,0))){
            active_io=io;submitted=1;active_ep=e.addr;active_packet=packet;active_interval=interval;return 1;
        }
    }
    return 0;
}
int rec_pointer_start(EFI_BOOT_SERVICES *bs, int x, int y)
{
    rx = x;
    ry = y;head=tail=0;last_buttons=0;active_io=0;submitted=0;pending_error=cooldown=consecutive_errors=0;
    EFI_GUID guid = {0x2b2f68d6, 0x0cd2, 0x44cf, {0x8e, 0x8b, 0xbb, 0xa2, 0x0b, 0x1b, 0x5b, 0x75}};
    UINTN n;
    EFI_HANDLE *handles;
    if (EFI_ERROR(bs->LocateHandleBuffer(ByProtocol, &guid, 0, &n, &handles))) return 0;
    int active = 0;
    for (UINTN i = 0; i < n && !active; i++) {
        EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *users = 0;
        UINTN count = 0;
        if (EFI_ERROR(bs->OpenProtocolInformation(handles[i], &guid, &users, &count))) continue;
        int claimed = 0;
        for (UINTN j = 0; j < count;
                j++) if (users[j].Attributes & (EFI_OPEN_PROTOCOL_BY_DRIVER | EFI_OPEN_PROTOCOL_EXCLUSIVE)) claimed = 1;
        EfiUsb *io=0;UsbInterface f;int detached=0;
        if(!EFI_ERROR(bs->HandleProtocol(handles[i],&guid,(void **)&io)) && io &&
           !EFI_ERROR(io->interface_desc(io,&f)) && f.cls==3 && !f.alt && f.sub==1 && f.proto==2 && claimed){
            int ok=1,drivers=0;
            for(UINTN j=0;j<count;j++)if(users[j].Attributes&EFI_OPEN_PROTOCOL_BY_DRIVER){
                drivers++;if(EFI_ERROR(bs->DisconnectController(handles[i],users[j].AgentHandle,0)))ok=0;
            }
            if(ok&&drivers){claimed=0;detached=1;}
        }
        if (users) bs->FreePool(users);
        if (!claimed && !EFI_ERROR(bs->HandleProtocol(handles[i], &guid, (void **)&io)) && io) active = probe(io);
        if(detached&&!active)bs->ConnectController(handles[i],0,0,FALSE);
    }
    bs->FreePool(handles);
    return active;
}
int rec_pointer_next(EFI_BOOT_SERVICES *bs, TabletState *s)
{
    /* A single async USB error used to leave the endpoint dead forever.
     * Quiesce, clear an actual STALL, and resubmit without touching host/hub.
     * Limit work per poll; repeated failures back off instead of an IRQ loop. */
    if(active_io && pending_error && (!cooldown || !--cooldown)){
        UINT32 error=pending_error;EFI_STATUS status=submitted?active_io->async(active_io,active_ep,FALSE,0,0,0,0):EFI_SUCCESS;
        if(!EFI_ERROR(status)){
            submitted=0;
            if(error&2){UsbRequest req={2,1,0,active_ep,0};UINT32 err=0;
                status=active_io->control(active_io,&req,2,250,0,0,&err);
                if(err)status=1;
            }
            if(!EFI_ERROR(status)){
                pending_error=0;
                status=active_io->async(active_io,active_ep,TRUE,active_interval,active_packet,received,0);
                if(!EFI_ERROR(status))submitted=1;
            }
        }
        if(EFI_ERROR(status))pending_error=error;
        if(pending_error)cooldown=++consecutive_errors<3?8:85;
    }
    EFI_TPL old = bs->RaiseTPL(TPL_NOTIFY);
    int ready = head != tail;
    if (ready) {
        *s = queue[tail];
        tail = (tail + 1) % 32;
    }
    bs->RestoreTPL(old);
    return ready;
}
