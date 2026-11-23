/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
/* Private parser ABI. No object crosses the Boot USB/HCD compatibility ABI.
 * Constants and parser/report/field structs below are from Linux v6.12 hid.h.
 * This is not a Linux device model, input subsystem or HID bus shim. */
#include "base.h"
#include <stdbool.h>
#include "linux_hid_parser.h"
typedef u8 __u8;typedef s8 __s8;typedef u16 __u16;typedef int16_t __s16;
typedef u32 __u32;typedef int32_t __s32;typedef int32_t s32;typedef u16 __le16;typedef u32 __le32;
#define BIT(n) (1u<<(n))
#define GFP_KERNEL 0
#define ENOMEM 12
#define EINVAL 22
#define EBUSY 16
#define ENODEV 19
#define EXPORT_SYMBOL_GPL(x)
#define WARN_ON(x) (!!(x))
#define unlikely(x) (x)
#define max_t(t,a,b) MAX((t)(a),(t)(b))
#define container_of(p,t,m) ((t *)((char *)(p)-offsetof(t,m)))
struct list_head {struct list_head *next,*prev;};
static inline void INIT_LIST_HEAD(struct list_head *h){h->next=h->prev=h;}
static inline void list_add_tail(struct list_head *n,struct list_head *h){n->prev=h->prev;n->next=h;h->prev->next=n;h->prev=n;}
#define list_for_each_entry(p,h,m) for(p=container_of((h)->next,__typeof__(*p),m);&p->m!=(h);p=container_of(p->m.next,__typeof__(*p),m))
enum hid_report_type {HID_INPUT_REPORT,HID_OUTPUT_REPORT,HID_FEATURE_REPORT,HID_REPORT_TYPES};
void *tmc_hid_alloc(size_t n,int zero);
void *tmc_hid_realloc(void *p,size_t n);
void tmc_hid_free(void *p);
void tmc_hid_notice(void);
#define kmalloc(n,f) tmc_hid_alloc(n,0)
#define kzalloc(n,f) tmc_hid_alloc(n,1)
#define kvzalloc(n,f) tmc_hid_alloc(n,1)
#define vzalloc(n) tmc_hid_alloc(n,1)
#define krealloc(p,n,f) tmc_hid_realloc(p,n)
#define kfree(p) tmc_hid_free((void *)(p))
#define kvfree(p) kfree(p)
#define vfree(p) kfree(p)
#define hid_err(d,...) tmc_hid_notice()
#define hid_warn(d,...) tmc_hid_notice()
#define dbg_hid(...) ((void)0)
static inline size_t array3_size(size_t a,size_t b,size_t c){return b&&a>SIZE_MAX/b?SIZE_MAX:c&&a*b>SIZE_MAX/c?SIZE_MAX:a*b*c;}
static inline void *kcalloc(size_t a,size_t b,unsigned flags){return tmc_hid_alloc(array3_size(a,b,1),1);}
static inline void *kmemdup(const void *p,size_t n,unsigned flags){void *d=tmc_hid_alloc(n,0);if(d)memcpy(d,p,n);return d;}
static inline u16 get_unaligned_le16(const u8 *p){return p[0]|(u16)p[1]<<8;}
static inline u32 get_unaligned_le32(const u8 *p){return get_unaligned_le16(p)|(u32)get_unaligned_le16(p+2)<<16;}
struct device {unsigned named;};
static inline const char *dev_name(struct device *d){return d->named?"tmc-hid-parser":(const char *)0;}
struct hid_item {
	unsigned  format;
	__u8      size;
	__u8      type;
	__u8      tag;
	union {
	    __u8   u8;
	    __s8   s8;
	    __u16  u16;
	    __s16  s16;
	    __u32  u32;
	    __s32  s32;
	    const __u8  *longdata;
	} data;
};

/*
 * HID report item format
 */

#define HID_ITEM_FORMAT_SHORT	0
#define HID_ITEM_FORMAT_LONG	1

/*
 * Special tag indicating long items
 */

#define HID_ITEM_TAG_LONG	15

/*
 * HID report descriptor item type (prefix bit 2,3)
 */

