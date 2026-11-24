#ifndef TMC_HID_CONFIG_H
#define TMC_HID_CONFIG_H
#include "base.h"
typedef struct {
    unsigned iface,proto,ep,packet,interval,report_length;
    unsigned burst,mult,bytes_per_interval;
} HidInterface;
/* Supported alternate-zero HID interfaces, one interrupt-IN endpoint each.
 * Protocol 1=boot keyboard, 2=boot mouse, 3=validated report pointer candidate.
 * More than capacity interfaces are ignored; malformed descriptors return -1. */
int hid_interfaces(const u8 *,unsigned,HidInterface *,unsigned);
/* Actual Linux USB speed number; parsed and normalized by original config.c. */
int hid_interfaces_at_speed(const u8 *,unsigned,unsigned,HidInterface *,unsigned);
/* Linux descriptor core also selects the actual configuration/hub class. */
int hid_configuration_at_speed(const u8 *,unsigned,unsigned,HidInterface *,unsigned,unsigned *,int *);
#endif
