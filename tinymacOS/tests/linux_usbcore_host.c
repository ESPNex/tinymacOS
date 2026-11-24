/* Test adapter ONLY; never compiled into a kernel. */
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
static uint64_t us;static unsigned allocations;static long fail_after=-1;
void host_fail_after(long n){fail_after=n;}
unsigned host_live_allocations(void){return allocations;}
void *tmc_usbcore_os_alloc(size_t n){if(fail_after==0)return NULL;if(fail_after>0)fail_after--;void *p=malloc(n);if(p)allocations++;return p;}
void tmc_usbcore_os_free(void *p){if(p){if(!allocations)abort();allocations--;free(p);}}
uint64_t tmc_usbcore_os_clock_ms(void){return us/1000;}
void tmc_usbcore_os_delay_us(unsigned n){us+=n;}
void tmc_usbcore_os_poll(void){}
uint32_t tmc_usbcore_os_irq_save(void){return 0;}
void tmc_usbcore_os_irq_restore(uint32_t f){(void)f;}
void tmc_usbcore_os_log(unsigned l,const char *s,va_list a){(void)l;(void)s;(void)a;}
void tmc_usbcore_os_fatal(void){fputs("usbcore fatal invariant\n",stderr);abort();}
