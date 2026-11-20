#pragma once
/* Small, optional handoff between the actual image reader and its EFI child.
 * Generated from the opened volume's UEFI device path, never from a UI label. */
#define ORIGIN_MAGIC 0x424f5232u
struct RecoveryOrigin { u32 magic, optical, partition, image_bytes; };
