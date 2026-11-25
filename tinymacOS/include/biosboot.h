#pragma once
/* Fixed reserved gap, outside GPT LBAs 1..33 and before ESP LBA 2048. */
#define TMC_DUET_LBA 64u
extern const unsigned char bios_coexist_mbr[440],bios_boot_mbr[440],bios_duet[],bios_duet_end[];
