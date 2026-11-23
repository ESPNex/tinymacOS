struct hid_report *hid_register_report(struct hid_device *device,
				       enum hid_report_type type, unsigned int id,
				       unsigned int application)
{
	struct hid_report_enum *report_enum = device->report_enum + type;
	struct hid_report *report;

	if (id >= HID_MAX_IDS)
		return NULL;
	if (report_enum->report_id_hash[id])
		return report_enum->report_id_hash[id];

	report = kzalloc(sizeof(struct hid_report), GFP_KERNEL);
	if (!report)
		return NULL;

	if (id != 0)
		report_enum->numbered = 1;

	report->id = id;
	report->type = type;
	report->size = 0;
	report->device = device;
	report->application = application;
	report_enum->report_id_hash[id] = report;

	list_add_tail(&report->list, &report_enum->report_list);
	INIT_LIST_HEAD(&report->field_entry_list);

	return report;
}
EXPORT_SYMBOL_GPL(hid_register_report);

/*
 * Register a new field for this report.
 */

static struct hid_field *hid_register_field(struct hid_report *report, unsigned usages)
{
	struct hid_field *field;

	if (report->maxfield == HID_MAX_FIELDS) {
		hid_err(report->device, "too many fields in report\n");
		return NULL;
	}

	field = kvzalloc((sizeof(struct hid_field) +
			  usages * sizeof(struct hid_usage) +
			  3 * usages * sizeof(unsigned int)), GFP_KERNEL);
	if (!field)
		return NULL;

	field->index = report->maxfield++;
	report->field[field->index] = field;
	field->usage = (struct hid_usage *)(field + 1);
	field->value = (s32 *)(field->usage + usages);
	field->new_value = (s32 *)(field->value + usages);
	field->usages_priorities = (s32 *)(field->new_value + usages);
	field->report = report;

	return field;
}

/*
 * Open a collection. The type/usage is pushed on the stack.
 */

static int open_collection(struct hid_parser *parser, unsigned type)
{
	struct hid_collection *collection;
	unsigned usage;
	int collection_index;

	usage = parser->local.usage[0];

	if (parser->collection_stack_ptr == parser->collection_stack_size) {
		unsigned int *collection_stack;
		unsigned int new_size = parser->collection_stack_size +
					HID_COLLECTION_STACK_SIZE;

		collection_stack = krealloc(parser->collection_stack,
					    new_size * sizeof(unsigned int),
					    GFP_KERNEL);
		if (!collection_stack)
			return -ENOMEM;

		parser->collection_stack = collection_stack;
		parser->collection_stack_size = new_size;
	}

	if (parser->device->maxcollection == parser->device->collection_size) {
		collection = kmalloc(
				array3_size(sizeof(struct hid_collection),
					    parser->device->collection_size,
					    2),
				GFP_KERNEL);
		if (collection == NULL) {
			hid_err(parser->device, "failed to reallocate collection array\n");
			return -ENOMEM;
		}
		memcpy(collection, parser->device->collection,
			sizeof(struct hid_collection) *
			parser->device->collection_size);
		memset(collection + parser->device->collection_size, 0,
			sizeof(struct hid_collection) *
			parser->device->collection_size);
		kfree(parser->device->collection);
		parser->device->collection = collection;
		parser->device->collection_size *= 2;
	}

	parser->collection_stack[parser->collection_stack_ptr++] =
		parser->device->maxcollection;

	collection_index = parser->device->maxcollection++;
	collection = parser->device->collection + collection_index;
	collection->type = type;
	collection->usage = usage;
	collection->level = parser->collection_stack_ptr - 1;
	collection->parent_idx = (collection->level == 0) ? -1 :
		parser->collection_stack[collection->level - 1];

	if (type == HID_COLLECTION_APPLICATION)
		parser->device->maxapplication++;

	return 0;
}

/*
 * Close a collection.
 */

static int close_collection(struct hid_parser *parser)
{
	if (!parser->collection_stack_ptr) {
		hid_err(parser->device, "collection stack underflow\n");
		return -EINVAL;
	}
	parser->collection_stack_ptr--;
	return 0;
}

/*
 * Climb up the stack, search for the specified collection type
 * and return the usage.
 */

static unsigned hid_lookup_collection(struct hid_parser *parser, unsigned type)
{
	struct hid_collection *collection = parser->device->collection;
	int n;

	for (n = parser->collection_stack_ptr - 1; n >= 0; n--) {
		unsigned index = parser->collection_stack[n];
		if (collection[index].type == type)
			return collection[index].usage;
	}
	return 0; /* we know nothing about this usage type */
}

/*
 * Concatenate usage which defines 16 bits or less with the
 * currently defined usage page to form a 32 bit usage
 */

