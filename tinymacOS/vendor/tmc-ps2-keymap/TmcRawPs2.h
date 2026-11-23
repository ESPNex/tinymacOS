/** @file
  Optional physical set-1 stream exposed by the tinymacOS-derived OpenCore
  Ps2KeyboardDxe. This is not an upstream or standard UEFI protocol.
  The firmware driver remains the only owner of the controller ports.

  SPDX-License-Identifier: BSD-2-Clause-Patent
  Copyright (c) 2026, tinymacOS contributors.
**/
#ifndef TMC_RAW_PS2_H
#define TMC_RAW_PS2_H
#define TMC_RAW_PS2_PROTOCOL_GUID \
  { 0xe2dbc617, 0xb651, 0x4ab7, { 0xb3, 0xa8, 0x7f, 0x28, 0xc6, 0xb1, 0x91, 0x40 } }
#define TMC_RAW_PS2_PROTOCOL_REVISION 1
/* Identifies the derived driver binding for targeted firmware rebinding. */
#define TMC_RAW_PS2_DRIVER_VERSION 0xa0170001u

typedef struct TMC_RAW_PS2_PROTOCOL TMC_RAW_PS2_PROTOCOL;
typedef EFI_STATUS (EFIAPI *TMC_RAW_PS2_RESET)(TMC_RAW_PS2_PROTOCOL *This);
/* Returns one byte, EFI_NOT_READY if empty, or EFI_DEVICE_ERROR for overflow.
 * On overflow the consumer MUST clear its prefix/modifier/held-key state.
 * This protocol is single-consumer. Reset before beginning consumption.
 * It is valid only while boot services and this driver binding are active.
 * Reset affects only this raw queue, not standard ConIn/ConInEx or hardware.
 */
typedef EFI_STATUS (EFIAPI *TMC_RAW_PS2_READ)(TMC_RAW_PS2_PROTOCOL *This, UINT8 *Byte);
struct TMC_RAW_PS2_PROTOCOL {
  UINT64 Revision;
  TMC_RAW_PS2_RESET Reset;
  TMC_RAW_PS2_READ Read;
};
#endif