#define HID_ITEM_TYPE_MAIN		0
#define HID_ITEM_TYPE_GLOBAL		1
#define HID_ITEM_TYPE_LOCAL		2
#define HID_ITEM_TYPE_RESERVED		3

/*
 * HID report descriptor main item tags
 */

#define HID_MAIN_ITEM_TAG_INPUT			8
#define HID_MAIN_ITEM_TAG_OUTPUT		9
#define HID_MAIN_ITEM_TAG_FEATURE		11
#define HID_MAIN_ITEM_TAG_BEGIN_COLLECTION	10
#define HID_MAIN_ITEM_TAG_END_COLLECTION	12

/*
 * HID report descriptor main item contents
 */

#define HID_MAIN_ITEM_CONSTANT		0x001
#define HID_MAIN_ITEM_VARIABLE		0x002
#define HID_MAIN_ITEM_RELATIVE		0x004
#define HID_MAIN_ITEM_WRAP		0x008
#define HID_MAIN_ITEM_NONLINEAR		0x010
#define HID_MAIN_ITEM_NO_PREFERRED	0x020
#define HID_MAIN_ITEM_NULL_STATE	0x040
#define HID_MAIN_ITEM_VOLATILE		0x080
#define HID_MAIN_ITEM_BUFFERED_BYTE	0x100

/*
 * HID report descriptor collection item types
 */

#define HID_COLLECTION_PHYSICAL		0
#define HID_COLLECTION_APPLICATION	1
#define HID_COLLECTION_LOGICAL		2
#define HID_COLLECTION_NAMED_ARRAY	4

/*
 * HID report descriptor global item tags
 */

#define HID_GLOBAL_ITEM_TAG_USAGE_PAGE		0
#define HID_GLOBAL_ITEM_TAG_LOGICAL_MINIMUM	1
#define HID_GLOBAL_ITEM_TAG_LOGICAL_MAXIMUM	2
#define HID_GLOBAL_ITEM_TAG_PHYSICAL_MINIMUM	3
#define HID_GLOBAL_ITEM_TAG_PHYSICAL_MAXIMUM	4
#define HID_GLOBAL_ITEM_TAG_UNIT_EXPONENT	5
#define HID_GLOBAL_ITEM_TAG_UNIT		6
#define HID_GLOBAL_ITEM_TAG_REPORT_SIZE		7
#define HID_GLOBAL_ITEM_TAG_REPORT_ID		8
#define HID_GLOBAL_ITEM_TAG_REPORT_COUNT	9
#define HID_GLOBAL_ITEM_TAG_PUSH		10
#define HID_GLOBAL_ITEM_TAG_POP			11

/*
 * HID report descriptor local item tags
 */

#define HID_LOCAL_ITEM_TAG_USAGE		0
#define HID_LOCAL_ITEM_TAG_USAGE_MINIMUM	1
#define HID_LOCAL_ITEM_TAG_USAGE_MAXIMUM	2
#define HID_LOCAL_ITEM_TAG_DESIGNATOR_INDEX	3
#define HID_LOCAL_ITEM_TAG_DESIGNATOR_MINIMUM	4
#define HID_LOCAL_ITEM_TAG_DESIGNATOR_MAXIMUM	5
#define HID_LOCAL_ITEM_TAG_STRING_INDEX		7
#define HID_LOCAL_ITEM_TAG_STRING_MINIMUM	8
#define HID_LOCAL_ITEM_TAG_STRING_MAXIMUM	9
#define HID_LOCAL_ITEM_TAG_DELIMITER		10

/*
 * HID usage tables
 */

#define HID_USAGE_PAGE		0xffff0000

