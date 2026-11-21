#define TMC_REC_USB_TEST 1
#include "../recovery/usbhid.c"
int rw=1280,rh=800;
static int state=1,deny_boot,deny_report,short_reply,bad_report,armed,cancelled,cleared,connected_back;
static int wide;
static const u8 wide_descriptor[]={
 5,1,9,2,0xa1,1,0x85,4,9,1,0xa1,0,
 5,9,0x19,1,0x29,5,0x15,0,0x25,1,0x75,1,0x95,5,0x81,2,
 0x75,3,0x95,1,0x81,1,
 5,1,9,0x30,9,0x31,0x16,0x01,0x80,0x26,0xff,0x7f,0x75,16,0x95,2,0x81,6,
 9,0x38,0x15,0x81,0x25,0x7f,0x75,8,0x95,1,0x81,6,0xc0,0xc0
};

static int exclusive,device_proto=2,disconnects;
static const u8 descriptor[]={5,1,9,2,0xa1,1,9,1,0xa1,0,5,9,0x19,1,0x29,3,0x15,0,0x25,1,0x75,1,0x95,3,0x81,2,0x75,5,0x95,1,0x81,1,5,1,9,0x30,9,0x31,0x15,0x81,0x25,0x7f,0x75,8,0x95,2,0x81,6,0xc0,0xc0};
static EFI_STATUS ctl(EfiUsb *io,UsbRequest *q,int dir,UINT32 timeout,VOID *dst,UINTN n,UINT32 *err){
 (void)io;(void)dir;assert(timeout==250);*err=0;
 if(q->req==6&&q->value==0x200){u8 c[]={9,2,34,0,1,1,0,128,50,9,4,0,0,1,3,1,2,0,9,0x21,0x11,1,0,1,0x22,sizeof descriptor,0,7,5,0x81,3,8,0,10};if(wide)c[25]=sizeof wide_descriptor;memcpy(dst,c,n);return 0;}
 if(q->req==6&&q->value==0x2200){memcpy(dst,wide?wide_descriptor:descriptor,n);if(bad_report)memset(dst,0,n);return 0;}
 if(q->req==11){if(q->value==0?deny_boot:deny_report)return 1;state=q->value;return 0;}
 if(q->req==3){if(!short_reply)*(u8*)dst=state;return 0;}
 if(q->req==1){assert(q->rt==2&&q->index==0x81&&!n);cleared++;return 0;}
 assert(0);return 1;
}
static EFI_STATUS iface(EfiUsb *io,UsbInterface *f){(void)io;*f=(UsbInterface){9,4,0,0,1,3,1,device_proto,0};return 0;}
static EFI_STATUS endpoint(EfiUsb *io,UINT8 n,UsbEndpoint *e){(void)io;assert(!n);*e=(UsbEndpoint){7,5,0x81,3,8,10};return 0;}
static EFI_STATUS start(EfiUsb *io,UINT8 ep,BOOLEAN add,UINTN interval,UINTN len,Notify cb,VOID *arg){
 (void)io;(void)arg;assert(ep==0x81);if(!add){assert(!interval&&!len&&!cb);cancelled++;return 0;}
 assert(interval==10&&len==8&&cb==received);armed++;return 0;
}
static EfiUsb io={.control=ctl,.async=start,.interface_desc=iface,.endpoint_desc=endpoint};
static EFI_STATUS locate(int t,EFI_GUID*g,void*v,UINTN*n,EFI_HANDLE**h){(void)t;(void)g;(void)v;static EFI_HANDLE one[1]={(void*)1};*n=1;*h=one;return 0;}
static EFI_STATUS info(EFI_HANDLE h,EFI_GUID*g,EFI_OPEN_PROTOCOL_INFORMATION_ENTRY**p,UINTN*n){(void)h;(void)g;static EFI_OPEN_PROTOCOL_INFORMATION_ENTRY x;*n=1;x=(EFI_OPEN_PROTOCOL_INFORMATION_ENTRY){exclusive?EFI_OPEN_PROTOCOL_BY_DRIVER:0,(void*)2};*p=&x;return 0;}
static EFI_STATUS freepool(void*p){(void)p;return 0;}
static EFI_STATUS handle(EFI_HANDLE h,EFI_GUID*g,void**p){(void)h;(void)g;*p=&io;return 0;}
static EFI_STATUS detach(EFI_HANDLE h,EFI_HANDLE a,EFI_HANDLE child){assert(h==(void*)1&&a==(void*)2&&!child);disconnects++;exclusive=0;return 0;}
static EFI_STATUS connect(EFI_HANDLE h,EFI_HANDLE*a,void*p,BOOLEAN r){(void)h;(void)a;(void)p;(void)r;connected_back++;return 0;}
static EFI_TPL raise(EFI_TPL t){assert(t==TPL_NOTIFY);return 4;}
static void restore(EFI_TPL t){assert(t==4);}
static EFI_BOOT_SERVICES bs={locate,info,freepool,handle,raise,restore,detach,connect};
int main(void){
 assert(probe(&io)&&!relative&&state==1);
 deny_report=1;state=1;assert(probe(&io)&&!relative);
 state=0;assert(probe(&io)&&relative);deny_boot=1;state=2;assert(!probe(&io));
 short_reply=1;assert(!probe(&io));short_reply=0;deny_boot=deny_report=0;
 bad_report=1;assert(probe(&io)&&relative);bad_report=0;
 deny_report=1;state=0;
 puts("PASS Recovery descriptor Report preference, verified Boot fallback, mode mismatch rejected");
 assert(rec_pointer_start(&bs,100,100));TabletState s;
 u8 r[]={1,12,0};received(r,3,0,0);r[1]=0;r[2]=-9;received(r,3,0,0);
 assert(rec_pointer_next(&bs,&s)&&rec_pointer_relative()&&s.x==12&&s.y==-9&&s.buttons==1);
 /* No stale private absolute accumulator: native cursor can move via fallback
  * before the next direct event; queued coordinates remain only +1,0. */
 r[1]=1;r[2]=0;received(r,3,0,0);assert(rec_pointer_next(&bs,&s)&&s.x==1&&!s.y);
 r[0]=0;r[1]=0;received(r,3,0,0);assert(rec_pointer_next(&bs,&s)&&!s.buttons);received(r,3,0,0);assert(!rec_pointer_next(&bs,&s));
 puts("PASS Recovery relative deltas coalesce without pinning cursor to a stale source position; release and idle filtering retained");
 for(int episode=0;episode<12;episode++){
  int before=armed;received(0,0,0,2);
  for(int i=0;i<8;i++)rec_pointer_next(&bs,&s);
  assert(armed==before+1&&!pending_error&&cancelled==episode+1&&cleared==episode+1);
  r[1]=1;received(r,3,0,0);assert(rec_pointer_next(&bs,&s)&&s.x==1);
 }
 puts("PASS Recovery actual async error path: twelve separate STALL/cancel/clear/rearm/success cycles; no lifetime failure limit");
 wide=1;deny_report=1;state=0;assert(rec_pointer_start(&bs,100,100)&&relative);
 u8 id_report[]={4,0,64,0,0,0,0};received(id_report,7,0,0);
 assert(rec_pointer_next(&bs,&s)&&s.x==64&&!s.y);
 id_report[2]=0;id_report[4]=64;received(id_report,7,0,0);
 assert(rec_pointer_next(&bs,&s)&&!s.x&&s.y==64);
 id_report[0]=5;received(id_report,7,0,0);assert(!rec_pointer_next(&bs,&s));
 puts("PASS actual Recovery vertical-only reproduction: ID/16-bit report despite Boot indication, X=64/Y=0 then X=0/Y=64; foreign ID ignored");
 wide=0;state=0;
 exclusive=1;assert(rec_pointer_start(&bs,100,100)&&disconnects==1&&!connected_back);
 exclusive=1;device_proto=1;assert(!rec_pointer_start(&bs,100,100)&&disconnects==1);
 device_proto=2;exclusive=1;deny_boot=deny_report=1;state=2;
 assert(!rec_pointer_start(&bs,100,100)&&disconnects==2&&connected_back==1);
 puts("PASS Recovery takes over only the Boot-mouse child; leaves keyboard driver alone and reconnects firmware on failed takeover");
}
