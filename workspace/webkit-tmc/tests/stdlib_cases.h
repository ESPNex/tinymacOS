#ifndef TMC_STDLIB_CASES_H
#define TMC_STDLIB_CASES_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
struct stdlib_job {unsigned seed;void*handoff;};
unsigned stdlib_setup(struct stdlib_job*);
void stdlib_start(void);
void *stdlib_worker(void*);
void *stdlib_dispatcher(void*);
unsigned stdlib_finish(void);
unsigned stdlib_teardown(void);
void stdlib_pause(void);
void stdlib_length_trap(void);
void stdlib_support_trap(void);
#ifdef __cplusplus
}
#endif
#endif
