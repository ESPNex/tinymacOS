# WebKit runtime — private parking and synchronization milestone

> **Latest large block:** [managed thread runtime](RUNTIME-THREADS.md) now adds automatic child TCB startup, stable process-local identity, safe kernel-final-handoff join and generation TSD with reentrant bounded destructors. Full expanded suite is **29/29 PASS**. This document retains the earlier synchronization milestone and interface comparison, updated below.

## What is implemented and tested

These are real native primitives, **not pthread stubs** and not a compiled WebKit engine:

- `S_WORDWAIT` appended to the ABI; `include/wordwaitabi.h` documents v1.
- Atomic compare-and-park and wake under the scheduler lock. Private key is `(process, virtual address)`; the kernel retains no dereferenceable user pointer while asleep.
- Aligned writable 32-bit user words, mismatch return, absolute 100 Hz tick deadline, timeout, bounded wake and wake-all. Timeout/normal wake clear the task's wait key; newly allocated task slots start without an old key.
- `posix/src/sync.c`: real mutex state machine (unlocked/uncontended/contended), trylock, timedlock and condition-variable sequence/wake logic. No busy-loop substitute for sleeping waiters.
- Acquire/release atomics; condition wait unlocks and always reacquires the mutex before returning, including timeout. Caller rechecks its predicate under the same mutex.
- Primitive errors are returned directly; this layer does not modify `errno` or require a TCB. Higher-level errno-using adapters still require explicit per-thread TCB attachment.

### Evidence

The synchronization milestone passed **24/24**; the subsequent managed-thread block expands this to **29/29 PASS** (see linked report). Original synchronization coverage:

- 10 host/contracts/target-compilation checks, including ASan/UBSan sync algorithms over Linux futex transport;
- four diagnostic guest builds;
- TCP/read-only-file and VM/MMU on 1/2 vCPU;
- native synchronization and FS/errno on 1/4/8 vCPU.

Host synchronization checks serialize **8,000 counter updates** over four concurrent host workers, exercise condition broadcast, timed mutex/condition wait and mutex reacquisition. Host pthreads/futex are test transport only, not the target implementation.

Native synchronization profiles execute **240 critical sections**, deliberately yielding *while holding* the mutex, four workers, predicate loops/broadcast, timedlock and condition timeout/reacquisition, invalid addresses/operations, changed-word return, wake count zero and wake-all without waiters. Each worker's errno remains unchanged. Multi-CPU profiles require critical-section samples from more than one CPU; this is not a claim that every thread migrates or all possible schedules were tested.

An initial 8-vCPU run failed. A diagnostic repeat identified a **coverage assertion** (CPU mask `4`: all critical sections on one CPU), with all 240 updates correct. The workload had formed a lock-handoff convoy. The probe now sleeps one tick *outside* each critical section as well as yielding inside it, while retaining the multi-CPU assertion. Three consecutive corrected 8-vCPU runs and the subsequent full suite passed. This did not justify weakening the assertion or calling earlier failed profiles successful; investigation is recorded in `test-results/sync-investigation.json`.

Results and source/build hashes: `test-results/guest-sync-*.json`, `test-results/milestone-tests.json`. Cropped real terminal captures: `test-results/sync/`.

## Comparison to actual pinned WTF interfaces

Real upstream headers are saved, copyright intact, in `upstream-reference/Source/WTF/wtf/`; immutable commit and per-file hashes in `upstream-reference/threading-reference-lock.json`. They remain a **reference subset**, not a complete source checkout or target build.

| Real upstream interface | Native foundation | Still required for an honest port |
|---|---|---|
| `ThreadingPrimitives.h`: `Mutex::lock/tryLock/unlock` | `tmc_mutex_*` | Platform types/configuration, real WTF compilation and tests |
| `ThreadCondition::wait/signal/broadcast` | `tmc_condition_*` | WTF adapter and engine lifecycle integration |
| `ThreadCondition::timedWait(Mutex&, WallTime)` | absolute **monotonic tick** wait only | A validated realtime/WallTime bridge. Do **not** pass epoch seconds as kernel ticks or pretend the two clocks are interchangeable |
| `Threading.h`: create, current ID, join/detach | managed native startup/identity/join + kernel final handoff now tested | WTF adapter/types, initial process startup and detach cleanup still missing |
| `ThreadSpecific.h`: key create/get/set and destructors | native generation keys, per-thread storage and bounded/reentrant destructor cleanup now tested | Integrate actual WTF types/calls; validate capacity and engine lifecycle |
| `StackBounds.h`: current/new-thread bounds | fixed native thread stacks | Real stack metadata and portable runtime exposure |
| Thread suspension/stack inspection | no complete native counterpart | Collector/thread cooperation or a supported non-JIT engine configuration; no fake success |

### Next implementation boundary

Native thread startup/lifetime and thread-specific keys with cleanup are now implemented in the subsequent macroblock. The next boundary is the **target allocator/libc/C++ runtime and actual WTF/JSC platform integration**; see `RUNTIME-THREADS.md`. The mutex/condition foundation does not by itself make the existing descriptor tables safe for shared concurrent callers.

No empty implementations should report success for join, cancellation, suspend/resume or thread-specific destructors. Until a correct realtime API exists, WTF WallTime waits also remain an explicit integration gap.

## Limits

- No complete pthread implementation, general initial-process bootstrap, detach, robust mutexes, priority inheritance, process-shared futexes, requeue, cancellation or signal semantics. Managed child startup and join/TSD cleanup are covered by the linked newer report; caller-owned objects still require external lifecycle ordering.
- Objects must stay mapped until all waiters return; callers prevent address/sequence ABA and do not reinitialize a live object. No fairness or latency guarantee; a woken task is eligible for ordinary scheduling.
- Thread-local errno is not ELF/C++ TLS/DTV or network TLS/HTTPS.
- No WTF/JSC/WebCore build or browser rendering/HTTPS acceptance yet. Real Safari UI and integration with every theme remain gated on the engine/runtime.

## Source and media policy

All authored source is saved under `/home/user` and synchronized to the private repository before builds. All diagnostic media and full screenshots stay outside the workspace. Diagnostic aliases run probes and **must not be distributed as an OS or browser**. No new release/ISO upload was performed in this milestone; OC2 remains unchanged. Future browser release remains **2.4.5 Levante**.
