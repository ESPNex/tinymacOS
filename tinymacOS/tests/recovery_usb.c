#define TMC_REC_USB_TEST 1
#include "../recovery/usbhid.c"
int rw=1280,rh=800;
static int state=1,deny_set,short_reply,bad_report,failures,armed;
static const u8 descriptor[]={5,1,9,2,0xa1,1,9,1,0xa1,0,5,9,0x19,1,0x29,3,0x15,0,0x25,1,0x75,1,0x95,3,0x81,2,0x75,5,0x95,1,0x81,1,5,1,9,0x30,9,0x31,0x15,0x81,0x25,0x7f,0x75,8,0x95,2,0x81,6,0xc0,0xc0};
static EFI_STATUS ctl(EfiUsb *io,UsbRequest *q,int dir,UINT32 timeout,VOID *dst,UINTN n,UINT32 *err){
 (void)io;(void)dir;assert(timeout==250);*err=0;
 if(q->req==6&&q->value==0x200){const u8 c[]={9,2,34,0,1,1,0,128,50,9,4,0,0,1,3,1,2,0,9,0x21,0x11,1,0,1,0x22,sizeof descriptor,0,7,5,0x81,3,8,0,10};memcpy(dst,c,n);return 0;}
 if(q->req==6&&q->value==0x2200){memcpy(dst,descriptor,n);if(bad_report)memset(dst,0,n);return 0;}
 if(q->req==11){if(deny_set){failures++;return 1;}state=q->value;return 0;}
 if(q->req==3){if(!short_reply)*(u8*)dst=state;return 0;}
 assert(0);return 1;
}
static EFI_STATUS iface(EfiUsb *io,UsbInterface *f){(void)io;*f=(UsbInterface){9,4,0,0,1,3,1,2,0};return 0;}
static EFI_STATUS endpoint(EfiUsb *io,UINT8 n,UsbEndpoint *e){(void)io;assert(!n);*e=(UsbEndpoint){7,5,0x81,3,8,10};return 0;}
static EFI_STATUS start(EfiUsb *io,UINT8 ep,BOOLEAN add,UINTN interval,UINTN len,Notify cb,VOID *arg){(void)io;(void)arg;assert(ep==0x81&&add&&interval==8&&len==8&&cb==received);armed++;return 0;}
int main(void){
 EfiUsb io={.control=ctl,.async=start,.interface_desc=iface,.endpoint_desc=endpoint};
 assert(probe(&io)&&!relative);deny_set=1;state=1;assert(probe(&io)&&!relative);
 state=0;assert(probe(&io)&&relative);bad_report=1;assert(probe(&io)&&relative);
 state=2;assert(!probe(&io));state=0;short_reply=1;assert(!probe(&io));
 assert(armed==4&&failures>=6);
 puts("PASS Recovery HID protocol: Report preferred, SET_PROTOCOL STALL confirmed by GET_PROTOCOL, verified Boot fallback, unknown/short state rejected");
 head=tail=0;rx=100;ry=100;relative=1;u8 data[]={1,12,0};received(data,3,0,0);assert(rx==112&&ry==100);data[1]=0;data[2]=-9;received(data,3,0,0);assert(rx==112&&ry==91&&queue[0].buttons==1);
 unsigned previous_head=head;u8 idle[]={1,0,0};
 received(idle,3,0,0);assert(head==previous_head);idle[0]=0;received(idle,3,0,0);assert(queue[(head+31)%32].buttons==0);
 previous_head=head;received(idle,3,0,0);assert(head==previous_head);
 puts("PASS Recovery idle reports do not starve firmware fallback; release transitions preserved");
 puts("PASS Recovery relative Boot: independent horizontal/vertical signed motion and buttons");
}