#define HID_UP_UNDEFINED	0x00000000
#define HID_UP_GENDESK		0x00010000
#define HID_UP_SIMULATION	0x00020000
#define HID_UP_GENDEVCTRLS	0x00060000
#define HID_UP_KEYBOARD		0x00070000
#define HID_UP_LED		0x00080000
#define HID_UP_BUTTON		0x00090000
#define HID_UP_ORDINAL		0x000a0000
#define HID_UP_TELEPHONY	0x000b0000
#define HID_UP_CONSUMER		0x000c0000
#define HID_UP_DIGITIZER	0x000d0000
#define HID_UP_PID		0x000f0000
#define HID_UP_BATTERY		0x00850000
#define HID_UP_CAMERA		0x00900000
#define HID_UP_HPVENDOR         0xff7f0000
#define HID_UP_HPVENDOR2        0xff010000
#define HID_UP_MSVENDOR		0xff000000
#define HID_UP_CUSTOM		0x00ff0000
#define HID_UP_LOGIVENDOR	0xffbc0000
#define HID_UP_LOGIVENDOR2   0xff090000
#define HID_UP_LOGIVENDOR3   0xff430000
#define HID_UP_LNVENDOR		0xffa00000
#define HID_UP_SENSOR		0x00200000
#define HID_UP_ASUSVENDOR	0xff310000
#define HID_UP_GOOGLEVENDOR	0xffd10000

#define HID_USAGE		0x0000ffff

#define HID_GD_POINTER		0x00010001
#define HID_GD_MOUSE		0x00010002
#define HID_GD_JOYSTICK		0x00010004
#define HID_GD_GAMEPAD		0x00010005
#define HID_GD_KEYBOARD		0x00010006
#define HID_GD_KEYPAD		0x00010007
#define HID_GD_MULTIAXIS	0x00010008
/*
 * Microsoft Win8 Wireless Radio Controls extensions CA, see:
 * http://www.usb.org/developers/hidpage/HUTRR40RadioHIDUsagesFinal.pdf
 */
#define HID_GD_WIRELESS_RADIO_CTLS	0x0001000c
/*
 * System Multi-Axis, see:
 * http://www.usb.org/developers/hidpage/HUTRR62_-_Generic_Desktop_CA_for_System_Multi-Axis_Controllers.txt
 */
#define HID_GD_SYSTEM_MULTIAXIS	0x0001000e

#define HID_GD_X		0x00010030
#define HID_GD_Y		0x00010031
#define HID_GD_Z		0x00010032
#define HID_GD_RX		0x00010033
#define HID_GD_RY		0x00010034
#define HID_GD_RZ		0x00010035
#define HID_GD_SLIDER		0x00010036
#define HID_GD_DIAL		0x00010037
#define HID_GD_WHEEL		0x00010038
#define HID_GD_HATSWITCH	0x00010039
#define HID_GD_BUFFER		0x0001003a
#define HID_GD_BYTECOUNT	0x0001003b
#define HID_GD_MOTION		0x0001003c
#define HID_GD_START		0x0001003d
#define HID_GD_SELECT		0x0001003e
#define HID_GD_VX		0x00010040
#define HID_GD_VY		0x00010041
#define HID_GD_VZ		0x00010042
#define HID_GD_VBRX		0x00010043
#define HID_GD_VBRY		0x00010044
#define HID_GD_VBRZ		0x00010045
#define HID_GD_VNO		0x00010046
#define HID_GD_FEATURE		0x00010047
#define HID_GD_RESOLUTION_MULTIPLIER	0x00010048
#define HID_GD_SYSTEM_CONTROL	0x00010080
#define HID_GD_UP		0x00010090
#define HID_GD_DOWN		0x00010091
#define HID_GD_RIGHT		0x00010092
#define HID_GD_LEFT		0x00010093
/* Microsoft Win8 Wireless Radio Controls CA usage codes */
#define HID_GD_RFKILL_BTN	0x000100c6
#define HID_GD_RFKILL_LED	0x000100c7
#define HID_GD_RFKILL_SWITCH	0x000100c8

#define HID_DC_BATTERYSTRENGTH	0x00060020

#define HID_CP_CONSUMER_CONTROL	0x000c0001
#define HID_CP_AC_PAN		0x000c0238