static void complete_usage(struct hid_parser *parser, unsigned int index)
{
	parser->local.usage[index] &= 0xFFFF;
	parser->local.usage[index] |=
		(parser->global.usage_page & 0xFFFF) << 16;
}

/*
 * Add a usage to the temporary parser table.
 */

static int hid_add_usage(struct hid_parser *parser, unsigned usage, u8 size)
{
	if (parser->local.usage_index >= HID_MAX_USAGES) {
		hid_err(parser->device, "usage index exceeded\n");
		return -1;
	}
	parser->local.usage[parser->local.usage_index] = usage;

	/*
	 * If Usage item only includes usage id, concatenate it with
	 * currently defined usage page
	 */
	if (size <= 2)
		complete_usage(parser, parser->local.usage_index);

	parser->local.usage_size[parser->local.usage_index] = size;
	parser->local.collection_index[parser->local.usage_index] =
		parser->collection_stack_ptr ?
		parser->collection_stack[parser->collection_stack_ptr - 1] : 0;
	parser->local.usage_index++;
	return 0;
}

/*
 * Register a new field for this report.
 */

static int hid_add_field(struct hid_parser *parser, unsigned report_type, unsigned flags)
{
	struct hid_report *report;
	struct hid_field *field;
	unsigned int max_buffer_size = HID_MAX_BUFFER_SIZE;
	unsigned int usages;
	unsigned int offset;
	unsigned int i;
	unsigned int application;

	application = hid_lookup_collection(parser, HID_COLLECTION_APPLICATION);

	report = hid_register_report(parser->device, report_type,
				     parser->global.report_id, application);
	if (!report) {
		hid_err(parser->device, "hid_register_report failed\n");
		return -1;
	}

	/* Handle both signed and unsigned cases properly */
	if ((parser->global.logical_minimum < 0 &&
		parser->global.logical_maximum <
		parser->global.logical_minimum) ||
		(parser->global.logical_minimum >= 0 &&
		(__u32)parser->global.logical_maximum <
		(__u32)parser->global.logical_minimum)) {
		dbg_hid("logical range invalid 0x%x 0x%x\n",
			parser->global.logical_minimum,
			parser->global.logical_maximum);
		return -1;
	}

	offset = report->size;
	report->size += parser->global.report_size * parser->global.report_count;

	if (parser->device->ll_driver->max_buffer_size)
		max_buffer_size = parser->device->ll_driver->max_buffer_size;

	/* Total size check: Allow for possible report index byte */
	if (report->size > (max_buffer_size - 1) << 3) {
		hid_err(parser->device, "report is too long\n");
		return -1;
	}

	if (!parser->local.usage_index) /* Ignore padding fields */
		return 0;

	usages = max_t(unsigned, parser->local.usage_index,
				 parser->global.report_count);

	field = hid_register_field(report, usages);
	if (!field)
		return 0;

	field->physical = hid_lookup_collection(parser, HID_COLLECTION_PHYSICAL);
	field->logical = hid_lookup_collection(parser, HID_COLLECTION_LOGICAL);
	field->application = application;

	for (i = 0; i < usages; i++) {
		unsigned j = i;
		/* Duplicate the last usage we parsed if we have excess values */
		if (i >= parser->local.usage_index)
			j = parser->local.usage_index - 1;
		field->usage[i].hid = parser->local.usage[j];
		field->usage[i].collection_index =
			parser->local.collection_index[j];
		field->usage[i].usage_index = i;
		field->usage[i].resolution_multiplier = 1;
	}

	field->maxusage = usages;
	field->flags = flags;
	field->report_offset = offset;
	field->report_type = report_type;
	field->report_size = parser->global.report_size;
	field->report_count = parser->global.report_count;
	field->logical_minimum = parser->global.logical_minimum;
	field->logical_maximum = parser->global.logical_maximum;
	field->physical_minimum = parser->global.physical_minimum;
	field->physical_maximum = parser->global.physical_maximum;
	field->unit_exponent = parser->global.unit_exponent;
	field->unit = parser->global.unit;

	return 0;
}

/*
 * Read data value from item.
 */

static u32 item_udata(struct hid_item *item)
{
	switch (item->size) {
	case 1: return item->data.u8;
	case 2: return item->data.u16;
	case 4: return item->data.u32;
	}
	return 0;
}

static s32 item_sdata(struct hid_item *item)
{
	switch (item->size) {
	case 1: return item->data.s8;
	case 2: return item->data.s16;
	case 4: return item->data.s32;
	}
	return 0;
}

/*
 * Process a global item.
 */

