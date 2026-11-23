static void hid_output_field(const struct hid_device *hid,
			     struct hid_field *field, __u8 *data)
{
	unsigned count = field->report_count;
	unsigned offset = field->report_offset;
	unsigned size = field->report_size;
	unsigned n;

	for (n = 0; n < count; n++) {
		if (field->logical_minimum < 0)	/* signed values */
			implement(hid, data, offset + n * size, size,
				  s32ton(field->value[n], size));
		else				/* unsigned values */
			implement(hid, data, offset + n * size, size,
				  field->value[n]);
	}
}

/*
 * Compute the size of a report.
 */
static size_t hid_compute_report_size(struct hid_report *report)
{
	if (report->size)
		return ((report->size - 1) >> 3) + 1;

	return 0;
}

/*
 * Create a report. 'data' has to be allocated using
 * hid_alloc_report_buf() so that it has proper size.
 */

void hid_output_report(struct hid_report *report, __u8 *data)
{
	unsigned n;

	if (report->id > 0)
		*data++ = report->id;

	memset(data, 0, hid_compute_report_size(report));
	for (n = 0; n < report->maxfield; n++)
		hid_output_field(report->device, report->field[n], data);
}
EXPORT_SYMBOL_GPL(hid_output_report);

/*
 * Allocator for buffer that is going to be passed to hid_output_report()
 */
u8 *hid_alloc_report_buf(struct hid_report *report, gfp_t flags)
{
	/*
	 * 7 extra bytes are necessary to achieve proper functionality
	 * of implement() working on 8 byte chunks
	 */

	u32 len = hid_report_len(report) + 7;

	return kzalloc(len, flags);
}
EXPORT_SYMBOL_GPL(hid_alloc_report_buf);

/*
 * Set a field value. The report this field belongs to has to be
 * created and transferred to the device, to set this value in the
 * device.
 */

int hid_set_field(struct hid_field *field, unsigned offset, __s32 value)
{
	unsigned size;

	if (!field)
		return -1;

	size = field->report_size;

	hid_dump_input(field->report->device, field->usage + offset, value);

	if (offset >= field->report_count) {
		hid_err(field->report->device, "offset (%d) exceeds report_count (%d)\n",
				offset, field->report_count);
		return -1;
	}
	if (field->logical_minimum < 0) {
		if (value != snto32(s32ton(value, size), size)) {
			hid_err(field->report->device, "value %d is out of range\n", value);
			return -1;
		}
	}
	field->value[offset] = value;
	return 0;
}
EXPORT_SYMBOL_GPL(hid_set_field);
