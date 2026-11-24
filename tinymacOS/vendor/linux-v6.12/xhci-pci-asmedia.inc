#define ASMT_DATA_WRITE0_REG	0xF8
#define ASMT_DATA_WRITE1_REG	0xFC
#define ASMT_CONTROL_REG	0xE0
#define ASMT_CONTROL_WRITE_BIT	0x02
#define ASMT_WRITEREG_CMD	0x10423
#define ASMT_FLOWCTL_ADDR	0xFA30
#define ASMT_FLOWCTL_DATA	0xBA
#define ASMT_PSEUDO_DATA	0

/* Intel quirk use */
#define USB_INTEL_XUSB2PR      0xD0
#define USB_INTEL_USB2PRM      0xD4
#define USB_INTEL_USB3_PSSEN   0xD8
#define USB_INTEL_USB3PRM      0xDC

#ifdef CONFIG_USB_PCI_AMD
/* AMD quirk use */
#define	AB_REG_BAR_LOW		0xe0
#define	AB_REG_BAR_HIGH		0xe1
#define	AB_REG_BAR_SB700	0xf0
#define	AB_INDX(addr)		((addr) + 0x00)
#define	AB_DATA(addr)		((addr) + 0x04)
#define	AX_INDXC		0x30
#define	AX_DATAC		0x34

#define PT_ADDR_INDX		0xE8
#define PT_READ_INDX		0xE4
#define PT_SIG_1_ADDR		0xA520
#define PT_SIG_2_ADDR		0xA521
#define PT_SIG_3_ADDR		0xA522
#define PT_SIG_4_ADDR		0xA523
#define PT_SIG_1_DATA		0x78
#define PT_SIG_2_DATA		0x56
#define PT_SIG_3_DATA		0x34
#define PT_SIG_4_DATA		0x12
#define PT4_P1_REG		0xB521
#define PT4_P2_REG		0xB522
#define PT2_P1_REG		0xD520
#define PT2_P2_REG		0xD521
#define PT1_P1_REG		0xD522
#define PT1_P2_REG		0xD523

#define	NB_PCIE_INDX_ADDR	0xe0
#define	NB_PCIE_INDX_DATA	0xe4
#define	PCIE_P_CNTL		0x10040
#define	BIF_NB			0x10002
#define	NB_PIF0_PWRDOWN_0	0x01100012
#define	NB_PIF0_PWRDOWN_1	0x01100013

/*
 * amd_chipset_gen values represent AMD different chipset generations
 */
enum amd_chipset_gen {
	NOT_AMD_CHIPSET = 0,
	AMD_CHIPSET_SB600,
	AMD_CHIPSET_SB700,
	AMD_CHIPSET_SB800,
	AMD_CHIPSET_HUDSON2,
	AMD_CHIPSET_BOLTON,
	AMD_CHIPSET_YANGTZE,
	AMD_CHIPSET_TAISHAN,
	AMD_CHIPSET_UNKNOWN,
};

struct amd_chipset_type {
	enum amd_chipset_gen gen;
	u8 rev;
};

static struct amd_chipset_info {
	struct pci_dev	*nb_dev;
	struct pci_dev	*smbus_dev;
	int nb_type;
	struct amd_chipset_type sb_type;
	int isoc_reqs;
	int probe_count;
	bool need_pll_quirk;
} amd_chipset;

static DEFINE_SPINLOCK(amd_lock);

/*
 * amd_chipset_sb_type_init - initialize amd chipset southbridge type
 *
 * AMD FCH/SB generation and revision is identified by SMBus controller
 * vendor, device and revision IDs.
 *
 * Returns: 1 if it is an AMD chipset, 0 otherwise.
 */
