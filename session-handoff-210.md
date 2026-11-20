# tinymacOS 2.1.0 — Session handoff (WS-B SMP done → WS-C+)

> update_session_memory tool is erroring; this file is the fallback state.
> Date 2026-11-18. Build id 20B1, version 2.1.0.

## CURRENT STATE (latest, 2026-11-18 ~16:15)
**WS-E STEP 1 DONE: live CLI matrix 16/16 + /sbin bug found&fixed; Drive ISO replaced; workspace purged.**

- **Workspace policy (user, hard rule)**: keep /home/user visible content well
  under the 128 MB snapshot cap — NO .iso/.zip/.img/test-artifacts in the
  workspace ever (deliver via Drive; builds/state live in /var/tmp/tmc210).
  Purged: root ISO (52M), tests/evidence+__pycache__ (16M), docs/storico
  evidence media+webm (~40M); workspace visible now ~16 MB (hidden .cache/...
  are excluded from snapshots anyway). tests/*.py scripts KEPT (needed, tiny).
- **NEW tests/cli_matrix.py (Retail driver, debug ISO, evidence in
  /var/tmp/tmc210/cli): 16/16 PASS** — enroll, boot [encdbg] serial assert,
  F2 Terminal, diskutil apfs list (Container cont0 + Crittografia), snapshot
  create x2/list, encryptVolume/lockVolume, apfs_unlockfv wrong-pass + ok,
  decryptVolume, verify/delete/restore (NOTE: restore reverts the snapshot
  catalog too — time-travel by design, cliuno vanishes post-restore: EXPECTED),
  fsck_apfs/mount_apfs/newfs_apfs honest paths, open -a "Time Machine" +
  GUI backup creation (Backup-... visible), Utility Disco opens clean,
  diskutil list, off -> ACPI S5. Retail.ocr monkeypatched (no tesseract here).
- **BUG FOUND & FIXED by the matrix: /sbin was unreachable** — cmdpath()
  (path.h) dirs, terminal.c completion dirs (many 3->4) and kernel
  spawn_command prefix whitelist all lacked /sbin -> fsck_apfs/mount_apfs/
  newfs_apfs could not exec. Fixed all three (+ Terminal help line lists the
  four new tools). WARNING: Retail builds have NO serial logs (by design) —
  serial waits only work on TMC_DEBUG ISOs; Retail tests assert screen pixels.
- **Disk Utility**: added "Crittografia" pair (two-column row with Contenitore
  at y254, values Disattiva/Attiva/Bloccata via EncInfo S_SNAP 6,0) in
  view==1 — compiles; pair text only shows on an installed volume (verified
  open-no-crash on blank boot; assert in the installed fixture next).
- **ISO re-cut + replaced on Drive**: release 54,177,792 B @16:12:34, SHA256
  1b1c322e0ba86b0e1769b7a48015c8067ad46b0aafb774a2179c9b7dac750aac MD5
  35f2242dd662b117f9eba0bd97720da5; smoke PASS (bbox canonical). Old Drive
  ISO (1vDk2kl0...) TRASHED (it lacked the /sbin fix); new upload id
  1ycc7qhErwBYwE78CTrPqYjhBWUbic62N
  (https://drive.google.com/file/d/1ycc7qhErwBYwE78CTrPqYjhBWUbic62N/view).
  NO workspace copy left (temp-copy for upload, then rm).
- Debug ISO same stamp (54,185,984) — runs the full matrix.

**Remaining WS-E**: riviera suite rerun vs BTRFS pins (audit_bytes/gpt still
read TINYFS1 + 1552-sector data -> update to TMCBFS1 @LDATA layout; plists
already 13/30), installed-volume boot for real fsck/recovery + DU Crittografia
pair + encryption UI assertions, boot_recovery.py (640,378 pixel looked
canonical in smoke, verify live), legacy GPT partitioned write-back migration,
strip [fsdbg][vmdbg][snapdbg][encdbg][hb][ap][vdbg][ovl][vec64] probes
(release is already probe-free via TMC_DEBUG gating; strip for debug hygiene),
then final re-ISO + re-upload + Italian delivery notes.

## PREVIOUS (2026-11-18 ~15:58)
**WS-D + macOS CLI + FileVault-style ENCRYPTION DONE; 2.1.0 ISO built, smoke-tested, UPLOADED TO DRIVE.**

- **macOS-exact tool naming (user directive)**: `/usr/sbin/diskutil` is THE tool;
  subtree `diskutil list | info | verifyVolume | repairVolume` +
  **`diskutil apfs list | snapshot list|create|restore|verify|delete <nome> |
  encryptVolume <pass> | decryptVolume <pass> | lockVolume | unlockVolume <pass>`**.
  Dedicated utilities: `/sbin/fsck_apfs [-y]` (real verify/repair via S_DISK 4/1),
  `/sbin/mount_apfs` (reports mounted volume), `/sbin/newfs_apfs` (honest guidance
  → Installer/Utility Disco; no runtime format op exists), `/usr/sbin/apfs_unlockfv`
  [passphrase|stdin] (S_SNAP 6,4). **`/usr/sbin/apfs` REMOVED** (no command named
  `apfs`). commands.def = **30 paths**. S_DISK op1 dispatch also allows program
  `fsck_apfs`. ⚠ diskutil apfs subtree must stay BEFORE the `n==3` info/verify
  branch (was ordered wrong once, fixed).
- **Kernel encryption (fs/btrfs.c)**: per-file flag = `Node.reserved`; header
  enc block: u32[6]@24 state, salt16 @28, verifier16 @44. KDF =
  PBKDF2-HMAC-SHA256 (crypto/pbkdf.c) 12000 rounds; keystream =
  SHA256(key‖rel_lba‖block), **rel = volume-relative** so live sectors and
  snapshot-image copies share keystream. Hook in `bytes()`: enc file +
  locked → return -1; unlocked → xform read/write ranges (byte-granular so
  partial writes stay consistent). Exempts system prefixes
  (/System /Applications /Library /bin /sbin /usr /Recovery) AND any path
  containing `/Library/` (prefs) → **locked volume still boots**. New files
  inherit flag at fs_create when armed. Toggle walks live nodes AND scrubs
  every snapshot image (extents + image node reserved bits + image header enc
  block + catalog crc recompute). Restore re-syncs state via `enc_load`
  (post-copy header read; salt mismatch → locked+key erased). S_SNAP **a==6**:
  b=0 status(OUT d EncInfo{state,locked,files}), 1 encrypt, 2 decrypt, 3 lock,
  4 unlock (pass in c). Boot: armed → starts LOCKED (FileVault-style).
- **encdbg (TMC_DEBUG sched hook) PASS** (debug ISO, LIVE=1):
  `[fs] volume encrypted; user files=2` →
  `[encdbg] mk=1 w=14 enc=0 files=2 lock=0 deny=1 ul=0 rd=14 txt=TopSecret-ABC dec=0 plain=14 fin=0`
  → `[fs] volume decrypted`. snapdbg suite still green (create/verify/restore).
- **Time Machine app** (`userspace/timemachine.c`): timeline of backups, usage
  bar, encryption status, "Esegui backup ora" (auto name Backup-YYYY-MM-DD-HHMM),
  Ripristina/Verifica/Elimina with confirm dialogs. Plumbing: NAPPS=16, enum
  `… STICKIES, TIMEMACHINE, INSTALLER, COMMAND` (TM=13), lastapp(normal)=TM,
  appname/appicon(42)/bundlename extended, **new icon tile 42 appended to
  assets/symbols.rgba (43 tiles, 704512 B), gfx.c icon() bound → id>42**,
  build.sh APPS += timemachine, system_volume apps[] + Time Machine bundle
  (→13 plists), shell.c Libreria counts 13/14 applicazioni. Open via Libreria,
  search, `open -a "Time Machine"`.
- **Monitor**: per-core bars (si.ncpu/core_busy/core_ticks) strip y176–228,
  window 464, Processi yy=236, hit regions +50 (sort 228–261, rows ≥282).
  **About** already original UI (2.1.0 · Riviera · Build 20B1) — untouched.
- **Tests**: riviera_media pins → plists/exes 13, message "13 bundle … 30 percorsi".
- **ISOs @15:57**: release **54,177,792 B** SHA256
  `849eeec605d2331a599e41b171257212e68e2b7f580a72ec627dc16a1b614db2` MD5
  `c1b2b5709e90146c80027e56beab1320`; debug 54,185,984 B. smoke.py PASS
  (login reached; bbox canonical y346–453 x596–683, center 399.5/639.5).
- **Drive uploaded**: `tinymacOS-2.1.0-x86_64.iso` id
  `1vDk2kl0ulSBbzvCtJ356eLXgF9Wz99Tg` (also workspace copy at
  /home/user/tinymacOS-2.1.0-x86_64.iso).

**Next:** WS-E — live-exercise CLI in Terminal (diskutil apfs full matrix +
fsck/mount/newfs/unlockfv + `open -a "Time Machine"`), Disk Utility encryption
row/UI (deferred), riviera suites (TMC_SMP=2), boot_recovery pixel pin, audit
bytes → TMCBFS1, legacy GPT migration write-back, strip probes ([fsdbg][vmdbg]
[snapdbg][encdbg][hb][ap][vdbg][ovl][vec64]) → final retail 2.1.0/20B1 re-ISO
+ re-upload + Italian delivery.

## PREVIOUS (WS-C)
WS-C BTRFS **KERNEL CORE DONE + VERIFIED** (2026-11-18 ~15:33):
- New format in `fs/btrfs.c` (renamed from tinyfs.c): container header
  "TMCBFS1" rev2 @sector0, nodes 1..12, accounts 13/14, factory 15,
  **real BTRFS superblock @64KiB (sector128, magic _BHRfS_M, crc32c,
  sectorsize/nodesize 4096, dev_item)** → blkid/btrfs dump-super can read it;
  snapshot catalog "SNAPCAT1" @sector136 (header magic/rev/count/nextid,
  entries@32, 8×48B SnapEnt); file data @144 (SECTORS=1680); snapshot images
  at 2048+k*1680 (8 slots, ends 15488); BACKUP_OFF moved 4096→16384;
  NDATA=2048, **NPART=16384** (Block bound, live+snapshots), backup = NDATA
  at BACKUP_OFF (fits min 64MiB disk: LDATA+18432 ≤ end).
- Legacy TinyFS: whole-disk (install media) → import to RAM with in-place
  relocation data16→144 (descending copy); partitioned legacy → validate nodes
  in RAM → relocate → write back (in-place MIGRATION) → active. Verified via
  legacy.raw boot: `[fs] legacy volume imported read-only into live RAM`.
- Snapshots: full-volume copy images (crc32c stream, readback-verified),
  create/list/verify/restore/delete + FsInfo op5; browse read-only under
  **`/.snapshots/<nome>/...`** (node-swap overlay under fs_gate; snapshot FDs
  encoded id=0x40000000|slot<<16|node; root listing shows virtual
  `.snapshots`; root dir is ro by design). VERIFIED in boot hook:
  create=0 list=1 verify=0 (crc match), stat inside image size=282, read
  "Benvenuti in ti", live file created after snap absent in image
  (mk=1 inimg=0 live=1), restore=1 liveafter=0 → point-in-time + restore OK.
- **S_SNAP syscall** (op0 list / 1 create / 2 restore / 3 verify / 4 delete /
  5 FsInfo) in abi.h + syscall.c (session_open guarded); k.h protos.
- **Terminal `apfs` command** (`/usr/sbin/apfs` added to commands.def —
  system_volume.py maps it): `apfs list`, `apfs snapshot create|list|restore|
  verify|delete <nome>` with dates + KB usage (FsInfo). diskutil label →
  "BTRFS subset".
- **Disk Utility rewritten APFS-style** (`userspace/diskutility.c`): sidebar
  Disco → volumi → **snapshot rows** (truncated names), detail pane for
  snapshot (ID/Data/Dimensione/Stato + Ripristina/Verifica/Elimina buttons),
  toolbar + "Nuovo snapshot" (auto name snapshot-YYYY-MM-DD-HHMM from si),
  format string "BTRFS subset (struttura APFS)", Snapshot/Contenitore pairs
  on data volume. Dialog confirm flows for create/restore/delete; verify runs
  immediately. NOT YET SCREENSHOT-TESTED (WS-E).
- maintenance.c `tiny_check` accepts TMCBFS1+super magic AND legacy; reset_data
  writes TMCBFS1+factory. install.c bread/bwrite/Block/mount guard → NPART.
- Builds @15:33: release 54,173,696 B (0 warnings), debug 54,177,792 B
  (0 warnings); smoke.py PASS (login reached; bbox noted wider — check in WS-E).

## PREVIOUS (WS-B, still true)
WS-B SMP **COMPLETE AND VERIFIED**: both cores tick at matched rates (t0≈t1),
tasks migrate cpu=0↔1 (heartbeat alternates), boot sequence `[smp] ap up id=1 /
[sched] smp done / [ap] tables / [boot] services ready / [ap] online`,
Triple fault=0. Release build ZERO warnings; smoke.py PASS (logo center
399.5/639.5, login reached, TMC_SMP=2).
- release ISO: `/var/tmp/tmc210/release/tinymacOS-2.1.0-x86_64.iso` 54,161,408 B @14:57:32
- debug ISO: `/var/tmp/tmc210/debug/tinymacOS-2.1.0-x86_64.iso` 54,169,600 B @14:57:41

Next steps:
1. Strip debug probes before retail: `[cal]`, `[hb]`, `[vec64]`, `[ap]` probes,
   `[spawn]`/`[load]`/`[timer preempt]`, idle rflags (all TMC_DEBUG-gated; retail
   unaffected either way). Keep real fixes (PIT parity, SVR, DCR, serial lock...).
2. WS-C: BTRFS real subset behind APFS-style wrapper (Disk Utility + Terminal
   container/volume/snapshot UX). Validator: Debian btrfs-progs 6.14-1 deb,
   SHA256 `ae3ab86d467b8b765ea1ea3e264c11a07a6f8a73e6b3df665c787d45b9b3491f`.
3. WS-D: About/monitor/disk UI with original macOS UIs, apple reposition
   (done: `Y=h/2-54`), Time Machine app (same-disk snapshots).
4. WS-D next: Time Machine app (NEW app package — NAPPS 15→16 + enum append
   after COMMAND + Info.plist package like other apps; tests assert 12 plists
   → 13), Monitor per-core UI (SysInfo core_ticks already fed), About with
   original macOS UI; then probe strip ([fsdbg]/[vmdbg]/[hb]/[snapdbg hook in
   sched.c]/[vec64]/[ap] probes/[vdbg]/[ovl] + cal) → retail → WS-E QA:
   update boot_recovery white pixel (bbox also observed 371-428/543-735 in
   latest smoke — investigate), run riviera suites with TMC_SMP=2 (gpt/audit
   pins now expect TMCBFS1 format!), legacy-migration GPT test.
5. WS-E: update `tests/boot_recovery.py` white pixel (640,378); run riviera
   suites with TMC_SMP=2 (`<src-iso> <out-dir>` + TMC_BUILD env, Retail harness,
   password `local-test-285`, initial `Elena`).
6. Italian delivery: ONE ISO 2.1.0/20B1, link + entry instructions, packaging
   like 2.0.2. **No ISO/ZIP under /home/user ever** (`tools/media_path.py`).

## USER REQUEST (verbatim)
«Ora voglio multi core, sfonde limite RAM, multithreading, system monitor più
completo (e stessa UI dell'originale), disk utility stessa UI dell'originale.
Ho notato che la mela è in posizione sfasata. Inoltre passiamo da TinyFS a
BTRFS, che viene utilizzato ovunque. Con vero sistema di snapshot e crea anche
l'app Time Machine, con stessa UI macOS. Ora anche info su tinymacOS deve
avere stessa UI dell'originale. Poi nuova ISO»

Scope decisions (ask_user): BTRFS = real on-disk subset through APFS-style
wrapper («Sottoinsieme BTRFS reale + strumenti apfs su disk utility e terminal
(é wrapper, il filesystem é BTRFS ma strutturato come APFS)»); Time Machine =
snapshots on the same disk with macOS UI; deliverable = one ISO 2.1.0/20B1
like 2.0.2. Baseline 2.0.2 still standing: BaseSystem inside ISO and on disk
after install; native x86-64; delivery in Italian; reply with one ISO link +
entry instructions.

## SMP FIX CHAIN (all applied in-tree, rebuilt, verified)
1. vm_pte_l deadlock → page_get_l (Bug A).
2. GDT/trampoline slot collision → aptramp relocation to 0x8E00 (Bug C).
3. PIT parity: `pit_wait` exits at counts-1 (`pit_elapsed(s)+1 < counts`).
4. `tramp_img[3622]` decl (was [2086] → silent truncation, nm must be 0xe26).
5. gdtr base 0x8E06 (6-byte header): bytes `1f 00 06 8e 00 00` @array 0xE00.
6. Identity low-2MiB no NX (`pd[j]=a|0x83` for a==0) — trampoline fetch.
   Keep EFER.NXE=1 (kroot PTEs carry NX bit; NXE=0 = reserved-bit fault).
7. Far jump to CS.L=1 must be 32-bit operand: jump2 = `ea 60 80 00 00 18 00`
   (NO 0x66 in D=1 CS, else #GP(0)); jump1 (real→pm32) KEEPS `66`.
   Length-neutral pad `90` at 0x5F keeps gdtr at 0xE00. decl stays 3622.
8. log.c: static `Spin slog` (2-CPU serial interleave) + NUL-terminated
   `b[25]` (com1 read past buffer).
9. ncpu double count removed (init=1 + ap_main only ⇒ ncpu=2).
10. **SVR=0x1FF per CPU**: APs arrive from INIT with SVR=0xFF (SW-disabled) —
    BSP only works because OVMF programmed 0x1FF. `lapic_periodic()` now
    writes `l[0xF0/4] = 0x1ff`. Symptom was: AP timer count runs, LVT ok,
    TPR=0, IF=1, but zero interrupts delivered.
11. **DCR /16 per CPU**: AP DCR resets to /1 → timer 16× fast.
    `lapic_periodic()` sets `l[0x3E0/4] = 3`. t0≈t1 after fix.
12. Probes gated `#ifdef TMC_DEBUG` (smp.c AP block, sched.c idle) — retail
    zero warnings.

## KEY PATHS
- `N=/home/user/tinymacOS` — sources. `W=/var/tmp/tmc210/original/` pristine.
- Debug aids: `/var/tmp/tmc210/{smoke.py,shot.py,regs.py,regs2.py,serial.log,qlog.txt,smoke-state/}`.
- Builds: `TOOLROOT=/tmp/tmc-tools/root TMC_BUILD=... [TMC_DEBUG=1] bash build.sh`.
- Fault capture: append `-no-reboot -d int,cpu_reset -D qlog.txt` to run-qemu.sh.
- QEMU: Q35/OVMF 1280×800, xHCI kbd p1 + tablet p2, `-nic none`,
  `TMC_SMP`/`TMC_MEM` defaults 2/1024.
- `V=/var/tmp/tmc202` = 2.0.2 QA; preview :8084 = 2.0.2 console.

## DELIVERED BASELINE (2.0.2/20A3, Drive)
[ISO](https://drive.google.com/uc?id=1KMBbKetxhhZgVnsp7bL4GCBTDI9vrSCy&export=download)
ID `1KMBbKetxhhZgVnsp7bL4GCBTDI9vrSCy`, 54,149,120 B,
SHA256 `26d159bb37587c11eb6fec9a173730a6519e7af2b546b7ca7fc733b1987d3c74`,
entry R/F12. Historical: 2.0.1 `1-Ja9qcEr34D8eSdNr9vsUULIwaleUHoI`; 1.3
`16fx2iSZpYGB7Zflvncaq726rc-CMmzTb`; no ACL changes ever.

## PITFALLS (top)
- Byte-array edits: verify `nm -S`; preserve fixed offsets (gdtr@0xE00) via
  padding or re-derive base. Python multi-step edits: assert every step.
- `-d int` qlog: read last `check_exception` + full GDT/CR0/CR4/EFER/CR3 frame.
- objdump `--disassemble=<symbol>` only. QMP for per-vCPU registers.
- Retail = TMC_DEBUG off: logstr no-op ⇒ probe locals become unused warnings
  → gate with `#ifdef TMC_DEBUG`.
- boot_recovery.py white pixel (640,378) invalid after logo move → WS-E.
