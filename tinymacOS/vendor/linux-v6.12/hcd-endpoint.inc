int usb_hcd_link_urb_to_ep(struct usb_hcd *hcd, struct urb *urb)
{
	int		rc = 0;

	spin_lock(&hcd_urb_list_lock);

	/* Check that the URB isn't being killed */
	if (unlikely(atomic_read(&urb->reject))) {
		rc = -EPERM;
		goto done;
	}

	if (unlikely(!urb->ep->enabled)) {
		rc = -ENOENT;
		goto done;
	}

	if (unlikely(!urb->dev->can_submit)) {
		rc = -EHOSTUNREACH;
		goto done;
	}

	/*
	 * Check the host controller's state and add the URB to the
	 * endpoint's queue.
	 */
	if (HCD_RH_RUNNING(hcd)) {
		urb->unlinked = 0;
		list_add_tail(&urb->urb_list, &urb->ep->urb_list);
	} else {
		rc = -ESHUTDOWN;
		goto done;
	}
 done:
	spin_unlock(&hcd_urb_list_lock);
	return rc;
}
EXPORT_SYMBOL_GPL(usb_hcd_link_urb_to_ep);

/**
 * usb_hcd_check_unlink_urb - check whether an URB may be unlinked
 * @hcd: host controller to which @urb was submitted
 * @urb: URB being checked for unlinkability
 * @status: error code to store in @urb if the unlink succeeds
 *
 * Host controller drivers should call this routine in their dequeue()
 * method.  The HCD's private spinlock must be held and interrupts must
 * be disabled.  The actions carried out here are required for making
 * sure than an unlink is valid.
 *
 * Return: 0 for no error, otherwise a negative error code (in which case
 * the dequeue() method must fail).  The possible error codes are:
 *
 *	-EIDRM: @urb was not submitted or has already completed.
 *		The completion function may not have been called yet.
 *
 *	-EBUSY: @urb has already been unlinked.
 */
int usb_hcd_check_unlink_urb(struct usb_hcd *hcd, struct urb *urb,
		int status)
{
	struct list_head	*tmp;

	/* insist the urb is still queued */
	list_for_each(tmp, &urb->ep->urb_list) {
		if (tmp == &urb->urb_list)
			break;
	}
	if (tmp != &urb->urb_list)
		return -EIDRM;

	/* Any status except -EINPROGRESS means something already started to
	 * unlink this URB from the hardware.  So there's no more work to do.
	 */
	if (urb->unlinked)
		return -EBUSY;
	urb->unlinked = status;
	return 0;
}
EXPORT_SYMBOL_GPL(usb_hcd_check_unlink_urb);

/**
 * usb_hcd_unlink_urb_from_ep - remove an URB from its endpoint queue
 * @hcd: host controller to which @urb was submitted
 * @urb: URB being unlinked
 *
 * Host controller drivers should call this routine before calling
 * usb_hcd_giveback_urb().  The HCD's private spinlock must be held and
 * interrupts must be disabled.  The actions carried out here are required
 * for URB completion.
 */
void usb_hcd_unlink_urb_from_ep(struct usb_hcd *hcd, struct urb *urb)
{
	/* clear all state linking urb to this dev (and hcd) */
	spin_lock(&hcd_urb_list_lock);
	list_del_init(&urb->urb_list);
	spin_unlock(&hcd_urb_list_lock);
}
EXPORT_SYMBOL_GPL(usb_hcd_unlink_urb_from_ep);