static int amd_chipset_sb_type_init(struct amd_chipset_info *pinfo)
{
	u8 rev = 0;
	pinfo->sb_type.gen = AMD_CHIPSET_UNKNOWN;

	pinfo->smbus_dev = pci_get_device(PCI_VENDOR_ID_ATI,
			PCI_DEVICE_ID_ATI_SBX00_SMBUS, NULL);
	if (pinfo->smbus_dev) {
		rev = pinfo->smbus_dev->revision;
		if (rev >= 0x10 && rev <= 0x1f)
			pinfo->sb_type.gen = AMD_CHIPSET_SB600;
		else if (rev >= 0x30 && rev <= 0x3f)
			pinfo->sb_type.gen = AMD_CHIPSET_SB700;
		else if (rev >= 0x40 && rev <= 0x4f)
			pinfo->sb_type.gen = AMD_CHIPSET_SB800;
	} else {
		pinfo->smbus_dev = pci_get_device(PCI_VENDOR_ID_AMD,
				PCI_DEVICE_ID_AMD_HUDSON2_SMBUS, NULL);

		if (pinfo->smbus_dev) {
			rev = pinfo->smbus_dev->revision;
			if (rev >= 0x11 && rev <= 0x14)
				pinfo->sb_type.gen = AMD_CHIPSET_HUDSON2;
			else if (rev >= 0x15 && rev <= 0x18)
				pinfo->sb_type.gen = AMD_CHIPSET_BOLTON;
			else if (rev >= 0x39 && rev <= 0x3a)
				pinfo->sb_type.gen = AMD_CHIPSET_YANGTZE;
		} else {
			pinfo->smbus_dev = pci_get_device(PCI_VENDOR_ID_AMD,
							  0x145c, NULL);
			if (pinfo->smbus_dev) {
				rev = pinfo->smbus_dev->revision;
				pinfo->sb_type.gen = AMD_CHIPSET_TAISHAN;
			} else {
				pinfo->sb_type.gen = NOT_AMD_CHIPSET;
				return 0;
			}
		}
	}
	pinfo->sb_type.rev = rev;
	return 1;
}

void sb800_prefetch(struct device *dev, int on)
{
	u16 misc;
	struct pci_dev *pdev = to_pci_dev(dev);

	pci_read_config_word(pdev, 0x50, &misc);
	if (on == 0)
		pci_write_config_word(pdev, 0x50, misc & 0xfcff);
	else
		pci_write_config_word(pdev, 0x50, misc | 0x0300);
}
EXPORT_SYMBOL_GPL(sb800_prefetch);

static void usb_amd_find_chipset_info(void)
{
	unsigned long flags;
	struct amd_chipset_info info = { };

	spin_lock_irqsave(&amd_lock, flags);

	/* probe only once */
	if (amd_chipset.probe_count > 0) {
		amd_chipset.probe_count++;
		spin_unlock_irqrestore(&amd_lock, flags);
		return;
	}
	spin_unlock_irqrestore(&amd_lock, flags);

	if (!amd_chipset_sb_type_init(&info)) {
		goto commit;
	}

	switch (info.sb_type.gen) {
	case AMD_CHIPSET_SB700:
		info.need_pll_quirk = info.sb_type.rev <= 0x3B;
		break;
	case AMD_CHIPSET_SB800:
	case AMD_CHIPSET_HUDSON2:
	case AMD_CHIPSET_BOLTON:
		info.need_pll_quirk = true;
		break;
	default:
		info.need_pll_quirk = false;
		break;
	}

	if (!info.need_pll_quirk) {
		if (info.smbus_dev) {
			pci_dev_put(info.smbus_dev);
			info.smbus_dev = NULL;
		}
		goto commit;
	}

	info.nb_dev = pci_get_device(PCI_VENDOR_ID_AMD, 0x9601, NULL);
	if (info.nb_dev) {
		info.nb_type = 1;
	} else {
		info.nb_dev = pci_get_device(PCI_VENDOR_ID_AMD, 0x1510, NULL);
		if (info.nb_dev) {
			info.nb_type = 2;
		} else {
			info.nb_dev = pci_get_device(PCI_VENDOR_ID_AMD,
						     0x9600, NULL);
			if (info.nb_dev)
				info.nb_type = 3;
		}
	}

	printk(KERN_DEBUG "QUIRK: Enable AMD PLL fix\n");

commit:

	spin_lock_irqsave(&amd_lock, flags);
	if (amd_chipset.probe_count > 0) {
		/* race - someone else was faster - drop devices */

		/* Mark that we where here */
		amd_chipset.probe_count++;

		spin_unlock_irqrestore(&amd_lock, flags);

		pci_dev_put(info.nb_dev);
		pci_dev_put(info.smbus_dev);

	} else {
		/* no race - commit the result */
		info.probe_count++;
		amd_chipset = info;
		spin_unlock_irqrestore(&amd_lock, flags);
	}
}

