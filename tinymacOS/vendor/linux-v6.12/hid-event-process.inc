static int search(__s32 *array, __s32 value, unsigned n)
{
	while (n--) {
		if (*array++ == value)
			return 0;
	}
	return -1;
}

/**
 * hid_match_report - check if driver's raw_event should be called
 *
 * @hid: hid device
 * @report: hid report to match against
 *
 * compare hid->driver->report_table->report_type to report->type
 */
static int hid_match_report(struct hid_device *hid, struct hid_report *report)
{
	const struct hid_report_id *id = hid->driver->report_table;

	if (!id) /* NULL means all */
		return 1;

	for (; id->report_type != HID_TERMINATOR; id++)
		if (id->report_type == HID_ANY_ID ||
				id->report_type == report->type)
			return 1;
	return 0;
}

/**
 * hid_match_usage - check if driver's event should be called
 *
 * @hid: hid device
 * @usage: usage to match against
 *
 * compare hid->driver->usage_table->usage_{type,code} to
 * usage->usage_{type,code}
 */
static int hid_match_usage(struct hid_device *hid, struct hid_usage *usage)
{
	const struct hid_usage_id *id = hid->driver->usage_table;

	if (!id) /* NULL means all */
		return 1;

	for (; id->usage_type != HID_ANY_ID - 1; id++)
		if ((id->usage_hid == HID_ANY_ID ||
				id->usage_hid == usage->hid) &&
				(id->usage_type == HID_ANY_ID ||
				id->usage_type == usage->type) &&
				(id->usage_code == HID_ANY_ID ||
				 id->usage_code == usage->code))
			return 1;
	return 0;
}

static void hid_process_event(struct hid_device *hid, struct hid_field *field,
		struct hid_usage *usage, __s32 value, int interrupt)
{
	struct hid_driver *hdrv = hid->driver;
	int ret;

	if (!list_empty(&hid->debug_list))
		hid_dump_input(hid, usage, value);

	if (hdrv && hdrv->event && hid_match_usage(hid, usage)) {
		ret = hdrv->event(hid, field, usage, value);
		if (ret != 0) {
			if (ret < 0)
				hid_err(hid, "%s's event failed with %d\n",
						hdrv->name, ret);
			return;
		}
	}

	if (hid->claimed & HID_CLAIMED_INPUT)
		hidinput_hid_event(hid, field, usage, value);
	if (hid->claimed & HID_CLAIMED_HIDDEV && interrupt && hid->hiddev_hid_event)
		hid->hiddev_hid_event(hid, field, usage, value);
}

/*
 * Checks if the given value is valid within this field
 */
static inline int hid_array_value_is_valid(struct hid_field *field,
					   __s32 value)
{
	__s32 min = field->logical_minimum;

	/*
	 * Value needs to be between logical min and max, and
	 * (value - min) is used as an index in the usage array.
	 * This array is of size field->maxusage
	 */
	return value >= min &&
	       value <= field->logical_maximum &&
	       value - min < field->maxusage;
}

/*
 * Fetch the field from the data. The field content is stored for next
 * report processing (we do differential reporting to the layer).
 */
static void hid_input_fetch_field(struct hid_device *hid,
				  struct hid_field *field,
				  __u8 *data)
{
	unsigned n;
	unsigned count = field->report_count;
	unsigned offset = field->report_offset;
	unsigned size = field->report_size;
	__s32 min = field->logical_minimum;
	__s32 *value;

	value = field->new_value;
	memset(value, 0, count * sizeof(__s32));
	field->ignored = false;

	for (n = 0; n < count; n++) {

		value[n] = min < 0 ?
			snto32(hid_field_extract(hid, data, offset + n * size,
			       size), size) :
			hid_field_extract(hid, data, offset + n * size, size);

		/* Ignore report if ErrorRollOver */
		if (!(field->flags & HID_MAIN_ITEM_VARIABLE) &&
		    hid_array_value_is_valid(field, value[n]) &&
		    field->usage[value[n] - min].hid == HID_UP_KEYBOARD + 1) {
			field->ignored = true;
			return;
		}
	}
}

/*
 * Process a received variable field.
 */

static void hid_input_var_field(struct hid_device *hid,
				struct hid_field *field,
				int interrupt)
{
	unsigned int count = field->report_count;
	__s32 *value = field->new_value;
	unsigned int n;

	for (n = 0; n < count; n++)
		hid_process_event(hid,
				  field,
				  &field->usage[n],
				  value[n],
				  interrupt);

	memcpy(field->value, value, count * sizeof(__s32));
}

/*
 * Process a received array field. The field content is stored for
 * next report processing (we do differential reporting to the layer).
 */