#define HID_DG_DIGITIZER	0x000d0001
#define HID_DG_PEN		0x000d0002
#define HID_DG_LIGHTPEN		0x000d0003
#define HID_DG_TOUCHSCREEN	0x000d0004
#define HID_DG_TOUCHPAD		0x000d0005
#define HID_DG_WHITEBOARD	0x000d0006
#define HID_DG_STYLUS		0x000d0020
#define HID_DG_PUCK		0x000d0021
#define HID_DG_FINGER		0x000d0022
#define HID_DG_TIPPRESSURE	0x000d0030
#define HID_DG_BARRELPRESSURE	0x000d0031
#define HID_DG_INRANGE		0x000d0032
#define HID_DG_TOUCH		0x000d0033
#define HID_DG_UNTOUCH		0x000d0034
#define HID_DG_TAP		0x000d0035
#define HID_DG_TRANSDUCER_INDEX	0x000d0038
#define HID_DG_TABLETFUNCTIONKEY	0x000d0039
#define HID_DG_PROGRAMCHANGEKEY	0x000d003a
#define HID_DG_BATTERYSTRENGTH	0x000d003b
#define HID_DG_INVERT		0x000d003c
#define HID_DG_TILT_X		0x000d003d
#define HID_DG_TILT_Y		0x000d003e
#define HID_DG_TWIST		0x000d0041
#define HID_DG_TIPSWITCH	0x000d0042
#define HID_DG_TIPSWITCH2	0x000d0043
#define HID_DG_BARRELSWITCH	0x000d0044
#define HID_DG_ERASER		0x000d0045
#define HID_DG_TABLETPICK	0x000d0046
#define HID_DG_PEN_COLOR			0x000d005c
#define HID_DG_PEN_LINE_WIDTH			0x000d005e
#define HID_DG_PEN_LINE_STYLE			0x000d0070
#define HID_DG_PEN_LINE_STYLE_INK		0x000d0072
#define HID_DG_PEN_LINE_STYLE_PENCIL		0x000d0073
#define HID_DG_PEN_LINE_STYLE_HIGHLIGHTER	0x000d0074
#define HID_DG_PEN_LINE_STYLE_CHISEL_MARKER	0x000d0075
#define HID_DG_PEN_LINE_STYLE_BRUSH		0x000d0076
#define HID_DG_PEN_LINE_STYLE_NO_PREFERENCE	0x000d0077

#define HID_CP_CONSUMERCONTROL	0x000c0001
#define HID_CP_NUMERICKEYPAD	0x000c0002
#define HID_CP_PROGRAMMABLEBUTTONS	0x000c0003
#define HID_CP_MICROPHONE	0x000c0004
#define HID_CP_HEADPHONE	0x000c0005
#define HID_CP_GRAPHICEQUALIZER	0x000c0006
#define HID_CP_FUNCTIONBUTTONS	0x000c0036
#define HID_CP_SELECTION	0x000c0080
#define HID_CP_MEDIASELECTION	0x000c0087
#define HID_CP_SELECTDISC	0x000c00ba
#define HID_CP_VOLUMEUP		0x000c00e9
#define HID_CP_VOLUMEDOWN	0x000c00ea
#define HID_CP_PLAYBACKSPEED	0x000c00f1
#define HID_CP_PROXIMITY	0x000c0109
#define HID_CP_SPEAKERSYSTEM	0x000c0160
#define HID_CP_CHANNELLEFT	0x000c0161
#define HID_CP_CHANNELRIGHT	0x000c0162
#define HID_CP_CHANNELCENTER	0x000c0163
#define HID_CP_CHANNELFRONT	0x000c0164
#define HID_CP_CHANNELCENTERFRONT	0x000c0165
#define HID_CP_CHANNELSIDE	0x000c0166
#define HID_CP_CHANNELSURROUND	0x000c0167
#define HID_CP_CHANNELLOWFREQUENCYENHANCEMENT	0x000c0168
#define HID_CP_CHANNELTOP	0x000c0169
#define HID_CP_CHANNELUNKNOWN	0x000c016a
#define HID_CP_APPLICATIONLAUNCHBUTTONS	0x000c0180
#define HID_CP_GENERICGUIAPPLICATIONCONTROLS	0x000c0200

