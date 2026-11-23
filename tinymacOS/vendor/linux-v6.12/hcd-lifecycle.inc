int usb_hcd_submit_urb (struct urb *urb, gfp_t mem_flags)
{
	int			status;
	struct usb_hcd		*hcd = bus_to_hcd(urb->dev->bus);

	/* increment urb's reference count as part of giving it to the HCD
	 * (which will control it).  HCD guarantees that it either returns
	 * an error or calls giveback(), but not both.
	 */
	usb_get_urb(urb);
	atomic_inc(&urb->use_count);
	atomic_inc(&urb->dev->urbnum);
	usbmon_urb_submit(&hcd->self, urb);

	/* NOTE requirements on root-hub callers (usbfs and the hub
	 * driver, for now):  URBs' urb->transfer_buffer must be
	 * valid and usb_buffer_{sync,unmap}() not be needed, since
	 * they could clobber root hub response data.  Also, control
	 * URBs must be submitted in process context with interrupts
	 * enabled.
	 */

	if (is_root_hub(urb->dev)) {
		status = rh_urb_enqueue(hcd, urb);
	} else {
		status = map_urb_for_dma(hcd, urb, mem_flags);
		if (likely(status == 0)) {
			status = hcd->driver->urb_enqueue(hcd, urb, mem_flags);
			if (unlikely(status))
				unmap_urb_for_dma(hcd, urb);
		}
	}

	if (unlikely(status)) {
		usbmon_urb_submit_error(&hcd->self, urb, status);
		urb->hcpriv = NULL;
		INIT_LIST_HEAD(&urb->urb_list);
		atomic_dec(&urb->use_count);
		/*
		 * Order the write of urb->use_count above before the read
		 * of urb->reject below.  Pairs with the memory barriers in
		 * usb_kill_urb() and usb_poison_urb().
		 */
		smp_mb__after_atomic();

		atomic_dec(&urb->dev->urbnum);
		if (atomic_read(&urb->reject))
			wake_up(&usb_kill_urb_queue);
		usb_put_urb(urb);
	}
	return status;
}

/*-------------------------------------------------------------------------*/

/* this makes the hcd giveback() the urb more quickly, by kicking it
 * off hardware queues (which may take a while) and returning it as
 * soon as practical.  we've already set up the urb's return status,
 * but we can't know if the callback completed already.
 */
static int unlink1(struct usb_hcd *hcd, struct urb *urb, int status)
{
	int		value;

	if (is_root_hub(urb->dev))
		value = usb_rh_urb_dequeue(hcd, urb, status);
	else {

		/* The only reason an HCD might fail this call is if
		 * it has not yet fully queued the urb to begin with.
		 * Such failures should be harmless. */
		value = hcd->driver->urb_dequeue(hcd, urb, status);
	}
	return value;
}

/*
 * called in any context
 *
 * caller guarantees urb won't be recycled till both unlink()
 * and the urb's completion function return
 */
int usb_hcd_unlink_urb (struct urb *urb, int status)
{
	struct usb_hcd		*hcd;
	struct usb_device	*udev = urb->dev;
	int			retval = -EIDRM;
	unsigned long		flags;

	/* Prevent the device and bus from going away while
	 * the unlink is carried out.  If they are already gone
	 * then urb->use_count must be 0, since disconnected
	 * devices can't have any active URBs.
	 */
	spin_lock_irqsave(&hcd_urb_unlink_lock, flags);
	if (atomic_read(&urb->use_count) > 0) {
		retval = 0;
		usb_get_dev(udev);
	}
	spin_unlock_irqrestore(&hcd_urb_unlink_lock, flags);
	if (retval == 0) {
		hcd = bus_to_hcd(urb->dev->bus);
		retval = unlink1(hcd, urb, status);
		if (retval == 0)
			retval = -EINPROGRESS;
		else if (retval != -EIDRM && retval != -EBUSY)
			dev_dbg(&udev->dev, "hcd_unlink_urb %pK fail %d\n",
					urb, retval);
		usb_put_dev(udev);
	}
	return retval;
}

/*-------------------------------------------------------------------------*/

static void __usb_hcd_giveback_urb(struct urb *urb)
{
	struct usb_hcd *hcd = bus_to_hcd(urb->dev->bus);
	struct usb_anchor *anchor = urb->anchor;
	int status = urb->unlinked;
	unsigned long flags;

	urb->hcpriv = NULL;
	if (unlikely((urb->transfer_flags & URB_SHORT_NOT_OK) &&
	    urb->actual_length < urb->transfer_buffer_length &&
	    !status))
		status = -EREMOTEIO;

	unmap_urb_for_dma(hcd, urb);
	usbmon_urb_complete(&hcd->self, urb, status);
	usb_anchor_suspend_wakeups(anchor);
	usb_unanchor_urb(urb);
	if (likely(status == 0))
		usb_led_activity(USB_LED_EVENT_HOST);

	/* pass ownership to the completion handler */
	urb->status = status;
	/*
	 * Only collect coverage in the softirq context and disable interrupts
	 * to avoid scenarios with nested remote coverage collection sections
	 * that KCOV does not support.
	 * See the comment next to kcov_remote_start_usb_softirq() for details.
	 */
	flags = kcov_remote_start_usb_softirq((u64)urb->dev->bus->busnum);
	urb->complete(urb);
	kcov_remote_stop_softirq(flags);

	usb_anchor_resume_wakeups(anchor);
	atomic_dec(&urb->use_count);
	/*
	 * Order the write of urb->use_count above before the read
	 * of urb->reject below.  Pairs with the memory barriers in
	 * usb_kill_urb() and usb_poison_urb().
	 */
	smp_mb__after_atomic();

	if (unlikely(atomic_read(&urb->reject)))
		wake_up(&usb_kill_urb_queue);
	usb_put_urb(urb);
}
