void psmouse_report_standard_buttons(struct input_dev *dev, u8 buttons)
{
	input_report_key(dev, BTN_LEFT,   buttons & BIT(0));
	input_report_key(dev, BTN_MIDDLE, buttons & BIT(2));
	input_report_key(dev, BTN_RIGHT,  buttons & BIT(1));
}

void psmouse_report_standard_motion(struct input_dev *dev, u8 *packet)
{
	int x, y;

	x = packet[1] ? packet[1] - ((packet[0] << 4) & 0x100) : 0;
	y = packet[2] ? packet[2] - ((packet[0] << 3) & 0x100) : 0;

	input_report_rel(dev, REL_X, x);
	input_report_rel(dev, REL_Y, -y);
}

void psmouse_report_standard_packet(struct input_dev *dev, u8 *packet)
{
	psmouse_report_standard_buttons(dev, packet[0]);
	psmouse_report_standard_motion(dev, packet);
}

/*
 * psmouse_process_byte() analyzes the PS/2 data stream and reports
 * relevant events to the input module once full packet has arrived.
 */
psmouse_ret_t psmouse_process_byte(struct psmouse *psmouse)
{
	struct input_dev *dev = psmouse->dev;
	u8 *packet = psmouse->packet;
	int wheel;

	if (psmouse->pktcnt < psmouse->pktsize)
		return PSMOUSE_GOOD_DATA;

	/* Full packet accumulated, process it */

	switch (psmouse->protocol->type) {
	case PSMOUSE_IMPS:
		/* IntelliMouse has scroll wheel */
		input_report_rel(dev, REL_WHEEL, -(s8) packet[3]);
		break;

	case PSMOUSE_IMEX:
		/* Scroll wheel and buttons on IntelliMouse Explorer */
		switch (packet[3] & 0xC0) {
		case 0x80: /* vertical scroll on IntelliMouse Explorer 4.0 */
			input_report_rel(dev, REL_WHEEL,
					 -sign_extend32(packet[3], 5));
			break;
		case 0x40: /* horizontal scroll on IntelliMouse Explorer 4.0 */
			input_report_rel(dev, REL_HWHEEL,
					 -sign_extend32(packet[3], 5));
			break;
		case 0x00:
		case 0xC0:
			wheel = sign_extend32(packet[3], 3);

			/*
			 * Some A4Tech mice have two scroll wheels, with first
			 * one reporting +/-1 in the lower nibble, and second
			 * one reporting +/-2.
			 */
			if (psmouse_a4tech_2wheels && abs(wheel) > 1)
				input_report_rel(dev, REL_HWHEEL, wheel / 2);
			else
				input_report_rel(dev, REL_WHEEL, -wheel);

			input_report_key(dev, BTN_SIDE,  packet[3] & BIT(4));
			input_report_key(dev, BTN_EXTRA, packet[3] & BIT(5));
			break;
		}
		break;

	case PSMOUSE_GENPS:
		/* Report scroll buttons on NetMice */
		input_report_rel(dev, REL_WHEEL, -(s8) packet[3]);

		/* Extra buttons on Genius NewNet 3D */
		input_report_key(dev, BTN_SIDE,  packet[0] & BIT(6));
		input_report_key(dev, BTN_EXTRA, packet[0] & BIT(7));
		break;

	case PSMOUSE_THINKPS:
		/* Extra button on ThinkingMouse */
		input_report_key(dev, BTN_EXTRA, packet[0] & BIT(3));

		/*
		 * Without this bit of weirdness moving up gives wildly
		 * high Y changes.
		 */
		packet[1] |= (packet[0] & 0x40) << 1;
		break;

	case PSMOUSE_CORTRON:
		/*
		 * Cortron PS2 Trackball reports SIDE button in the
		 * 4th bit of the first byte.
		 */
		input_report_key(dev, BTN_SIDE, packet[0] & BIT(3));
		packet[0] |= BIT(3);
		break;

	default:
		break;
	}

	/* Generic PS/2 Mouse */
	packet[0] |= psmouse->extra_buttons;
	psmouse_report_standard_packet(dev, packet);

	input_sync(dev);

	return PSMOUSE_FULL_PACKET;
}