#define HID_DG_DEVICECONFIG	0x000d000e
#define HID_DG_DEVICESETTINGS	0x000d0023
#define HID_DG_AZIMUTH		0x000d003f
#define HID_DG_CONFIDENCE	0x000d0047
#define HID_DG_WIDTH		0x000d0048
#define HID_DG_HEIGHT		0x000d0049
#define HID_DG_CONTACTID	0x000d0051
#define HID_DG_INPUTMODE	0x000d0052
#define HID_DG_DEVICEINDEX	0x000d0053
#define HID_DG_CONTACTCOUNT	0x000d0054
#define HID_DG_CONTACTMAX	0x000d0055
#define HID_DG_SCANTIME		0x000d0056
#define HID_DG_SURFACESWITCH	0x000d0057
#define HID_DG_BUTTONSWITCH	0x000d0058
#define HID_DG_BUTTONTYPE	0x000d0059
#define HID_DG_BARRELSWITCH2	0x000d005a
#define HID_DG_TOOLSERIALNUMBER	0x000d005b
#define HID_DG_LATENCYMODE	0x000d0060

#define HID_BAT_ABSOLUTESTATEOFCHARGE	0x00850065
#define HID_BAT_CHARGING		0x00850044

#define HID_VD_ASUS_CUSTOM_MEDIA_KEYS	0xff310076

/*
 * HID connect requests
 */

#define HID_CONNECT_HIDINPUT		BIT(0)
#define HID_CONNECT_HIDINPUT_FORCE	BIT(1)
#define HID_CONNECT_HIDRAW		BIT(2)
#define HID_CONNECT_HIDDEV		BIT(3)
#define HID_CONNECT_HIDDEV_FORCE	BIT(4)
#define HID_CONNECT_FF			BIT(5)
#define HID_CONNECT_DRIVER		BIT(6)
#define HID_CONNECT_DEFAULT	(HID_CONNECT_HIDINPUT|HID_CONNECT_HIDRAW| \
		HID_CONNECT_HIDDEV|HID_CONNECT_FF)


struct hid_global {
	unsigned usage_page;
	__s32    logical_minimum;
	__s32    logical_maximum;
	__s32    physical_minimum;
	__s32    physical_maximum;
	__s32    unit_exponent;
	unsigned unit;
	unsigned report_id;
	unsigned report_size;
	unsigned report_count;
};

/*
 * This is the local environment. It is persistent up the next main-item.
 */

#define HID_MAX_USAGES			12288
#define HID_DEFAULT_NUM_COLLECTIONS	16

struct hid_local {
	unsigned usage[HID_MAX_USAGES]; /* usage array */
	u8 usage_size[HID_MAX_USAGES]; /* usage size array */
	unsigned collection_index[HID_MAX_USAGES]; /* collection index array */
	unsigned usage_index;
	unsigned usage_minimum;
	unsigned delimiter_depth;
	unsigned delimiter_branch;
};

/*
 * This is the collection stack. We climb up the stack to determine
 * application and function of each field.
 */

struct hid_collection {
	int parent_idx; /* device->collection */
	unsigned type;
	unsigned usage;
	unsigned level;
};

struct hid_usage {
	unsigned  hid;			/* hid usage code */
	unsigned  collection_index;	/* index into collection array */
	unsigned  usage_index;		/* index into usage array */
	__s8	  resolution_multiplier;/* Effective Resolution Multiplier
					   (HUT v1.12, 4.3.1), default: 1 */
	/* hidinput data */
	__s8	  wheel_factor;		/* 120/resolution_multiplier */
	__u16     code;			/* input driver code */
	__u8      type;			/* input driver type */
	__s16	  hat_min;		/* hat switch fun */
	__s16	  hat_max;		/* ditto */
	__s16	  hat_dir;		/* ditto */
	__s16	  wheel_accumulated;	/* hi-res wheel */
};

struct hid_input;

