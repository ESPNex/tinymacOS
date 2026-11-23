static u32 s32ton(__s32 value, unsigned n)
{
	s32 a = value >> (n - 1);
	if (a && a != -1)
		return value < 0 ? 1 << (n - 1) : (1 << (n - 1)) - 1;
	return value & ((1 << n) - 1);
}

/*
 * Extract/implement a data field from/to a little endian report (bit array).
 *
 * Code sort-of follows HID spec:
 *     http://www.usb.org/developers/hidpage/HID1_11.pdf
 *
 * While the USB HID spec allows unlimited length bit fields in "report
 * descriptors", most devices never use more than 16 bits.
 * One model of UPS is claimed to report "LINEV" as a 32-bit field.
 * Search linux-kernel and linux-usb-devel archives for "hid-core extract".
 */

static u32 __extract(u8 *report, unsigned offset, int n)
{
	unsigned int idx = offset / 8;
	unsigned int bit_nr = 0;
	unsigned int bit_shift = offset % 8;
	int bits_to_copy = 8 - bit_shift;
	u32 value = 0;
	u32 mask = n < 32 ? (1U << n) - 1 : ~0U;

	while (n > 0) {
		value |= ((u32)report[idx] >> bit_shift) << bit_nr;
		n -= bits_to_copy;
		bit_nr += bits_to_copy;
		bits_to_copy = 8;
		bit_shift = 0;
		idx++;
	}

	return value & mask;
}

u32 hid_field_extract(const struct hid_device *hid, u8 *report,
			unsigned offset, unsigned n)
{
	if (n > 32) {
		hid_warn_once(hid, "%s() called with n (%d) > 32! (%s)\n",
			      __func__, n, current->comm);
		n = 32;
	}

	return __extract(report, offset, n);
}
EXPORT_SYMBOL_GPL(hid_field_extract);

/*
 * "implement" : set bits in a little endian bit stream.
 * Same concepts as "extract" (see comments above).
 * The data mangled in the bit stream remains in little endian
 * order the whole time. It make more sense to talk about
 * endianness of register values by considering a register
 * a "cached" copy of the little endian bit stream.
 */

static void __implement(u8 *report, unsigned offset, int n, u32 value)
{
	unsigned int idx = offset / 8;
	unsigned int bit_shift = offset % 8;
	int bits_to_set = 8 - bit_shift;

	while (n - bits_to_set >= 0) {
		report[idx] &= ~(0xff << bit_shift);
		report[idx] |= value << bit_shift;
		value >>= bits_to_set;
		n -= bits_to_set;
		bits_to_set = 8;
		bit_shift = 0;
		idx++;
	}

	/* last nibble */
	if (n) {
		u8 bit_mask = ((1U << n) - 1);
		report[idx] &= ~(bit_mask << bit_shift);
		report[idx] |= value << bit_shift;
	}
}

static void implement(const struct hid_device *hid, u8 *report,
		      unsigned offset, unsigned n, u32 value)
{
	if (unlikely(n > 32)) {
		hid_warn(hid, "%s() called with n (%d) > 32! (%s)\n",
			 __func__, n, current->comm);
		n = 32;
	} else if (n < 32) {
		u32 m = (1U << n) - 1;

		if (unlikely(value > m)) {
			hid_warn(hid,
				 "%s() called with too large value %d (n: %d)! (%s)\n",
				 __func__, value, n, current->comm);
			value &= m;
		}
	}

	__implement(report, offset, n, value);
}