static int hid_parser_global(struct hid_parser *parser, struct hid_item *item)
{
	__s32 raw_value;
	switch (item->tag) {
	case HID_GLOBAL_ITEM_TAG_PUSH:

		if (parser->global_stack_ptr == HID_GLOBAL_STACK_SIZE) {
			hid_err(parser->device, "global environment stack overflow\n");
			return -1;
		}

		memcpy(parser->global_stack + parser->global_stack_ptr++,
			&parser->global, sizeof(struct hid_global));
		return 0;

	case HID_GLOBAL_ITEM_TAG_POP:

		if (!parser->global_stack_ptr) {
			hid_err(parser->device, "global environment stack underflow\n");
			return -1;
		}

		memcpy(&parser->global, parser->global_stack +
			--parser->global_stack_ptr, sizeof(struct hid_global));
		return 0;

	case HID_GLOBAL_ITEM_TAG_USAGE_PAGE:
		parser->global.usage_page = item_udata(item);
		return 0;

	case HID_GLOBAL_ITEM_TAG_LOGICAL_MINIMUM:
		parser->global.logical_minimum = item_sdata(item);
		return 0;

	case HID_GLOBAL_ITEM_TAG_LOGICAL_MAXIMUM:
		if (parser->global.logical_minimum < 0)
			parser->global.logical_maximum = item_sdata(item);
		else
			parser->global.logical_maximum = item_udata(item);
		return 0;

	case HID_GLOBAL_ITEM_TAG_PHYSICAL_MINIMUM:
		parser->global.physical_minimum = item_sdata(item);
		return 0;

	case HID_GLOBAL_ITEM_TAG_PHYSICAL_MAXIMUM:
		if (parser->global.physical_minimum < 0)
			parser->global.physical_maximum = item_sdata(item);
		else
			parser->global.physical_maximum = item_udata(item);
		return 0;

	case HID_GLOBAL_ITEM_TAG_UNIT_EXPONENT:
		/* Many devices provide unit exponent as a two's complement
		 * nibble due to the common misunderstanding of HID
		 * specification 1.11, 6.2.2.7 Global Items. Attempt to handle
		 * both this and the standard encoding. */
		raw_value = item_sdata(item);
		if (!(raw_value & 0xfffffff0))
			parser->global.unit_exponent = hid_snto32(raw_value, 4);
		else
			parser->global.unit_exponent = raw_value;
		return 0;

	case HID_GLOBAL_ITEM_TAG_UNIT:
		parser->global.unit = item_udata(item);
		return 0;

	case HID_GLOBAL_ITEM_TAG_REPORT_SIZE:
		parser->global.report_size = item_udata(item);
		if (parser->global.report_size > 256) {
			hid_err(parser->device, "invalid report_size %d\n",
					parser->global.report_size);
			return -1;
		}
		return 0;

	case HID_GLOBAL_ITEM_TAG_REPORT_COUNT:
		parser->global.report_count = item_udata(item);
		if (parser->global.report_count > HID_MAX_USAGES) {
			hid_err(parser->device, "invalid report_count %d\n",
					parser->global.report_count);
			return -1;
		}
		return 0;

	case HID_GLOBAL_ITEM_TAG_REPORT_ID:
		parser->global.report_id = item_udata(item);
		if (parser->global.report_id == 0 ||
		    parser->global.report_id >= HID_MAX_IDS) {
			hid_err(parser->device, "report_id %u is invalid\n",
				parser->global.report_id);
			return -1;
		}
		return 0;

	default:
		hid_err(parser->device, "unknown global tag 0x%x\n", item->tag);
		return -1;
	}
}

/*
 * Process a local item.
 */

static int hid_parser_local(struct hid_parser *parser, struct hid_item *item)
{
	__u32 data;
	unsigned n;
	__u32 count;

	data = item_udata(item);

	switch (item->tag) {
	case HID_LOCAL_ITEM_TAG_DELIMITER:

		if (data) {
			/*
			 * We treat items before the first delimiter
			 * as global to all usage sets (branch 0).
			 * In the moment we process only these global
			 * items and the first delimiter set.
			 */
			if (parser->local.delimiter_depth != 0) {
				hid_err(parser->device, "nested delimiters\n");
				return -1;
			}
			parser->local.delimiter_depth++;
			parser->local.delimiter_branch++;
		} else {
			if (parser->local.delimiter_depth < 1) {
				hid_err(parser->device, "bogus close delimiter\n");
				return -1;
			}
			parser->local.delimiter_depth--;
		}
		return 0;

	case HID_LOCAL_ITEM_TAG_USAGE:

		if (parser->local.delimiter_branch > 1) {
			dbg_hid("alternative usage ignored\n");
			return 0;
		}

		return hid_add_usage(parser, data, item->size);

	case HID_LOCAL_ITEM_TAG_USAGE_MINIMUM:

		if (parser->local.delimiter_branch > 1) {
			dbg_hid("alternative usage ignored\n");
			return 0;
		}

		parser->local.usage_minimum = data;
		return 0;

	case HID_LOCAL_ITEM_TAG_USAGE_MAXIMUM:

		if (parser->local.delimiter_branch > 1) {
			dbg_hid("alternative usage ignored\n");
			return 0;
		}

		count = data - parser->local.usage_minimum;
		if (count + parser->local.usage_index >= HID_MAX_USAGES) {
			/*
			 * We do not warn if the name is not set, we are
			 * actually pre-scanning the device.
			 */
			if (dev_name(&parser->device->dev))
				hid_warn(parser->device,
					 "ignoring exceeding usage max\n");
			data = HID_MAX_USAGES - parser->local.usage_index +
				parser->local.usage_minimum - 1;
			if (data <= 0) {
				hid_err(parser->device,
					"no more usage index available\n");
				return -1;
			}
		}

		for (n = parser->local.usage_minimum; n <= data; n++)
			if (hid_add_usage(parser, n, item->size)) {
				dbg_hid("hid_add_usage failed\n");
				return -1;
			}
		return 0;

	default:

		dbg_hid("unknown local item tag 0x%x\n", item->tag);
		return 0;
	}
	return 0;
}

