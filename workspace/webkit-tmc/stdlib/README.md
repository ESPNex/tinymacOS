# Selected C++ consumers and pinned WTF numeric component — 10.3 development

This is **not** a full standard-library port, complete WTF build, JavaScriptCore,
WebCore, Safari, or an addition to the published 10.2.5 ISO.

## What's real

- Real GCC 14/libstdc++ header-template instantiations of `vector`, `string`,
  `unique_ptr`, `function`, `algorithm`, `span`, `array`, `atomic`.
- No `libstdc++.so`, `libstdc++.a`, `libc++.so`, or host libc in the native target.
  Target links with `ld -nostdlib`; undefined-symbol and dynamic-dependency audits
  must pass. Existing native heap, mutex/condition/thread, lifecycle and task queue
  service these consumers. `record-stdlib-headers.py` records the actual compile
  closure with compiler version and SHA-256 for each GCC/C header.
- 22 byte-original upstream files including licenses, pinned to WebKit commit
  `23a3d9001767ddc9a4b120b6d7db33ca21230ab8`. A deterministic *external* port replaces
  four heavyweight helper includes with the explicitly bounded support header.
  It also applies **two explicit signed casts in `strtod.cc`**. Source hashes,
  generated hashes and the complete patch are recorded. No hidden file edits.
- Why those casts: the original pin adds negative exponent to unsigned `size_t`
  length, so `0.1` parses as Infinity. The initial independent 1,040-vector test
  found 526 mismatches. The runner replays the original failing `0.1` case using
  original `strtod.cc`, then requires correct binary64 bits with the patched port.
  No failing corpus vector was deleted or oracle changed to fit the implementation.
- Native `ceil` implements required binary64 rounding by integer decomposition;
  preserves zeros/infinity/quiet-NaN payloads. It is not a complete libm ABI or
  signaling-NaN/floating-exception-flag implementation. Narrow `strnlen`/`memchr`
  supplement native base memory/string helpers.

## Boundaries

Headers are intentionally the build-host **hosted** GNU configuration. GCC's
`-ffreestanding` rejects these containers. They are not relabeled freestanding
libc++; only the linked consumers and their actual target dependencies are in
scope. Unsupported standard-library combinations may demand additional symbols.
Compile header hashes are recorded, not vendored: using a different GNU toolchain
requires a new manifest and complete tests. Native errno/TLS/type aliases are not
claims of glibc ABI compatibility. License/copyright from the actual Debian compiler
package is included in `GCC-DEBIAN-COPYRIGHT.txt`; upstream sources retain notices.

Exceptions and RTTI are disabled. The selected `std::__throw_*`, assertion and
`abort` entry points trap; they **do not return success or emulate exceptions**.
Host forked children must die with SIGILL for impossible vector size and invalid
support hex input. Those fatal paths are not executed in the native desktop probe.
No streams/locale/`std::thread`/unwinding/`shared_ptr`/C++ TLS claim is made.

Kernel setup enables baseline x87/SSE2, disables OSXSAVE; scheduler FXSAVE/FXRSTOR
is exercised with four independently configured rounding-control words over 128
sleep/context switches, restoring round-to-nearest before numeric algorithms.
AVX is disabled. These checks are not a proof of every extended-register lane.

## Workloads and replay

```sh
bash workspace/webkit-tmc/tools/test-stdlib.sh
PYTHONDONTWRITEBYTECODE=1 PYTHONPATH=/var/tmp/tmc-python-release \
 TOOLROOT=/var/tmp/tmc-tools python3 workspace/webkit-tmc/tools/run-tests.py \
 --gigablock --output /var/tmp/tmc-gigablock-final/results/acceptance.json
```

Authored assertions (see dated evidence for actual results): 1,040 independent
CPython decimal parse oracle vectors, special/UTF-16/float/fixed/trailing-junk cases,
8,000 finite binary64 shortest-format/parse roundtrips, cross-thread destruction
of allocated vector/string handoffs, vector moves/copies/erases/sort/embedded NUL,
once-only static guard and native DSO destruction, 400 allocating `std::function`
tasks with exactly-once cleanup, 128 x87/SSE2 context checks, heap zero-residue.
The cache is explicitly opt-in at 256 KiB and drained at teardown. Full previous
unanchored C++ workload and its existing deadline remain unchanged.

The native QA command is injected into disposable diagnostic media, **not** the
retail release. Acceptance also rebuilds base+6 consumers and runs prior events,
heap, C++ runtime, lifecycle, pthread consumers on 1/4/8 CPUs, plus cleanup/active
FS2/4Kn/conversion/input regressions and native Recovery R-key boots. This does not
replace physical-hardware certification or installed BIOS/4Kn boot acceptance.

## Next dependencies, not delivered here

Full target libc/standard-library ABI selection, actual WTF threading/WallTime/
RunLoop/allocators and support classes, JSC generated sources and interpreter,
WebCore rendering/networking/TLS/fonts/images, then complete classic Safari UI.
This numeric component alone cannot load, execute, or render a web page.