struct hid_field {
	unsigned  physical;		/* physical usage for this field */
	unsigned  logical;		/* logical usage for this field */
	unsigned  application;		/* application usage for this field */
	struct hid_usage *usage;	/* usage table for this function */
	unsigned  maxusage;		/* maximum usage index */
	unsigned  flags;		/* main-item flags (i.e. volatile,array,constant) */
	unsigned  report_offset;	/* bit offset in the report */
	unsigned  report_size;		/* size of this field in the report */
	unsigned  report_count;		/* number of this field in the report */
	unsigned  report_type;		/* (input,output,feature) */
	__s32    *value;		/* last known value(s) */
	__s32    *new_value;		/* newly read value(s) */
	__s32    *usages_priorities;	/* priority of each usage when reading the report
					 * bits 8-16 are reserved for hid-input usage
					 */
	__s32     logical_minimum;
	__s32     logical_maximum;
	__s32     physical_minimum;
	__s32     physical_maximum;
	__s32     unit_exponent;
	unsigned  unit;
	bool      ignored;		/* this field is ignored in this event */
	struct hid_report *report;	/* associated report */
	unsigned index;			/* index into report->field[] */
	/* hidinput data */
	struct hid_input *hidinput;	/* associated input structure */
	__u16 dpad;			/* dpad input code */
	unsigned int slot_idx;		/* slot index in a report */
};

#define HID_MAX_FIELDS 256

struct hid_field_entry {
	struct list_head list;
	struct hid_field *field;
	unsigned int index;
	__s32 priority;
};

struct hid_report {
	struct list_head list;
	struct list_head hidinput_list;
	struct list_head field_entry_list;		/* ordered list of input fields */
	unsigned int id;				/* id of this report */
	enum hid_report_type type;			/* report type */
	unsigned int application;			/* application usage for this report */
	struct hid_field *field[HID_MAX_FIELDS];	/* fields of the report */
	struct hid_field_entry *field_entries;		/* allocated memory of input field_entry */
	unsigned maxfield;				/* maximum valid field index */
	unsigned size;					/* size of the report (bits) */
	struct hid_device *device;			/* associated device */

	/* tool related state */
	bool tool_active;				/* whether the current tool is active */
	unsigned int tool;				/* BTN_TOOL_* */
};

#define HID_MAX_IDS 256

struct hid_report_enum {
	unsigned numbered;
	struct list_head report_list;
	struct hid_report *report_id_hash[HID_MAX_IDS];
};

#define HID_MIN_BUFFER_SIZE	64		/* make sure there is at least a packet size of space */
#define HID_MAX_BUFFER_SIZE	16384		/* 16kb */
#define HID_CONTROL_FIFO_SIZE	256		/* to init devices with >100 reports */
#define HID_OUTPUT_FIFO_SIZE	64


#define HID_GLOBAL_STACK_SIZE 4
#define HID_COLLECTION_STACK_SIZE 4

#define HID_SCAN_FLAG_MT_WIN_8			BIT(0)
#define HID_SCAN_FLAG_VENDOR_SPECIFIC		BIT(1)
#define HID_SCAN_FLAG_GD_POINTER		BIT(2)

struct hid_parser {
	struct hid_global     global;
	struct hid_global     global_stack[HID_GLOBAL_STACK_SIZE];
	unsigned int          global_stack_ptr;
	struct hid_local      local;
	unsigned int         *collection_stack;
	unsigned int          collection_stack_ptr;
	unsigned int          collection_stack_size;
	struct hid_device    *device;
	unsigned int          scan_flags;
};


#define HID_STAT_PARSED BIT(1)
struct hid_device;
struct hid_driver {u8 *(*report_fixup)(struct hid_device *,u8 *,unsigned *);};
struct hid_ll_driver {unsigned max_buffer_size;};
struct hid_device {
 const u8 *dev_rdesc,*rdesc;unsigned dev_rsize,rsize;
 struct hid_collection *collection;unsigned collection_size,maxcollection,maxapplication;
 struct hid_report_enum report_enum[HID_REPORT_TYPES];
 struct hid_ll_driver *ll_driver;struct hid_driver *driver;unsigned status;struct device dev;
};
/* CONFIG_HID_BPF is not implemented/enabled: the upstream disabled path
 * is a real owned copy, not a pretend successful BPF invocation. */
static inline u8 *call_hid_bpf_rdesc_fixup(struct hid_device *d,const u8 *p,unsigned *n){return kmemdup(p,*n,GFP_KERNEL);}
s32 hid_snto32(__u32 value,unsigned n);
