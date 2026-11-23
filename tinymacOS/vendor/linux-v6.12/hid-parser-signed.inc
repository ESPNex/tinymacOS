static s32 snto32(__u32 value, unsigned n)
{
	if (!value || !n)
		return 0;

	if (n > 32)
		n = 32;

	switch (n) {
	case 8:  return ((__s8)value);
	case 16: return ((__s16)value);
	case 32: return ((__s32)value);
	}
	return value & (1 << (n - 1)) ? value | (~0U << n) : value;
}

s32 hid_snto32(__u32 value, unsigned n)
{
	return snto32(value, n);
}
EXPORT_SYMBOL_GPL(hid_snto32);
