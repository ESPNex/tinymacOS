/* Included by the existing nonidentity DMA hardware models, not production. */
#ifdef TMC_USB8_XHCI
#define USB8_DEVICE TmcXhciDevice
#define USB8_CALL(n) tmc_linux_xhci_##n
#else
#define USB8_DEVICE TmcEhciDevice
#define USB8_CALL(n) tmc_linux_ehci_##n
#endif
static unsigned bulk8_callbacks,bulk8_bytes,bulk8_rearm;
static int bulk8_status;
static USB8_DEVICE *bulk8_device;
static u8 bulk8_buffer[65536+32];
static void bulk8_done(void *ctx,int status,unsigned n)
{
 assert(ctx==(void *)0xb818);bulk8_callbacks++;bulk8_status=status;bulk8_bytes=n;
 if(bulk8_rearm){assert(!status&&n==512);if(--bulk8_rearm)assert(!USB8_CALL(bulk_submit)(bulk8_device,2,512,0,512,bulk8_buffer+16,bulk8_done,ctx));}
}
static void bulk8_checks(USB8_DEVICE *d)
{
 bulk8_device=d;unsigned before,old_interrupts=int_callbacks;
 memset(bulk8_buffer,0xcc,sizeof bulk8_buffer);
 assert(USB8_CALL(bulk_submit)(d,0,512,0,512,bulk8_buffer+16,bulk8_done,(void*)0xb818)==-EINVAL);
 assert(USB8_CALL(bulk_submit)(d,2,0,0,512,bulk8_buffer+16,bulk8_done,(void*)0xb818)==-EINVAL);
 assert(USB8_CALL(bulk_submit)(d,2,512,0,65537,bulk8_buffer+16,bulk8_done,(void*)0xb818)==-EINVAL);
 assert(USB8_CALL(bulk_submit)(d,2,512,0,1,NULL,bulk8_done,(void*)0xb818)==-EINVAL);
 assert(USB8_CALL(bulk_submit)(d,2,512,1,512,bulk8_buffer+16,bulk8_done,(void*)0xb818)==-EINVAL);
 before=bulk8_callbacks;assert(!USB8_CALL(bulk_submit)(d,2,512,0,65536,bulk8_buffer+16,bulk8_done,(void*)0xb818));
 assert(USB8_CALL(bulk_submit)(d,2,512,0,512,bulk8_buffer+16,bulk8_done,(void*)0xb818)==-EBUSY);
 hw_interrupt_ready=1024;
 for(unsigned i=0;i<300&&bulk8_callbacks==before;i++)USB8_CALL(poll)();
 assert(bulk8_callbacks==before+1&&!bulk8_status&&bulk8_bytes==65536&&bulk8_buffer[16]==0x40);
 for(unsigned i=0;i<16;i++)assert(bulk8_buffer[i]==0xcc&&bulk8_buffer[65536+16+i]==0xcc);
 memset(bulk8_buffer,0xcc,sizeof bulk8_buffer);hw_short=1;before=bulk8_callbacks;
 assert(!USB8_CALL(bulk_submit)(d,2,512,0,512,bulk8_buffer+16,bulk8_done,(void*)0xb818));hw_interrupt_ready=64;
 for(unsigned i=0;i<100&&bulk8_callbacks==before;i++)USB8_CALL(poll)();
 assert(bulk8_callbacks==before+1&&!bulk8_status&&bulk8_bytes==3&&bulk8_buffer[16]==0x40&&bulk8_buffer[18]==0x42);
 for(unsigned i=19;i<sizeof bulk8_buffer;i++)assert(bulk8_buffer[i]==0xcc);hw_short=0;
 before=bulk8_callbacks;hw_stall=1;
 assert(!USB8_CALL(bulk_submit)(d,2,512,0,512,bulk8_buffer+16,bulk8_done,(void*)0xb818));hw_interrupt_ready=64;
 for(unsigned i=0;i<100&&bulk8_callbacks==before;i++)USB8_CALL(poll)();
 assert(bulk8_callbacks==before+1&&bulk8_status==-EPIPE);
 assert(!USB8_CALL(control)(d,2,1,0,0x82,NULL,0,300));USB8_CALL(reset_toggle)(d,2);
 before=bulk8_callbacks;hw_never=1;
 assert(!USB8_CALL(bulk_submit)(d,2,512,0,512,bulk8_buffer+16,bulk8_done,(void*)0xb818));
 for(unsigned i=0;i<10;i++)USB8_CALL(poll)();assert(bulk8_callbacks==before);
 assert(!USB8_CALL(bulk_stop)(d,2));assert(bulk8_callbacks==before+1&&bulk8_status==-ENOENT);hw_never=0;
 before=bulk8_callbacks;memset(bulk8_buffer+16,0xa5,32768);unsigned output_before=output_bytes;
 assert(!USB8_CALL(bulk_submit)(d,0x103,512,0,32768,bulk8_buffer+16,bulk8_done,(void*)0xb818));hw_interrupt_ready=1024;
 for(unsigned i=0;i<300&&bulk8_callbacks==before;i++)USB8_CALL(poll)();
 assert(bulk8_callbacks==before+1&&!bulk8_status&&bulk8_bytes==32768&&output_bytes==output_before+32768);
 before=bulk8_callbacks;bulk8_rearm=64;
 assert(!USB8_CALL(bulk_submit)(d,2,512,0,512,bulk8_buffer+16,bulk8_done,(void*)0xb818));
 for(unsigned i=0;i<512&&bulk8_rearm;i++){hw_interrupt_ready=32;USB8_CALL(poll)();}
 assert(!bulk8_rearm&&bulk8_callbacks==before+64);
 assert(int_callbacks==old_interrupts); /* bulk must not invoke/change HID report state */
 assert(!USB8_CALL(bulk_stop)(d,2));assert(!USB8_CALL(bulk_stop)(d,0x103));
 puts("PASS bulk original HCD: 64KiB IN with guards, 32KiB exact OUT, busy/invalid bounds, short-copy actual length, STALL/clear-halt/reset, non-completion/cancel, 64 immediate callback requeues, unchanged interrupt callback state; nonidentity DMA MODEL only");
}
#undef USB8_DEVICE
#undef USB8_CALL
