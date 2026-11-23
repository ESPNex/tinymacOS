/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Copyright (c) 2026, tinymacOS contributors.
 * Single producer/consumer, serialized by the firmware at TPL_NOTIFY.
 * Overflow is explicit: a consumer must reset its physical decoder, never
 * interpret a suffix with a missing E0/E1 prefix or a lost modifier release.
 */
#ifndef TMC_RAW_PS2_QUEUE_H
#define TMC_RAW_PS2_QUEUE_H
#define TMC_RAW_PS2_QUEUE_SIZE 256u
struct TmcRawPs2Queue {
  unsigned head, tail, overflow;
  unsigned char data[TMC_RAW_PS2_QUEUE_SIZE];
};
static inline void tmc_raw_ps2_reset(struct TmcRawPs2Queue *q)
{
  q->head = q->tail = q->overflow = 0;
}
static inline void tmc_raw_ps2_push(struct TmcRawPs2Queue *q, unsigned char b)
{
  unsigned next = (q->head + 1u) % TMC_RAW_PS2_QUEUE_SIZE;
  if (q->overflow) return;
  if (next == q->tail) { q->overflow = 1; return; }
  q->data[q->head] = b;
  q->head = next;
}
/* -1: discontinuity, decoder reset required; 0: empty; 1: one byte. */
static inline int tmc_raw_ps2_pop(struct TmcRawPs2Queue *q, unsigned char *b)
{
  if (q->overflow) { tmc_raw_ps2_reset(q); return -1; }
  if (q->head == q->tail) return 0;
  *b = q->data[q->tail];
  q->tail = (q->tail + 1u) % TMC_RAW_PS2_QUEUE_SIZE;
  return 1;
}
#endif
