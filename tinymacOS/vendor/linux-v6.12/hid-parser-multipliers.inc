static int hid_calculate_multiplier(struct hid_device *hid,
				     struct hid_field *multiplier)
{
	int m;
	__s32 v = *multiplier->value;
	__s32 lmin = multiplier->logical_minimum;
	__s32 lmax = multiplier->logical_maximum;
	__s32 pmin = multiplier->physical_minimum;
	__s32 pmax = multiplier->physical_maximum;

	/*
	 * "Because OS implementations will generally divide the control's
	 * reported count by the Effective Resolution Multiplier, designers
	 * should take care not to establish a potential Effective
	 * Resolution Multiplier of zero."
	 * HID Usage Table, v1.12, Section 4.3.1, p31
	 */
	if (lmax - lmin == 0)
		return 1;
	/*
	 * Handling the unit exponent is left as an exercise to whoever
	 * finds a device where that exponent is not 0.
	 */
	m = ((v - lmin)/(lmax - lmin) * (pmax - pmin) + pmin);
	if (unlikely(multiplier->unit_exponent != 0)) {
		hid_warn(hid,
			 "unsupported Resolution Multiplier unit exponent %d\n",
			 multiplier->unit_exponent);
	}

	/* There are no devices with an effective multiplier > 255 */
	if (unlikely(m == 0 || m > 255 || m < -255)) {
		hid_warn(hid, "unsupported Resolution Multiplier %d\n", m);
		m = 1;
	}

	return m;
}

static void hid_apply_multiplier_to_field(struct hid_device *hid,
					  struct hid_field *field,
					  struct hid_collection *multiplier_collection,
					  int effective_multiplier)
{
	struct hid_collection *collection;
	struct hid_usage *usage;
	int i;

	/*
	 * If multiplier_collection is NULL, the multiplier applies
	 * to all fields in the report.
	 * Otherwise, it is the Logical Collection the multiplier applies to
	 * but our field may be in a subcollection of that collection.
	 */
	for (i = 0; i < field->maxusage; i++) {
		usage = &field->usage[i];

		collection = &hid->collection[usage->collection_index];
		while (collection->parent_idx != -1 &&
		       collection != multiplier_collection)
			collection = &hid->collection[collection->parent_idx];

		if (collection->parent_idx != -1 ||
		    multiplier_collection == NULL)
			usage->resolution_multiplier = effective_multiplier;

	}
}

static void hid_apply_multiplier(struct hid_device *hid,
				 struct hid_field *multiplier)
{
	struct hid_report_enum *rep_enum;
	struct hid_report *rep;
	struct hid_field *field;
	struct hid_collection *multiplier_collection;
	int effective_multiplier;
	int i;

	/*
	 * "The Resolution Multiplier control must be contained in the same
	 * Logical Collection as the control(s) to which it is to be applied.
	 * If no Resolution Multiplier is defined, then the Resolution
	 * Multiplier defaults to 1.  If more than one control exists in a
	 * Logical Collection, the Resolution Multiplier is associated with
	 * all controls in the collection. If no Logical Collection is
	 * defined, the Resolution Multiplier is associated with all
	 * controls in the report."
	 * HID Usage Table, v1.12, Section 4.3.1, p30
	 *
	 * Thus, search from the current collection upwards until we find a
	 * logical collection. Then search all fields for that same parent
	 * collection. Those are the fields the multiplier applies to.
	 *
	 * If we have more than one multiplier, it will overwrite the
	 * applicable fields later.
	 */
	multiplier_collection = &hid->collection[multiplier->usage->collection_index];
	while (multiplier_collection->parent_idx != -1 &&
	       multiplier_collection->type != HID_COLLECTION_LOGICAL)
		multiplier_collection = &hid->collection[multiplier_collection->parent_idx];

	effective_multiplier = hid_calculate_multiplier(hid, multiplier);

	rep_enum = &hid->report_enum[HID_INPUT_REPORT];
	list_for_each_entry(rep, &rep_enum->report_list, list) {
		for (i = 0; i < rep->maxfield; i++) {
			field = rep->field[i];
			hid_apply_multiplier_to_field(hid, field,
						      multiplier_collection,
						      effective_multiplier);
		}
	}
}

/*
 * hid_setup_resolution_multiplier - set up all resolution multipliers
 *
 * @device: hid device
 *
 * Search for all Resolution Multiplier Feature Reports and apply their
 * value to all matching Input items. This only updates the internal struct
 * fields.
 *
 * The Resolution Multiplier is applied by the hardware. If the multiplier
 * is anything other than 1, the hardware will send pre-multiplied events
 * so that the same physical interaction generates an accumulated
 *	accumulated_value = value * * multiplier
 * This may be achieved by sending
 * - "value * multiplier" for each event, or
 * - "value" but "multiplier" times as frequently, or
 * - a combination of the above
 * The only guarantee is that the same physical interaction always generates
 * an accumulated 'value * multiplier'.
 *
 * This function must be called before any event processing and after
 * any SetRequest to the Resolution Multiplier.
 */
void hid_setup_resolution_multiplier(struct hid_device *hid)
{
	struct hid_report_enum *rep_enum;
	struct hid_report *rep;
	struct hid_usage *usage;
	int i, j;

	rep_enum = &hid->report_enum[HID_FEATURE_REPORT];
	list_for_each_entry(rep, &rep_enum->report_list, list) {
		for (i = 0; i < rep->maxfield; i++) {
			/* Ignore if report count is out of bounds. */
			if (rep->field[i]->report_count < 1)
				continue;

			for (j = 0; j < rep->field[i]->maxusage; j++) {
				usage = &rep->field[i]->usage[j];
				if (usage->hid == HID_GD_RESOLUTION_MULTIPLIER)
					hid_apply_multiplier(hid,
							     rep->field[i]);
			}
		}
	}
}
EXPORT_SYMBOL_GPL(hid_setup_resolution_multiplier);
