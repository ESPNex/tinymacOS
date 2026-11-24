void uhci_reset_hc(struct pci_dev *pdev, unsigned long base)
{
	/* Turn off PIRQ enable and SMI enable.  (This also turns off the
	 * BIOS's USB Legacy Support.)  Turn off all the R/WC bits too.
	 */
	pci_write_config_word(pdev, UHCI_USBLEGSUP, UHCI_USBLEGSUP_RWC);

	/* Reset the HC - this will force us to get a
	 * new notification of any already connected
	 * ports due to the virtual disconnect that it
	 * implies.
	 */
	outw(UHCI_USBCMD_HCRESET, base + UHCI_USBCMD);
	mb();
	udelay(5);
	if (inw(base + UHCI_USBCMD) & UHCI_USBCMD_HCRESET)
		dev_warn(&pdev->dev, "HCRESET not completed yet!\n");

	/* Just to be safe, disable interrupt requests and
	 * make sure the controller is stopped.
	 */
	outw(0, base + UHCI_USBINTR);
	outw(0, base + UHCI_USBCMD);
}
EXPORT_SYMBOL_GPL(uhci_reset_hc);

/*
 * Initialize a controller that was newly discovered or has just been
 * resumed.  In either case we can't be sure of its previous state.
 *
 * Returns: 1 if the controller was reset, 0 otherwise.
 */
int uhci_check_and_reset_hc(struct pci_dev *pdev, unsigned long base)
{
	u16 legsup;
	unsigned int cmd, intr;

	/*
	 * When restarting a suspended controller, we expect all the
	 * settings to be the same as we left them:
	 *
	 *	PIRQ and SMI disabled, no R/W bits set in USBLEGSUP;
	 *	Controller is stopped and configured with EGSM set;
	 *	No interrupts enabled except possibly Resume Detect.
	 *
	 * If any of these conditions are violated we do a complete reset.
	 */
	pci_read_config_word(pdev, UHCI_USBLEGSUP, &legsup);
	if (legsup & ~(UHCI_USBLEGSUP_RO | UHCI_USBLEGSUP_RWC)) {
		dev_dbg(&pdev->dev, "%s: legsup = 0x%04x\n",
				__func__, legsup);
		goto reset_needed;
	}

	cmd = inw(base + UHCI_USBCMD);
	if ((cmd & UHCI_USBCMD_RUN) || !(cmd & UHCI_USBCMD_CONFIGURE) ||
			!(cmd & UHCI_USBCMD_EGSM)) {
		dev_dbg(&pdev->dev, "%s: cmd = 0x%04x\n",
				__func__, cmd);
		goto reset_needed;
	}

	intr = inw(base + UHCI_USBINTR);
	if (intr & (~UHCI_USBINTR_RESUME)) {
		dev_dbg(&pdev->dev, "%s: intr = 0x%04x\n",
				__func__, intr);
		goto reset_needed;
	}
	return 0;

reset_needed:
	dev_dbg(&pdev->dev, "Performing full reset\n");
	uhci_reset_hc(pdev, base);
	return 1;
}
EXPORT_SYMBOL_GPL(uhci_check_and_reset_hc);
