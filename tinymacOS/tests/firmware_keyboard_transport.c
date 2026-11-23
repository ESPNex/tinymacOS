/* Host tests execute the actual Recovery transport consumer with firmware
 * protocol doubles. They do not replace the real firmware/guest regressions. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../recovery/keyboard.c"
#include "../vendor/tmc-ps2-keymap/raw_ps2_queue.h"
static struct TmcRawPs2Queue queue;
static unsigned snapshot[256],snapshot_n,snapshot_mods;
static EFI_STATUS map_error;
static EFI_STATUS EFIAPI raw_read(TMC_RAW_PS2_PROTOCOL *self,UINT8 *b)
{
    (void)self;int r=tmc_raw_ps2_pop(&queue,b);
    return r<0?EFI_DEVICE_ERROR:r?EFI_SUCCESS:EFI_NOT_READY;
}
static EFI_STATUS EFIAPI raw_reset(TMC_RAW_PS2_PROTOCOL *self)
{
    (void)self;tmc_raw_ps2_reset(&queue);return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI map_get(RecKeymap *self,UINT16 *mods,UINTN *n,UINT16 *keys)
{
    (void)self;if(map_error)return map_error;
    assert(*n>=snapshot_n);*n=snapshot_n;*mods=snapshot_mods;
    for(unsigned i=0;i<snapshot_n;i++)keys[i]=snapshot[i];
    return EFI_SUCCESS;
}
static TMC_RAW_PS2_PROTOCOL fake_raw={TMC_RAW_PS2_PROTOCOL_REVISION,raw_reset,raw_read};
static RecKeymap fake_map={1,map_get,0};
static void clear(void)
{
    tmc_raw_ps2_reset(&queue);rec_fw_set1_reset(&ps2[0]);raw[0]=&fake_raw;
    nraw=1;keymap=&fake_map;caps=head=tail=0;ticks=repeat_at=0;
    repeat_key=repeat_mods=0;memset(usbheld,0,sizeof(usbheld));
    snapshot_n=snapshot_mods=0;map_error=EFI_SUCCESS;
}
static int poll(void)
{
    int c=-1;return rec_keyboard_next(0,&c)?c:0;
}
int main(void)
{
    clear();
    tmc_raw_ps2_push(&queue,42);tmc_raw_ps2_push(&queue,53);
    tmc_raw_ps2_push(&queue,0xb5);tmc_raw_ps2_push(&queue,0xaa);
    assert(poll()=='_');assert(!poll());assert(!ps2[0].mods);
    tmc_raw_ps2_push(&queue,0xe0);tmc_raw_ps2_push(&queue,56);tmc_raw_ps2_push(&queue,39);
    assert(poll()=='@');assert(ps2[0].mods==64);
    for(unsigned i=0;i<256;i++)tmc_raw_ps2_push(&queue,0xe0);
    assert(!poll());assert(!ps2[0].mods&&!ps2[0].ext);
    tmc_raw_ps2_push(&queue,30);assert(poll()=='a');
    clear();
    snapshot_n=2;snapshot[0]=snapshot[1]=0x7038;snapshot_mods=2;
    assert(poll()=='_');for(unsigned i=0;i<10;i++)assert(!poll());
    ticks=repeat_at;assert(poll()=='_');snapshot_n=0;
    for(unsigned i=0;i<100;i++)assert(!poll());assert(!repeat_key);
    snapshot_n=1;snapshot[0]=0x7004;snapshot_mods=0;
    assert(poll()=='a');map_error=EFI_DEVICE_ERROR;assert(!poll());assert(!repeat_key);
    for(unsigned i=0;i<100;i++)assert(!poll());
    map_error=EFI_SUCCESS;snapshot_n=0;assert(!poll());
    snapshot_n=2;snapshot[0]=0x7039;snapshot[1]=0x7004;
    assert(poll()=='A');assert(caps);assert(!poll());
    snapshot_n=0;assert(!poll());snapshot_n=1;snapshot[0]=0x7004;snapshot_mods=2;
    assert(poll()=='a');
    clear();
    snapshot_n=1;snapshot[0]=0x7033;snapshot_mods=64;assert(poll()=='@');
    snapshot_n=0;assert(!poll());snapshot_n=1;snapshot[0]=0x7040;assert(!poll());
    snapshot_n=1;snapshot[0]=0x8051;assert(!poll()); /* other HID page ignored */
    clear();tmc_raw_ps2_push(&queue,0xe0);tmc_raw_ps2_push(&queue,72);assert(poll()==0x101);
    puts("PASS actual Recovery consumer: firmware raw/USB, deduplication, Shift/AltGr, caps, repeat/release, overflow/error recovery");
    return 0;
}