int usb_hcd_amd_remote_wakeup_quirk(struct pci_dev *pdev)
{
	/* Make sure amd chipset type has already been initialized */
	usb_amd_find_chipset_info();
	if (amd_chipset.sb_type.gen == AMD_CHIPSET_YANGTZE ||
	    amd_chipset.sb_type.gen == AMD_CHIPSET_TAISHAN) {
		dev_dbg(&pdev->dev, "QUIRK: Enable AMD remote wakeup fix\n");
		return 1;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(usb_hcd_amd_remote_wakeup_quirk);

bool usb_amd_hang_symptom_quirk(void)
{
	u8 rev;

	usb_amd_find_chipset_info();
	rev = amd_chipset.sb_type.rev;
	/* SB600 and old version of SB700 have hang symptom bug */
	return amd_chipset.sb_type.gen == AMD_CHIPSET_SB600 ||
			(amd_chipset.sb_type.gen == AMD_CHIPSET_SB700 &&
			 rev >= 0x3a && rev <= 0x3b);
}
EXPORT_SYMBOL_GPL(usb_amd_hang_symptom_quirk);

bool usb_amd_prefetch_quirk(void)
{
	usb_amd_find_chipset_info();
	/* SB800 needs pre-fetch fix */
	return amd_chipset.sb_type.gen == AMD_CHIPSET_SB800;
}
EXPORT_SYMBOL_GPL(usb_amd_prefetch_quirk);

bool usb_amd_quirk_pll_check(void)
{
	usb_amd_find_chipset_info();
	return amd_chipset.need_pll_quirk;
}
EXPORT_SYMBOL_GPL(usb_amd_quirk_pll_check);

/*
 * The hardware normally enables the A-link power management feature, which
 * lets the system lower the power consumption in idle states.
 *
 * This USB quirk prevents the link going into that lower power state
 * during isochronous transfers.
 *
 * Without this quirk, isochronous stream on OHCI/EHCI/xHCI controllers of
 * some AMD platforms may stutter or have breaks occasionally.
 */
static void usb_amd_quirk_pll(int disable)
{
	u32 addr, addr_low, addr_high, val;
	u32 bit = disable ? 0 : 1;
	unsigned long flags;

	spin_lock_irqsave(&amd_lock, flags);

	if (disable) {
		amd_chipset.isoc_reqs++;
		if (amd_chipset.isoc_reqs > 1) {
			spin_unlock_irqrestore(&amd_lock, flags);
			return;
		}
	} else {
		amd_chipset.isoc_reqs--;
		if (amd_chipset.isoc_reqs > 0) {
			spin_unlock_irqrestore(&amd_lock, flags);
			return;
		}
	}

	if (amd_chipset.sb_type.gen == AMD_CHIPSET_SB800 ||
			amd_chipset.sb_type.gen == AMD_CHIPSET_HUDSON2 ||
			amd_chipset.sb_type.gen == AMD_CHIPSET_BOLTON) {
		outb_p(AB_REG_BAR_LOW, 0xcd6);
		addr_low = inb_p(0xcd7);
		outb_p(AB_REG_BAR_HIGH, 0xcd6);
		addr_high = inb_p(0xcd7);
		addr = addr_high << 8 | addr_low;

		outl_p(0x30, AB_INDX(addr));
		outl_p(0x40, AB_DATA(addr));
		outl_p(0x34, AB_INDX(addr));
		val = inl_p(AB_DATA(addr));
	} else if (amd_chipset.sb_type.gen == AMD_CHIPSET_SB700 &&
			amd_chipset.sb_type.rev <= 0x3b) {
		pci_read_config_dword(amd_chipset.smbus_dev,
					AB_REG_BAR_SB700, &addr);
		outl(AX_INDXC, AB_INDX(addr));
		outl(0x40, AB_DATA(addr));
		outl(AX_DATAC, AB_INDX(addr));
		val = inl(AB_DATA(addr));
	} else {
		spin_unlock_irqrestore(&amd_lock, flags);
		return;
	}

	if (disable) {
		val &= ~0x08;
		val |= (1 << 4) | (1 << 9);
	} else {
		val |= 0x08;
		val &= ~((1 << 4) | (1 << 9));
	}
	outl_p(val, AB_DATA(addr));

	if (!amd_chipset.nb_dev) {
		spin_unlock_irqrestore(&amd_lock, flags);
		return;
	}

	if (amd_chipset.nb_type == 1 || amd_chipset.nb_type == 3) {
		addr = PCIE_P_CNTL;
		pci_write_config_dword(amd_chipset.nb_dev,
					NB_PCIE_INDX_ADDR, addr);
		pci_read_config_dword(amd_chipset.nb_dev,
					NB_PCIE_INDX_DATA, &val);

		val &= ~(1 | (1 << 3) | (1 << 4) | (1 << 9) | (1 << 12));
		val |= bit | (bit << 3) | (bit << 12);
		val |= ((!bit) << 4) | ((!bit) << 9);
		pci_write_config_dword(amd_chipset.nb_dev,
					NB_PCIE_INDX_DATA, val);

		addr = BIF_NB;
		pci_write_config_dword(amd_chipset.nb_dev,
					NB_PCIE_INDX_ADDR, addr);
		pci_read_config_dword(amd_chipset.nb_dev,
					NB_PCIE_INDX_DATA, &val);
		val &= ~(1 << 8);
		val |= bit << 8;

		pci_write_config_dword(amd_chipset.nb_dev,
					NB_PCIE_INDX_DATA, val);
	} else if (amd_chipset.nb_type == 2) {
		addr = NB_PIF0_PWRDOWN_0;
		pci_write_config_dword(amd_chipset.nb_dev,
					NB_PCIE_INDX_ADDR, addr);
		pci_read_config_dword(amd_chipset.nb_dev,
					NB_PCIE_INDX_DATA, &val);
		if (disable)
			val &= ~(0x3f << 7);
		else
			val |= 0x3f << 7;

		pci_write_config_dword(amd_chipset.nb_dev,
					NB_PCIE_INDX_DATA, val);

		addr = NB_PIF0_PWRDOWN_1;
		pci_write_config_dword(amd_chipset.nb_dev,
					NB_PCIE_INDX_ADDR, addr);
		pci_read_config_dword(amd_chipset.nb_dev,
					NB_PCIE_INDX_DATA, &val);
		if (disable)
			val &= ~(0x3f << 7);
		else
			val |= 0x3f << 7;

		pci_write_config_dword(amd_chipset.nb_dev,
					NB_PCIE_INDX_DATA, val);
	}

	spin_unlock_irqrestore(&amd_lock, flags);
	return;
}

void usb_amd_quirk_pll_disable(void)
{
	usb_amd_quirk_pll(1);
}
EXPORT_SYMBOL_GPL(usb_amd_quirk_pll_disable);

void usb_amd_quirk_pll_enable(void)
{
	usb_amd_quirk_pll(0);
}
EXPORT_SYMBOL_GPL(usb_amd_quirk_pll_enable);

void usb_amd_dev_put(void)
{
	struct pci_dev *nb, *smbus;
	unsigned long flags;

	spin_lock_irqsave(&amd_lock, flags);

	amd_chipset.probe_count--;
	if (amd_chipset.probe_count > 0) {
		spin_unlock_irqrestore(&amd_lock, flags);
		return;
	}

	/* save them to pci_dev_put outside of spinlock */
	nb    = amd_chipset.nb_dev;
	smbus = amd_chipset.smbus_dev;

	amd_chipset.nb_dev = NULL;
	amd_chipset.smbus_dev = NULL;
	amd_chipset.nb_type = 0;
	memset(&amd_chipset.sb_type, 0, sizeof(amd_chipset.sb_type));
	amd_chipset.isoc_reqs = 0;
	amd_chipset.need_pll_quirk = false;

	spin_unlock_irqrestore(&amd_lock, flags);

	pci_dev_put(nb);
	pci_dev_put(smbus);
}
EXPORT_SYMBOL_GPL(usb_amd_dev_put);

/*
 * Check if port is disabled in BIOS on AMD Promontory host.
 * BIOS Disabled ports may wake on connect/disconnect and need
 * driver workaround to keep them disabled.
 * Returns true if port is marked disabled.
 */
bool usb_amd_pt_check_port(struct device *device, int port)
{
	unsigned char value, port_shift;
	struct pci_dev *pdev;
	u16 reg;

	pdev = to_pci_dev(device);
	pci_write_config_word(pdev, PT_ADDR_INDX, PT_SIG_1_ADDR);

	pci_read_config_byte(pdev, PT_READ_INDX, &value);
	if (value != PT_SIG_1_DATA)
		return false;

	pci_write_config_word(pdev, PT_ADDR_INDX, PT_SIG_2_ADDR);

	pci_read_config_byte(pdev, PT_READ_INDX, &value);
	if (value != PT_SIG_2_DATA)
		return false;

	pci_write_config_word(pdev, PT_ADDR_INDX, PT_SIG_3_ADDR);

	pci_read_config_byte(pdev, PT_READ_INDX, &value);
	if (value != PT_SIG_3_DATA)
		return false;

	pci_write_config_word(pdev, PT_ADDR_INDX, PT_SIG_4_ADDR);

	pci_read_config_byte(pdev, PT_READ_INDX, &value);
	if (value != PT_SIG_4_DATA)
		return false;

	/* Check disabled port setting, if bit is set port is enabled */
	switch (pdev->device) {
	case 0x43b9:
	case 0x43ba:
	/*
	 * device is AMD_PROMONTORYA_4(0x43b9) or PROMONTORYA_3(0x43ba)
	 * PT4_P1_REG bits[7..1] represents USB2.0 ports 6 to 0
	 * PT4_P2_REG bits[6..0] represents ports 13 to 7
	 */
		if (port > 6) {
			reg = PT4_P2_REG;
			port_shift = port - 7;
		} else {
			reg = PT4_P1_REG;
			port_shift = port + 1;
		}
		break;
	case 0x43bb:
	/*
	 * device is AMD_PROMONTORYA_2(0x43bb)
	 * PT2_P1_REG bits[7..5] represents USB2.0 ports 2 to 0
	 * PT2_P2_REG bits[5..0] represents ports 9 to 3
	 */
		if (port > 2) {
			reg = PT2_P2_REG;
			port_shift = port - 3;
		} else {
			reg = PT2_P1_REG;
			port_shift = port + 5;
		}
		break;
	case 0x43bc:
	/*
	 * device is AMD_PROMONTORYA_1(0x43bc)
	 * PT1_P1_REG[7..4] represents USB2.0 ports 3 to 0
	 * PT1_P2_REG[5..0] represents ports 9 to 4
	 */
		if (port > 3) {
			reg = PT1_P2_REG;
			port_shift = port - 4;
		} else {
			reg = PT1_P1_REG;
			port_shift = port + 4;
		}
		break;
	default:
		return false;
	}
	pci_write_config_word(pdev, PT_ADDR_INDX, reg);
	pci_read_config_byte(pdev, PT_READ_INDX, &value);

	return !(value & BIT(port_shift));
}
EXPORT_SYMBOL_GPL(usb_amd_pt_check_port);
#endif /* CONFIG_USB_PCI_AMD */

static int usb_asmedia_wait_write(struct pci_dev *pdev)
{
	unsigned long retry_count;
	unsigned char value;

	for (retry_count = 1000; retry_count > 0; --retry_count) {

		pci_read_config_byte(pdev, ASMT_CONTROL_REG, &value);

		if (value == 0xff) {
			dev_err(&pdev->dev, "%s: check_ready ERROR", __func__);
			return -EIO;
		}

		if ((value & ASMT_CONTROL_WRITE_BIT) == 0)
			return 0;

		udelay(50);
	}

	dev_warn(&pdev->dev, "%s: check_write_ready timeout", __func__);
	return -ETIMEDOUT;
}

void usb_asmedia_modifyflowcontrol(struct pci_dev *pdev)
{
	if (usb_asmedia_wait_write(pdev) != 0)
		return;

	/* send command and address to device */
	pci_write_config_dword(pdev, ASMT_DATA_WRITE0_REG, ASMT_WRITEREG_CMD);
	pci_write_config_dword(pdev, ASMT_DATA_WRITE1_REG, ASMT_FLOWCTL_ADDR);
	pci_write_config_byte(pdev, ASMT_CONTROL_REG, ASMT_CONTROL_WRITE_BIT);

	if (usb_asmedia_wait_write(pdev) != 0)
		return;

	/* send data to device */
	pci_write_config_dword(pdev, ASMT_DATA_WRITE0_REG, ASMT_FLOWCTL_DATA);
	pci_write_config_dword(pdev, ASMT_DATA_WRITE1_REG, ASMT_PSEUDO_DATA);
	pci_write_config_byte(pdev, ASMT_CONTROL_REG, ASMT_CONTROL_WRITE_BIT);
}
EXPORT_SYMBOL_GPL(usb_asmedia_modifyflowcontrol);
