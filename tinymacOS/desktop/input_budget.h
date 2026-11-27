#pragma once
/* Rendering must get a turn even when physical input or application IPC stays
 * readable indefinitely. Drag/selection adds IPC; neither queue may own the
 * entire desktop frame. Leave remaining events queued, never discard them. */
#define TMC_INPUT_FRAME_BUDGET 64u
#define TMC_MESSAGE_FRAME_BUDGET 32u
