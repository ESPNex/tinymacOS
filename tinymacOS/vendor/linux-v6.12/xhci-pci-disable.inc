void usb_disable_xhci_ports(struct pci_dev *xhci_pdev)
{
	pci_write_config_dword(xhci_pdev, USB_INTEL_USB3_PSSEN, 0x0);
	pci_write_config_dword(xhci_pdev, USB_INTEL_XUSB2PR, 0x0);
}
EXPORT_SYMBOL_GPL(usb_disable_xhci_ports);
