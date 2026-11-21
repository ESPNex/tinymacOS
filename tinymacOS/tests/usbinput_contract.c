#define TMC_INPUT_TEST 1
#include "../drivers/input.c"
#include TMC_USBINPUT_TEST_SOURCE
u64 ticks;
static int acknowledge_without_switch;
static int reject_boot,reject_report,bad_report,short_config,is_hub,protocol=1;
static const u8 report[]={
 5,1,9,2,0xa1,1,0x85,4,9,1,0xa1,0,5,9,0x19,1,0x29,5,0x15,0,0x25,1,0x75,1,0x95,5,0x81,2,
 0x75,3,0x95,1,0x81,1,5,1,9,0x30,9,0x31,0x16,1,0x80,0x26,255,0x7f,0x75,16,0x95,2,0x81,6,
 9,0x38,0x15,0x81,0x25,0x7f,0x75,8,0x95,1,0x81,6,0xc0,0xc0};
static int transport(UsbInput *u,int rt,int rq,int val,int ix,int n)
{
 (void)rt;(void)ix;u->ctrl_bytes=n;
 if(rq==6&&val==0x100){memset(u->desc,0,18);u->desc[7]=8;u->desc[8]=0x34;u->desc[9]=0x12;}
 else if(rq==6&&val==0x200){
  const u8 cfg[]={9,2,34,0,1,1,0,0x80,50,9,4,0,0,1,3,1,2,0,9,0x21,0x11,1,0,1,0x22,sizeof report,0,7,5,0x81,3,8,0,10};
  memcpy(u->desc,cfg,n);if(is_hub)u->desc[14]=9;if(short_config&&n>9)u->ctrl_bytes=n-1;
 }else if(rq==6&&val==0x2200){memcpy(u->desc,report,n);if(bad_report)memset(u->desc,0,n);}
 else if(rq==5){assert(!u->address&&val==7);}
 else if(rq==11){if((val==0&&reject_boot)||(val==1&&reject_report))return -2;if(val!=1||!acknowledge_without_switch)protocol=val;}
 else if(rq==3)u->desc[0]=protocol;
 else if(rq==10)return -2; /* optional SET_IDLE rejection must not kill the HID */
 else assert(rq==9);
 return 0;
}
static UsbInput u;
static int enumerate(void){memset(&u,0,sizeof u);u.speed=1;u.control=transport;return usb_input_enumerate(&u,7);}
int main(void)
{
 Event e;assert(enumerate()==1&&u.address==7&&u.npipe==1&&u.pipe[0].proto==2);
 memcpy(u.pipe[0].report,(u8[]){0,29,0},3);usb_input_report(&u,0,3);
 assert(input_pop(&e)&&e.type==EV_MOUSE&&e.a==29&&!e.b);
 memcpy(u.pipe[0].report,(u8[]){1,0,249},3);usb_input_report(&u,0,3);
 assert(input_pop(&e)&&e.type==EV_MOUSE&&!e.a&&e.b==-7&&e.c==1);
 puts("PASS USB2 Boot-first, independent X/Y and buttons despite valid Report-ID descriptor");
 reject_boot=1;protocol=1;assert(enumerate()==1&&u.pipe[0].proto==3&&u.pipe[0].layout.id==4);
 u8 data[]={4,5,0x34,0x12,0xfe,0xff,255};memcpy(u.pipe[0].report,data,sizeof data);usb_input_report(&u,0,7);
 assert(input_pop(&e)&&e.type==EV_MOUSE&&e.a==0x1234&&e.b==-2&&e.c==5);
 assert(input_pop(&e)&&e.type==EV_SCROLL&&e.a==-1);assert(!input_pop(&e));
 reject_report=1;protocol=1;assert(enumerate()==1&&u.pipe[0].proto==3);
 protocol=0;assert(enumerate()==1&&u.pipe[0].proto==2);
 protocol=2;assert(!enumerate());bad_report=1;assert(!enumerate());
 reject_boot=reject_report=0;protocol=1;assert(enumerate()==1&&u.pipe[0].proto==2);
 bad_report=0;acknowledge_without_switch=1;protocol=0;assert(enumerate()==1&&u.pipe[0].proto==2);
 puts("PASS USB2 descriptor Report fallback, ID/16-bit axes/wheel, confirmed mode despite STALL and unknown modes fail closed; optional SET_IDLE rejected safely");
 acknowledge_without_switch=0;short_config=1;assert(!enumerate());short_config=0;is_hub=1;assert(enumerate()==2&&!u.npipe);
 puts("PASS truncated configuration rejected; hub distinguished from HID");
}