/*
 * Concatenate Usage Pages into Usages where relevant:
 * As per specification, 6.2.2.8: "When the parser encounters a main item it
 * concatenates the last declared Usage Page with a Usage to form a complete
 * usage value."
 */

static void hid_concatenate_last_usage_page(struct hid_parser *parser)
{
	int i;
	unsigned int usage_page;
	unsigned int current_page;

	if (!parser->local.usage_index)
		return;

	usage_page = parser->global.usage_page;

	/*
	 * Concatenate usage page again only if last declared Usage Page
	 * has not been already used in previous usages concatenation
	 */
	for (i = parser->local.usage_index - 1; i >= 0; i--) {
		if (parser->local.usage_size[i] > 2)
			/* Ignore extended usages */
			continue;

		current_page = parser->local.usage[i] >> 16;
		if (current_page == usage_page)
			break;

		complete_usage(parser, i);
	}
}

/*
 * Process a main item.
 */

static int hid_parser_main(struct hid_parser *parser, struct hid_item *item)
{
	__u32 data;
	int ret;

	hid_concatenate_last_usage_page(parser);

	data = item_udata(item);

	switch (item->tag) {
	case HID_MAIN_ITEM_TAG_BEGIN_COLLECTION:
		ret = open_collection(parser, data & 0xff);
		break;
	case HID_MAIN_ITEM_TAG_END_COLLECTION:
		ret = close_collection(parser);
		break;
	case HID_MAIN_ITEM_TAG_INPUT:
		ret = hid_add_field(parser, HID_INPUT_REPORT, data);
		break;
	case HID_MAIN_ITEM_TAG_OUTPUT:
		ret = hid_add_field(parser, HID_OUTPUT_REPORT, data);
		break;
	case HID_MAIN_ITEM_TAG_FEATURE:
		ret = hid_add_field(parser, HID_FEATURE_REPORT, data);
		break;
	default:
		hid_warn(parser->device, "unknown main item tag 0x%x\n", item->tag);
		ret = 0;
	}

	memset(&parser->local, 0, sizeof(parser->local));	/* Reset the local parser environment */

	return ret;
}

/*
 * Process a reserved item.
 */

static int hid_parser_reserved(struct hid_parser *parser, struct hid_item *item)
{
	dbg_hid("reserved item type, tag 0x%x\n", item->tag);
	return 0;
}

/*
 * Free a report and all registered fields. The field->usage and
 * field->value table's are allocated behind the field, so we need
 * only to free(field) itself.
 */

static void hid_free_report(struct hid_report *report)
{
	unsigned n;

	kfree(report->field_entries);

	for (n = 0; n < report->maxfield; n++)
		kvfree(report->field[n]);
	kfree(report);
}

/*
 * Close report. This function returns the device
 * state to the point prior to hid_open_report().
 */
static void hid_close_report(struct hid_device *device)
{
	unsigned i, j;

	for (i = 0; i < HID_REPORT_TYPES; i++) {
		struct hid_report_enum *report_enum = device->report_enum + i;

		for (j = 0; j < HID_MAX_IDS; j++) {
			struct hid_report *report = report_enum->report_id_hash[j];
			if (report)
				hid_free_report(report);
		}
		memset(report_enum, 0, sizeof(*report_enum));
		INIT_LIST_HEAD(&report_enum->report_list);
	}

	kfree(device->rdesc);
	device->rdesc = NULL;
	device->rsize = 0;

	kfree(device->collection);
	device->collection = NULL;
	device->collection_size = 0;
	device->maxcollection = 0;
	device->maxapplication = 0;

	device->status &= ~HID_STAT_PARSED;
}
