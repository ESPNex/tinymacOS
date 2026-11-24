/* SPDX-License-Identifier: GPL-2.0-only */
#include <tmc_input.h>
static LIST_HEAD(inputs);static unsigned input_count;
extern unsigned tmc_usb_usage(unsigned);
extern void tmc_usbcore_os_input_registered(unsigned,int);
extern void tmc_usbcore_os_keys(const u8[32],unsigned);
extern void tmc_usbcore_os_pointer(int,int,unsigned,int,int);
extern void tmc_usbcore_os_absolute(int,int,unsigned,int,int);
static int pointer(struct input_dev *d){return test_bit(EV_REL,d->evbit)||test_bit(ABS_X,d->absbit)||test_bit(ABS_Y,d->absbit);}
static int scale(struct input_absinfo *a){s64 n=(s64)a->value-a->minimum,range=(s64)a->maximum-a->minimum;if(range<=0)return 0;if(n<0)n=0;if(n>range)n=range;return n*32767/range;}
static void input_release(struct device *d){kfree(container_of(d,struct input_dev,dev));}
struct input_dev *input_allocate_device(void){struct input_dev *d=kzalloc(sizeof(*d),GFP_KERNEL);if(!d)return NULL;device_initialize(&d->dev);d->dev.release=input_release;INIT_LIST_HEAD(&d->registered_entry);return d;}
void input_free_device(struct input_dev *d){if(d){BUG_ON(d->registered);put_device(&d->dev);}}
int input_register_device(struct input_dev *d){if(!d||d->registered)return -EINVAL;int r=dev_set_name(&d->dev,"usb-input%u",input_count++);if(r)return r;r=device_add(&d->dev);if(r)return r;list_add_tail(&d->registered_entry,&inputs);d->registered=1;r=d->open?d->open(d):0;if(r){d->registered=0;list_del_init(&d->registered_entry);device_del(&d->dev);}else tmc_usbcore_os_input_registered(pointer(d)?2:1,1);return r;}
void input_report_key(struct input_dev *d,unsigned code,int value){if(!d||!d->registered||code>=KEY_CNT)return;if(value)set_bit(code,d->key);else clear_bit(code,d->key);if(code>=BTN_LEFT&&code<=BTN_EXTRA){unsigned b=1u<<(code-BTN_LEFT);if(value)d->buttons|=b;else d->buttons&=~b;}}
void input_report_rel(struct input_dev *d,unsigned code,int v){if(!d||!d->registered)return;if(code==REL_X)d->x+=v;else if(code==REL_Y)d->y+=v;else if(code==REL_WHEEL)d->wheel+=v;else if(code==REL_HWHEEL)d->hwheel+=v;}
void input_sync(struct input_dev *d){if(!d||!d->registered)return;struct input_dev *i;u8 keys[32]={0};unsigned mods=0,buttons=0;list_for_each_entry(i,&inputs,registered_entry){buttons|=i->buttons;if(!pointer(i))for(unsigned k=1;k<256;k++)if(test_bit(k,i->key)){unsigned u=tmc_usb_usage(k);if(u>=224&&u<232)mods|=1u<<(u-224);else if(u>=4&&u<224)keys[u/8]|=1u<<(u%8);}}tmc_usbcore_os_keys(keys,mods);if(test_bit(ABS_X,d->absbit)&&test_bit(ABS_Y,d->absbit))tmc_usbcore_os_absolute(scale(&d->absinfo[ABS_X]),scale(&d->absinfo[ABS_Y]),buttons,d->wheel,d->hwheel);else if(test_bit(EV_REL,d->evbit))tmc_usbcore_os_pointer(d->x,d->y,buttons,d->wheel,d->hwheel);d->x=d->y=d->wheel=d->hwheel=0;}
void input_unregister_device(struct input_dev *d){if(!d)return;if(d->registered){if(d->close)d->close(d);memset(d->key,0,sizeof(d->key));d->buttons=0;input_sync(d);tmc_usbcore_os_input_registered(pointer(d)?2:1,0);list_del_init(&d->registered_entry);d->registered=0;device_unregister(&d->dev);}else input_free_device(d);}
void tmc_usb_input_leds(unsigned leds){struct input_dev *d;list_for_each_entry(d,&inputs,registered_entry)if(d->event&&test_bit(EV_LED,d->evbit)){d->led[0]=leds;d->event(d,EV_LED,LED_CAPSL,!!(leds&2));}}
void *memscan(void *p,int c,size_t n){void *q=memchr(p,c,n);return q?q:(u8 *)p+n;}
size_t strlcat(char *d,const char *s,size_t n){size_t a=strnlen(d,n),b=strlen(s);if(a<n){size_t c=min(b,n-a-1);memcpy(d+a,s,c);d[a+c]=0;}return a+b;}

void input_event(struct input_dev *d,unsigned t,unsigned c,int v){if(!d||!d->registered)return;if(t==EV_KEY)input_report_key(d,c,v);else if(t==EV_REL)input_report_rel(d,c,v);else if(t==EV_SYN&&c==SYN_REPORT)input_sync(d);else if(t==EV_ABS&&c<ABS_CNT)d->absinfo[c].value=v;else if(t==EV_LED&&c<LED_CNT){if(v)set_bit(c,d->led);else clear_bit(c,d->led);if(d->event)d->event(d,t,c,v);}}
