# FV003 / TMCBFS2 — development, not release acceptance

> The host-prepared-fixture results below are historical. The current
> fresh-installer and destructive re-enrollment macroblock is documented in
> [ENCRYPTED-ENROLLMENT.md](ENCRYPTED-ENROLLMENT.md); do not treat the older
> statements about missing create/reset APIs as the current implementation.

This is a **fresh-volume** authenticated block envelope. It is deliberately not
an extension of the fixed-geometry FV002/v1 FileVault format. The existing
plaintext installer remains unchanged. No in-place enable/decrypt/conversion is
exposed; encrypted factory reset is refused until its enrollment/key-erasure
policy is implemented. Do not advertise the old Settings activation button as
supporting conversion of plaintext TMCBFS2.

## Format and cost

- Physical I/O unit: **4096 bytes**, individually owned/aligned. Logical block:
  **16384 bytes**. Each logical block owns four ciphertext units plus one metadata
  unit. Overhead is **25% of plaintext capacity**, i.e. about 80% usable raw
  capacity before the nine reserved units. This is explicit, not hidden capacity.
- Units 0/1: redundant headers. 2/3: redundant journal descriptors. 4–8:
  ciphertext + complete metadata journal payload. Logical block `b` starts at
  physical unit `9 + 5*b`. Its metadata follows four ciphertext units.
- All addresses/capacities are u64. Maximum logical payload: 1 TiB. The host
  boundary test really uses a **1.25 TiB sparse backing file** for a 1 TiB logical
  payload, and writes/reads the last logical block. It does not claim that a raw
  1 TiB disk exposes a 1 TiB encrypted payload. The current kernel's raw device
  capacity limit remains separate.
- Only four physical writes create an envelope, irrespective of capacity. No
  capacity-sized tag table, zero-fill scan or capacity-sized contiguous buffer.
  The QA guest fixture helper separately wipes its small old test partition;
  that is NOT a feature or performance claim about the envelope.

## Cryptography and publication

- Existing pinned Monocypher XChaCha20-Poly1305, 16 KiB per authenticated record.
  AAD binds volume UUID, domain/version and logical block index. Metadata binds
  the UUID, block index, nonce and tag. Unknown/unwritten/corrupt records return
  an error and an entirely zeroed output, never unauthenticated plaintext.
- Random 256-bit volume key; PBKDF2-HMAC-SHA256 **600000 rounds** wraps its password
  slot. An independently derived slot wraps the same key using a random 256-bit
  recovery credential. Header fields are keyed-BLAKE2b authenticated; a public
  SHA detects torn headers but is not treated as authentication.
- Nonce = random per-volume 128-bit prefix + monotonic u64 counter. A range is
  written/flushed to BOTH authenticated headers before any counter is issued.
  Reopening burns the prior range. All I/O failures poison the context until
  probe/unlock/replay; no retries on ambiguous writes with the same nonce.
- Before touching the target record, ciphertext AND complete metadata are
  journaled, then both intent descriptors are flushed. Recovery authenticates
  the journal before replay. A poisoned/corrupt header is never auto-formatted.
- **Not anti-rollback:** old, valid same-index records, or whole-medium images,
  can be replayed. No monotonic external trust anchor or authenticated freshness
  tree is provided. Physical tamper/authentication is not a promise of freshness.
- Flush correctness is assumed. Native 4Kn transport/partition/boot acceptance
  is still a separate gate; using aligned 4 KiB I/O here does not close it.

## Kernel wiring

`fs/tmcbfs2_volume.c` dispatches plaintext or encrypted TMCBFS2 block I/O,
reports truthful logical capacity, handles status/password/recovery unlock and
keyslot rotation, and wipes cached keys when partition mappings are forgotten.
The legacy sector-I/O entry point refuses overlaps with an encrypted extent,
even while unlocked. The legacy vault must not inspect its fixed offsets inside
an FV003 extent. Header damage is also recognized by block zero's public
record-metadata marker, preventing fallback to legacy initialization.

