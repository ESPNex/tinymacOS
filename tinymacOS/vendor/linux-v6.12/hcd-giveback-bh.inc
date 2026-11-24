static void usb_giveback_urb_bh(struct work_struct *work)
{
	struct giveback_urb_bh *bh =
		container_of(work, struct giveback_urb_bh, bh);
	struct list_head local_list;

	spin_lock_irq(&bh->lock);
	bh->running = true;
	list_replace_init(&bh->head, &local_list);
	spin_unlock_irq(&bh->lock);

	while (!list_empty(&local_list)) {
		struct urb *urb;

		urb = list_entry(local_list.next, struct urb, urb_list);
		list_del_init(&urb->urb_list);
		bh->completing_ep = urb->ep;
		__usb_hcd_giveback_urb(urb);
		bh->completing_ep = NULL;
	}

	/*
	 * giveback new URBs next time to prevent this function
	 * from not exiting for a long time.
	 */
	spin_lock_irq(&bh->lock);
	if (!list_empty(&bh->head)) {
		if (bh->high_prio)
			queue_work(system_bh_highpri_wq, &bh->bh);
		else
			queue_work(system_bh_wq, &bh->bh);
	}
	bh->running = false;
	spin_unlock_irq(&bh->lock);
}

/**
 * usb_hcd_giveback_urb - return URB from HCD to device driver
 * @hcd: host controller returning the URB
 * @urb: urb being returned to the USB device driver.
 * @status: completion status code for the URB.
 *
 * Context: atomic. The completion callback is invoked in caller's context.
 * For HCDs with HCD_BH flag set, the completion callback is invoked in BH
 * context (except for URBs submitted to the root hub which always complete in
 * caller's context).
 *
 * This hands the URB from HCD to its USB device driver, using its
 * completion function.  The HCD has freed all per-urb resources
 * (and is done using urb->hcpriv).  It also released all HCD locks;
 * the device driver won't cause problems if it frees, modifies,
 * or resubmits this URB.
 *
 * If @urb was unlinked, the value of @status will be overridden by
 * @urb->unlinked.  Erroneous short transfers are detected in case
 * the HCD hasn't checked for them.
 */
void usb_hcd_giveback_urb(struct usb_hcd *hcd, struct urb *urb, int status)
{
	struct giveback_urb_bh *bh;
	bool running;

	/* pass status to BH via unlinked */
	if (likely(!urb->unlinked))
		urb->unlinked = status;

	if (!hcd_giveback_urb_in_bh(hcd) && !is_root_hub(urb->dev)) {
		__usb_hcd_giveback_urb(urb);
		return;
	}

	if (usb_pipeisoc(urb->pipe) || usb_pipeint(urb->pipe))
		bh = &hcd->high_prio_bh;
	else
		bh = &hcd->low_prio_bh;

	spin_lock(&bh->lock);
	list_add_tail(&urb->urb_list, &bh->head);
	running = bh->running;
	spin_unlock(&bh->lock);

	if (running)
		;
	else if (bh->high_prio)
		queue_work(system_bh_highpri_wq, &bh->bh);
	else
		queue_work(system_bh_wq, &bh->bh);
}