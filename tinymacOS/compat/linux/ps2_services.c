/* SPDX-License-Identifier: GPL-2.0-or-later */
/* i8042/psmouse services only. No USB device, interface, URB or class binding. */
#include "linux_usb.h"
#include "linux/tmc_compat.h"
extern void hid_relative(int,int,int,int),hid_horizontal(int);
extern int tmc_usbcore_vsnprintf(char *,size_t,const char *,va_list);
static struct input_dev *ps2_pointers[16];
void tmc_lx_spin_init(spinlock_t *s){__atomic_store_n(&s->word,0,__ATOMIC_RELAXED);}
unsigned long tmc_lx_spin_lock_irqsave(spinlock_t *s){u32 flags=tmc_lx_os_irq_save();while(__atomic_exchange_n(&s->word,1,__ATOMIC_ACQUIRE))tmc_lx_os_spin_wait();return flags;}
void tmc_lx_spin_unlock_irqrestore(spinlock_t *s,unsigned long flags){__atomic_store_n(&s->word,0,__ATOMIC_RELEASE);tmc_lx_os_irq_restore((u32)flags);}
int tmc_lx_snprintf(char *out,size_t n,const char *fmt,...){va_list ap;va_start(ap,fmt);int rc=tmc_usbcore_vsnprintf(out,n,fmt,ap);va_end(ap);return rc;}
void tmc_lx_hid_log(void *device,unsigned level,const char *fmt,...){(void)device;(void)level;char text[192];va_list ap;va_start(ap,fmt);tmc_usbcore_vsnprintf(text,sizeof text,fmt,ap);va_end(ap);tmc_lx_os_error(text);}
void input_report_key(struct input_dev *d,unsigned code,int on){if(!d||!d->registered||code<BTN_LEFT||code>BTN_EXTRA)return;unsigned bit=1u<<(code-BTN_LEFT);if(on)d->buttons|=bit;else d->buttons&=~bit;}
void input_report_rel(struct input_dev *d,unsigned axis,int delta){if(!d||!d->registered)return;if(axis==REL_X)d->x=delta;else if(axis==REL_Y)d->y=delta;else if(axis==REL_WHEEL)d->wheel=delta;else if(axis==REL_HWHEEL)d->hwheel=delta;}
void input_sync(struct input_dev *d){if(!d||!d->registered)return;unsigned buttons=0;for(unsigned i=0;i<COUNT(ps2_pointers);i++)if(ps2_pointers[i]&&ps2_pointers[i]->registered)buttons|=ps2_pointers[i]->buttons;hid_relative(d->x,d->y,buttons,d->wheel);if(d->hwheel)hid_horizontal(d->hwheel);d->x=d->y=d->wheel=d->hwheel=0;}
int tmc_lx_input_register_pointer(struct input_dev *d){if(!d||d->registered||d->owner)return -EINVAL;for(unsigned i=0;i<COUNT(ps2_pointers);i++)if(!ps2_pointers[i]){d->x=d->y=d->wheel=d->hwheel=d->buttons=0;ps2_pointers[i]=d;d->registered=1;return 0;}return -ENOSPC;}
void tmc_lx_input_unregister_pointer(struct input_dev *d){if(!d)return;for(unsigned i=0;i<COUNT(ps2_pointers);i++)if(ps2_pointers[i]==d){d->x=d->y=d->wheel=d->hwheel=0;if(d->buttons){d->buttons=0;input_sync(d);}ps2_pointers[i]=NULL;d->registered=0;return;}}