Normal boot unlocks the volume before the imported account is authenticated;
Recovery keeps its RAM maintenance home and uses its existing FileVault panel
to unlock an offline encrypted home. Offline verify, local CoW backup and restore
use the same authenticated adapter. Plaintext reset still works; encrypted reset
and activation/deactivation/conversion are deliberately not exposed.

## Completed host evidence

`tests/test-vault2.sh`, normal + ASan/UBSan, actual production KDF parameters:

- Sparse 1 TiB logical boundary, constant-size initialization, password and
  recovery-key rotation.
- **120** write/flush/torn-4KiB/volatile-cache interruptions per mode: whole old
  or new block after authenticated replay; nonce ranges never reused after boot.
- **18** interrupted recovery-slot rotations per mode.
- Ciphertext/tag/index/nonce/UUID/padding and cross-block substitution rejection;
  corrupt committed journal refusal; one-header recovery/two-header refusal;
  a recomputed public checksum cannot forge the keyed header MAC; fresh create
  rechecks persisted blank headers even if its caller skipped probe.
- Real TMCBFS2 formatted through the envelope, including superblocks, trees,
  file data, private accounts, snapshots, checkpoint, lock/reopen/restore/GC.
- **Independent Python hashlib + libsodium** unwrap both real keyslots and
  verify header MAC and actual 16 KiB ciphertext, nonce and AAD. No exported
  plaintext volume key and no shortened KDF in the test.

`tests/test-fs2-install.sh`, normal + ASan/UBSan: actual kernel adapter over the
real installer-created GPT/MBR fixture; locked access denied, unlock, truthful
capacity, account import, offline verify/checkpoint/restore, lock/reprobe,
corrupt-header fail-closed behavior, sector-bypass guard and unrelated
FAT32/NTFS/RAW hashes unchanged. Encrypted DATA is explicitly host-prepared:
this does **not** certify an encrypted GUI install.

## Guest gate and remaining work

`tests/vault2_guest.py` runs only on a disposable actual GUI-installed NVMe
fixture matching the current ISO. `tests/fs2_encrypt_fixture.c` transforms its
DATA partition on the host with bounded RAM import after reading the QA home and
account, and is explicitly NOT crash-safe migration or a product installer API.
The guest gate tests cold password/recovery-key unlock, account authentication,
TextEdit 128 KiB I/O, native Recovery encrypted backup/change/restore, snapshots,
offline absence of plaintext DATA and unchanged public extents. Its result must
be checked before any guest-success claim; host results do not substitute it.

Still required before full release: encrypted fresh-install UI and end-to-end
policy, safe activation/deactivation/conversion or an explicitly accepted scope
change, encrypted reset/enrollment/key erasure, external scalable backup import,
native 4Kn installation/boot and the final complete regression/release matrix.
The published 10.2.4 stays untouched; TMCBFS2 remains opt-in, no 10.2.5 deployment.

## Current guest result (2026-11-18)

PASS on ISO `438c2c4428486ee7611d03d2765650aeca8f310bbcccef9382e4d7c8db773cf2`
and KernelCache `5fbbaddcaf62169b5afb417aad8dfb57257d9bcb9573fda0ccd365cb840e90b1`.
The actual GUI plaintext install/reinstall gate passed first on that same ISO.
All four encrypted guest checks described above subsequently passed, including
Recovery restore, cold recovery-key unlock plus account login, 128 KiB TextEdit
read/save and offline DATA plaintext scan. A scan establishes absence of the
tested markers, not a cryptographic proof or secure-erasure guarantee.

A separate SeaBIOS build with CONFIG_NVME disabled passed the no-helper negative
control and helper-assisted encrypted system/Recovery boot. Recovery unlocked
the encrypted volume, ran the ordinary same-app reinstall and verified the
volume. The entire encrypted DATA partition SHA stayed byte-identical through
reinstall; the helper source stayed unchanged. No physical hardware was tested.

Persistent receipt: `workspace/local-10.2.5/vault2-verification.json`, including
per-artifact hashes and original guest results. These passes still do not close
the encrypted-installer, conversion, reset, 4Kn or complete release gates above.
