struct api_context {
	struct completion	done;
	int			status;
};

static void usb_api_blocking_completion(struct urb *urb)
{
	struct api_context *ctx = urb->context;

	ctx->status = urb->status;
	complete(&ctx->done);
}


/*
 * Starts urb and waits for completion or timeout. Note that this call
 * is NOT interruptible. Many device driver i/o requests should be
 * interruptible and therefore these drivers should implement their
 * own interruptible routines.
 */
static int usb_start_wait_urb(struct urb *urb, int timeout, int *actual_length)
{
	struct api_context ctx;
	unsigned long expire;
	int retval;

	init_completion(&ctx.done);
	urb->context = &ctx;
	urb->actual_length = 0;
	retval = usb_submit_urb(urb, GFP_NOIO);
	if (unlikely(retval))
		goto out;

	expire = timeout ? msecs_to_jiffies(timeout) : MAX_SCHEDULE_TIMEOUT;
	if (!wait_for_completion_timeout(&ctx.done, expire)) {
		usb_kill_urb(urb);
		retval = (ctx.status == -ENOENT ? -ETIMEDOUT : ctx.status);

		dev_dbg(&urb->dev->dev,
			"%s timed out on ep%d%s len=%u/%u\n",
			current->comm,
			usb_endpoint_num(&urb->ep->desc),
			usb_urb_dir_in(urb) ? "in" : "out",
			urb->actual_length,
			urb->transfer_buffer_length);
	} else
		retval = ctx.status;
out:
	if (actual_length)
		*actual_length = urb->actual_length;

	usb_free_urb(urb);
	return retval;
}

/*-------------------------------------------------------------------*/
/* returns status (negative) or length (positive) */
static int usb_internal_control_msg(struct usb_device *usb_dev,
				    unsigned int pipe,
				    struct usb_ctrlrequest *cmd,
				    void *data, int len, int timeout)
{
	struct urb *urb;
	int retv;
	int length;

	urb = usb_alloc_urb(0, GFP_NOIO);
	if (!urb)
		return -ENOMEM;

	usb_fill_control_urb(urb, usb_dev, pipe, (unsigned char *)cmd, data,
			     len, usb_api_blocking_completion, NULL);

	retv = usb_start_wait_urb(urb, timeout, &length);
	if (retv < 0)
		return retv;
	else
		return length;
}

/**
 * usb_control_msg - Builds a control urb, sends it off and waits for completion
 * @dev: pointer to the usb device to send the message to
 * @pipe: endpoint "pipe" to send the message to
 * @request: USB message request value
 * @requesttype: USB message request type value
 * @value: USB message value
 * @index: USB message index value
 * @data: pointer to the data to send
 * @size: length in bytes of the data to send
 * @timeout: time in msecs to wait for the message to complete before timing
 *	out (if 0 the wait is forever)
 *
 * Context: task context, might sleep.
 *
 * This function sends a simple control message to a specified endpoint and
 * waits for the message to complete, or timeout.
 *
 * Don't use this function from within an interrupt context. If you need
 * an asynchronous message, or need to send a message from within interrupt
 * context, use usb_submit_urb(). If a thread in your driver uses this call,
 * make sure your disconnect() method can wait for it to complete. Since you
 * don't have a handle on the URB used, you can't cancel the request.
 *
 * Return: If successful, the number of bytes transferred. Otherwise, a negative
 * error number.
 */
int usb_control_msg(struct usb_device *dev, unsigned int pipe, __u8 request,
		    __u8 requesttype, __u16 value, __u16 index, void *data,
		    __u16 size, int timeout)
{
	struct usb_ctrlrequest *dr;
	int ret;

	dr = kmalloc(sizeof(struct usb_ctrlrequest), GFP_NOIO);
	if (!dr)
		return -ENOMEM;

	dr->bRequestType = requesttype;
	dr->bRequest = request;
	dr->wValue = cpu_to_le16(value);
	dr->wIndex = cpu_to_le16(index);
	dr->wLength = cpu_to_le16(size);

	ret = usb_internal_control_msg(dev, pipe, dr, data, size, timeout);

	/* Linger a bit, prior to the next control message. */
	if (dev->quirks & USB_QUIRK_DELAY_CTRL_MSG)
		msleep(200);

	kfree(dr);

	return ret;
}
EXPORT_SYMBOL_GPL(usb_control_msg);

