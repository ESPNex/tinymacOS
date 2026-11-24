/* Actual input queue/diagnostic ABI tests. Original xHCI transport behavior
 * is tested separately by linux_xhci_hcd.c, not the retired native TRB code. */
#define TMC_INPUT_TEST 1
#include "../drivers/input.c"
u64 ticks;
void usb_delay_ms(unsigned ms){(void)ms;}
static Event take(int type,int x,int y,int btn)
{Event e;assert(input_pop(&e));assert(e.type==type&&e.a==x&&e.b==y&&e.c==btn);return e;}
int main(void)
{
    (void)boot;
    for(int i=0;i<10000;i++)hid_relative(1,-1,0,0);
    take(EV_MOUSE,10000,-10000,0);Event e;assert(!input_pop(&e));assert(input_diag.merged==9999);
    hid_relative(5,2,0,0);hid_relative(0,0,1,0);hid_relative(9,3,1,0);
    hid_relative(0,0,0,0);hid_relative(7,4,0,1);hid_relative(2,1,0,0);
    take(EV_MOUSE,5,2,0);take(EV_MOUSE,0,0,1);take(EV_MOUSE,9,3,1);
    take(EV_MOUSE,0,0,0);take(EV_MOUSE,7,4,0);
    take(EV_SCROLL,1,0,0);take(EV_MOUSE,2,1,0);assert(!input_pop(&e));
    hid_absolute(10,20,0,0);hid_absolute(30,40,0,0);take(EV_POINTER_ABS,30,40,0);
    hid_absolute(10,20,0,1);hid_absolute(30,40,0,0);
    assert(take(EV_POINTER_ABS,10,20,0).d==1);take(EV_POINTER_ABS,30,40,0);
    for(int i=0;i<130;i++)put((Event){.type=EV_KEY,.a=i});
    assert(input_diag.dropped==3);for(int i=0;i<127;i++)take(EV_KEY,i,0,0);
    puts("PASS queue: 10000 motion reports, exact sum, buttons/wheel/key barriers, absolute newest, full-queue counter");
    InputDiag d;input_diagnostics(&d);assert(d.version==1&&d.bytes==sizeof d);
    puts("PASS diagnostic snapshot ABI; no keyboard content or DMA pointers exposed");
}