static void hid_input_array_field(struct hid_device *hid,
				  struct hid_field *field,
				  int interrupt)
{
	unsigned int n;
	unsigned int count = field->report_count;
	__s32 min = field->logical_minimum;
	__s32 *value;

	value = field->new_value;

	/* ErrorRollOver */
	if (field->ignored)
		return;

	for (n = 0; n < count; n++) {
		if (hid_array_value_is_valid(field, field->value[n]) &&
		    search(value, field->value[n], count))
			hid_process_event(hid,
					  field,
					  &field->usage[field->value[n] - min],
					  0,
					  interrupt);

		if (hid_array_value_is_valid(field, value[n]) &&
		    search(field->value, value[n], count))
			hid_process_event(hid,
					  field,
					  &field->usage[value[n] - min],
					  1,
					  interrupt);
	}

	memcpy(field->value, value, count * sizeof(__s32));
}

/*
 * Analyse a received report, and fetch the data from it. The field
 * content is stored for next report processing (we do differential
 * reporting to the layer).
 */
static void hid_process_report(struct hid_device *hid,
			       struct hid_report *report,
			       __u8 *data,
			       int interrupt)
{
	unsigned int a;
	struct hid_field_entry *entry;
	struct hid_field *field;

	/* first retrieve all incoming values in data */
	for (a = 0; a < report->maxfield; a++)
		hid_input_fetch_field(hid, report->field[a], data);

	if (!list_empty(&report->field_entry_list)) {
		/* INPUT_REPORT, we have a priority list of fields */
		list_for_each_entry(entry,
				    &report->field_entry_list,
				    list) {
			field = entry->field;

			if (field->flags & HID_MAIN_ITEM_VARIABLE)
				hid_process_event(hid,
						  field,
						  &field->usage[entry->index],
						  field->new_value[entry->index],
						  interrupt);
			else
				hid_input_array_field(hid, field, interrupt);
		}

		/* we need to do the memcpy at the end for var items */
		for (a = 0; a < report->maxfield; a++) {
			field = report->field[a];

			if (field->flags & HID_MAIN_ITEM_VARIABLE)
				memcpy(field->value, field->new_value,
				       field->report_count * sizeof(__s32));
		}
	} else {
		/* FEATURE_REPORT, regular processing */
		for (a = 0; a < report->maxfield; a++) {
			field = report->field[a];

			if (field->flags & HID_MAIN_ITEM_VARIABLE)
				hid_input_var_field(hid, field, interrupt);
			else
				hid_input_array_field(hid, field, interrupt);
		}
	}
}

/*
 * Insert a given usage_index in a field in the list
 * of processed usages in the report.
 *
 * The elements of lower priority score are processed
 * first.
 */
static void __hid_insert_field_entry(struct hid_device *hid,
				     struct hid_report *report,
				     struct hid_field_entry *entry,
				     struct hid_field *field,
				     unsigned int usage_index)
{
	struct hid_field_entry *next;

	entry->field = field;
	entry->index = usage_index;
	entry->priority = field->usages_priorities[usage_index];

	/* insert the element at the correct position */
	list_for_each_entry(next,
			    &report->field_entry_list,
			    list) {
		/*
		 * the priority of our element is strictly higher
		 * than the next one, insert it before
		 */
		if (entry->priority > next->priority) {
			list_add_tail(&entry->list, &next->list);
			return;
		}
	}

	/* lowest priority score: insert at the end */
	list_add_tail(&entry->list, &report->field_entry_list);
}

static void hid_report_process_ordering(struct hid_device *hid,
					struct hid_report *report)
{
	struct hid_field *field;
	struct hid_field_entry *entries;
	unsigned int a, u, usages;
	unsigned int count = 0;

	/* count the number of individual fields in the report */
	for (a = 0; a < report->maxfield; a++) {
		field = report->field[a];

		if (field->flags & HID_MAIN_ITEM_VARIABLE)
			count += field->report_count;
		else
			count++;
	}

	/* allocate the memory to process the fields */
	entries = kcalloc(count, sizeof(*entries), GFP_KERNEL);
	if (!entries)
		return;

	report->field_entries = entries;

	/*
	 * walk through all fields in the report and
	 * store them by priority order in report->field_entry_list
	 *
	 * - Var elements are individualized (field + usage_index)
	 * - Arrays are taken as one, we can not chose an order for them
	 */
	usages = 0;
	for (a = 0; a < report->maxfield; a++) {
		field = report->field[a];

		if (field->flags & HID_MAIN_ITEM_VARIABLE) {
			for (u = 0; u < field->report_count; u++) {
				__hid_insert_field_entry(hid, report,
							 &entries[usages],
							 field, u);
				usages++;
			}
		} else {
			__hid_insert_field_entry(hid, report, &entries[usages],
						 field, 0);
			usages++;
		}
	}
}

static void hid_process_ordering(struct hid_device *hid)
{
	struct hid_report *report;
	struct hid_report_enum *report_enum = &hid->report_enum[HID_INPUT_REPORT];

	list_for_each_entry(report, &report_enum->report_list, list)
		hid_report_process_ordering(hid, report);
}