/**
 * usb_control_msg_send - Builds a control "send" message, sends it off and waits for completion
 * @dev: pointer to the usb device to send the message to
 * @endpoint: endpoint to send the message to
 * @request: USB message request value
 * @requesttype: USB message request type value
 * @value: USB message value
 * @index: USB message index value
 * @driver_data: pointer to the data to send
 * @size: length in bytes of the data to send
 * @timeout: time in msecs to wait for the message to complete before timing
 *	out (if 0 the wait is forever)
 * @memflags: the flags for memory allocation for buffers
 *
 * Context: !in_interrupt ()
 *
 * This function sends a control message to a specified endpoint that is not
 * expected to fill in a response (i.e. a "send message") and waits for the
 * message to complete, or timeout.
 *
 * Do not use this function from within an interrupt context. If you need
 * an asynchronous message, or need to send a message from within interrupt
 * context, use usb_submit_urb(). If a thread in your driver uses this call,
 * make sure your disconnect() method can wait for it to complete. Since you
 * don't have a handle on the URB used, you can't cancel the request.
 *
 * The data pointer can be made to a reference on the stack, or anywhere else,
 * as it will not be modified at all.  This does not have the restriction that
 * usb_control_msg() has where the data pointer must be to dynamically allocated
 * memory (i.e. memory that can be successfully DMAed to a device).
 *
 * Return: If successful, 0 is returned, Otherwise, a negative error number.
 */
int usb_control_msg_send(struct usb_device *dev, __u8 endpoint, __u8 request,
			 __u8 requesttype, __u16 value, __u16 index,
			 const void *driver_data, __u16 size, int timeout,
			 gfp_t memflags)
{
	unsigned int pipe = usb_sndctrlpipe(dev, endpoint);
	int ret;
	u8 *data = NULL;

	if (size) {
		data = kmemdup(driver_data, size, memflags);
		if (!data)
			return -ENOMEM;
	}

	ret = usb_control_msg(dev, pipe, request, requesttype, value, index,
			      data, size, timeout);
	kfree(data);

	if (ret < 0)
		return ret;

	return 0;
}
EXPORT_SYMBOL_GPL(usb_control_msg_send);

/**
 * usb_control_msg_recv - Builds a control "receive" message, sends it off and waits for completion
 * @dev: pointer to the usb device to send the message to
 * @endpoint: endpoint to send the message to
 * @request: USB message request value
 * @requesttype: USB message request type value
 * @value: USB message value
 * @index: USB message index value
 * @driver_data: pointer to the data to be filled in by the message
 * @size: length in bytes of the data to be received
 * @timeout: time in msecs to wait for the message to complete before timing
 *	out (if 0 the wait is forever)
 * @memflags: the flags for memory allocation for buffers
 *
 * Context: !in_interrupt ()
 *
 * This function sends a control message to a specified endpoint that is
 * expected to fill in a response (i.e. a "receive message") and waits for the
 * message to complete, or timeout.
 *
 * Do not use this function from within an interrupt context. If you need
 * an asynchronous message, or need to send a message from within interrupt
 * context, use usb_submit_urb(). If a thread in your driver uses this call,
 * make sure your disconnect() method can wait for it to complete. Since you
 * don't have a handle on the URB used, you can't cancel the request.
 *
 * The data pointer can be made to a reference on the stack, or anywhere else
 * that can be successfully written to.  This function does not have the
 * restriction that usb_control_msg() has where the data pointer must be to
 * dynamically allocated memory (i.e. memory that can be successfully DMAed to a
 * device).
 *
 * The "whole" message must be properly received from the device in order for
 * this function to be successful.  If a device returns less than the expected
 * amount of data, then the function will fail.  Do not use this for messages
 * where a variable amount of data might be returned.
 *
 * Return: If successful, 0 is returned, Otherwise, a negative error number.
 */
int usb_control_msg_recv(struct usb_device *dev, __u8 endpoint, __u8 request,
			 __u8 requesttype, __u16 value, __u16 index,
			 void *driver_data, __u16 size, int timeout,
			 gfp_t memflags)
{
	unsigned int pipe = usb_rcvctrlpipe(dev, endpoint);
	int ret;
	u8 *data;

	if (!size || !driver_data)
		return -EINVAL;

	data = kmalloc(size, memflags);
	if (!data)
		return -ENOMEM;

	ret = usb_control_msg(dev, pipe, request, requesttype, value, index,
			      data, size, timeout);

	if (ret < 0)
		goto exit;

	if (ret == size) {
		memcpy(driver_data, data, size);
		ret = 0;
	} else {
		ret = -EREMOTEIO;
	}

exit:
	kfree(data);
	return ret;
}
EXPORT_SYMBOL_GPL(usb_control_msg_recv);
