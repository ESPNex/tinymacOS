## Current v18 — whole original Linux EHCI, USB macroblock still incomplete

**Non pubblicata. PS/2 escluso dal lavoro residuo su richiesta dell’utente.** Il riferimento corrente è `v18-validation.json`; i risultati storici sotto non certificano il nuovo kernel.

EHCI usa ora i file completi originali Linux v6.12 HCD/queue/memory/schedule/root-hub/timer/PCI/sysfs, con funzioni originali HCD_BH e worker TT-clear estratte byte-exact. Runtime reale per PCI, coherent DMA/pool, lock IF-preserving, timer/htimer, polling e lavoro differito. L’enumerazione e il confine di classe UsbInput rimangono separati; non è un port completo USB core/hub/usbhid.

Corrette due regressioni native: QH EP0 con indirizzo zero memorizzato dopo SET_ADDRESS (ora ritirato con endpoint_disable originale), e deadlock del reinvio dalla callback interrupt sotto il lock EHCI (ora giveback HCD_BH originale). Le completion BH hanno un esecutore indipendente dal worker TT, perché quest’ultimo attende veri controlli USB sincroni. Il modello host controlla lock e completing_ep, 16 reinvii immediati, full/short/STALL/cancel/timeout, topologia HS hub/FS child e CLEAR_TT_BUFFER reale, 38 rollback di inizializzazione. Il contratto DMA non usa più il vecchio EHCI nativo come fixture: esegue il nuovo controller originale per 128 cicli completi con bus non-identity.

Validazione: **30/30 profili USB BIOS/UEFI** (12 mouse, 6 tastiera italiana/LED, 10 tablet/generic, 2 doppio mouse), sei osservatori EHCI prima/dopo input con contatori originali in crescita, **14 suite host / 15 ricevute sorgente-verificate**, ASan/UBSan O1/O2. Alignment UBSan escluso soltanto per le sentinelle list_entry upstream; l’allineamento DMA è verificato. Nessun test PS/2 richiesto o conteggiato. Le build indipendenti hanno kernel/cache/ISO nativa/ISO dualboot identici; anche l’ultimo rebuild dopo gli aggiornamenti dei commenti conserva gli stessi byte.

ISO privata v18: **225908736 byte**, SHA-256 `17ba26a8750cf8fd7d07a46c04999d21a144e4c45a3045a6211d383617b95bd9`. Primo packaging indipendente fallito per spazio disco sandbox: cancellate soltanto due ISO derivate dei precedenti smoke falliti, poi packaging ripetuto con successo. Per il budget snapshot, il vecchio rapporto visivo 2.4.1 è conservato integralmente e senza perdita nell’archivio XZ adiacente; nessun sorgente corrente rimosso.

**Restano incompleti:** xHCI originale (ancora nativo), USB core/root-hub/device lifecycle condiviso, hub completo e usbhid/HID bus completi. Polling non equivale a IRQ hardware o PM di sistema. I test QEMU non sono una prova fisica ASRock. Installazione/Recovery/kernel/licenze devono superare il gate finale dopo il completamento del macroblocco: non è dedotto da queste regressioni USB. Nessuna release, upload o push di questo incremento; release stabile 10.1.6 e main pubblico v17 invariati.

## Historical v17 — whole original Linux UHCI, no deployment

Sostituito lo scheduler UHCI nativo con i file originali completi Linux v6.12 HCD/queue/hub/PCI, byte-exact e verificati nel manifest. Runtime reale per coherent DMA/pool, timer cancellabili, lock IF-preserving, PCI e dispatch polling; originali anche URB e funzioni HCD lifecycle. Contratto host O1/O2 ASan/UBSan passato: esecuzione DMA non-identity di controllo/interrupt, full/short/STALL/cancel/timeout, toggle, reset/status root hub, rollback e 128 cicli senza leak. UBSan alignment escluso per i pseudo-oggetti sentinella list_entry upstream, non ASan né gli altri controlli UBSan.

Validazione finale: **34/34 guest BIOS/UEFI**, **14 contratti principali legati agli SHA dei sorgenti**, suite manutenzione/legacy, contratti ausiliari EHCI qTD/Intel e identità/OpenCore/licenze nei tre media passati. Sono osservati prima/dopo l'input **10 profili con UHCI originale**, oltre a 16 DMA e 28 message. Il banco DMA misto inizialmente pretendeva due allocazioni nel vecchio facade anche quando il periodico era ormai originale UHCI; corretto distinguendo i controller attivi, mantenendo i controlli su memorie vive/errori/attributi. Tutti i 16 profili DMA sono stati rieseguiti e superati col controllo definitivo. Due build/packaging indipendenti identici: **225851392 byte**, SHA-256 `dfe113a908b61dd6d5a8c353a8b927df957566881f6cb40f5fae11282766767b`. Sorgenti sincronizzati prima dei test finali: `1a7e04e40efcb64d7edf7f002c9ad2cc6d75c05d`.

Classe/enumerazione UsbInput ancora separate; SG/mapping esterni, IRQ hardware, PM, hotplug generale e core/usbhid non completi. xHCI/EHCI restano nativi. PS/2 per ultimo. Macroblocco richiesto ancora FALSE, gate finali kernel/installazione/Recovery non dedotti da questi test; nessuna prova ASRock fisica o nuova release. Per il limite snapshot è stata compattata soltanto la formattazione di un vecchio audit JSON, mantenendone invariato il contenuto.

# 10.1.7 / 30B8 — development, not published

Current status is machine-readable in `STATUS.json`. Every guest receipt identifies its actual ISO; older successful tests do not certify a later binary.

## Implemented port milestone

Linux v6.12 `usbmouse.c` is preserved byte-for-byte upstream, compiled and linked into the native kernel. The implemented USB/input compatibility subset receives real interrupt packets from **native** xHCI/EHCI/UHCI transport. The historical v3 twelve BIOS/UEFI topologies verified live driver completions plus 72 exact rendered cursor positions; their ISO hashes do not certify v5. Host sanitizers cover allocation failure, cancellation/reopen, held-button release on disconnect and 4,000 concurrent lifecycles. This is a Boot mouse HID port, not Linux HCD/general USB-core/generic-HID support.

## Recovery

Firmware relative counts are converted using counts/mm, with per-device signed fractional state. The direct USB mouse transport is not linked into Recovery. Keyboard transport now uses OpenUsbKbDxe’s Apple keymap and the declared derived Ps2KeyboardDxe raw-set-1 protocol. No native keyboard transport, direct i8042 I/O or UsbIo keyboard requests are linked into Recovery.

The attempted Apple keymap/KeySupport combination duplicated USB input. A SimpleTextInputEx PS/2 adapter lost correct punctuation. Both implementations were reverted; their failed receipts remain under `attempts/`. The subsequent firmware implementation avoids both failures: KeySupport stays off, USB remains on OpenUsbKbDxe, and PS/2 uses physical bytes mirrored by the firmware driver rather than guessing positions from Unicode. Four current v3 guest profiles pass the exact Italian punctuation string, AltGr and UTF-8 backspace. The derivative is separately named and source-locked; original OpenCore binaries/lock stay untouched.

## Harness/resource failures retained

- A concurrent BIOS staging attempt lost QMP when the host OOM killer terminated QEMU. The OOM trace and failed receipt are retained; a memory-bounded rerun passed. Do not run more than two guests in this 2 GiB workspace.
- Control Center initially failed a flat-pixel text template on its glass backdrop although the panel was visibly open. The harness now uses cropped pixel/OCR verification of the actual labels, as for its other glass controls; no click is treated as proof.

Through v3, About source and executable were unchanged from 10.1.6 / 30B7 by the earlier request. The latest user instruction reverses that exception: v4 updates only the version/build strings to 10.1.7 / 30B8, retaining the layout. The icon manifest's historical 10.1.6 provenance note is likewise unchanged. No QEMU result is physical ASRock compatibility evidence. Final publication remains blocked on the remaining implementation and complete release gates.

## Historical v3 integration

ISO SHA-256 `cfed0081712277fd25b7020f56c15fe52e252316a42f5c1c64d116ca200fae03`, 225718272 bytes; two independent full builds are byte-identical. The linked KernelCache is byte-identical to v2 (the tested 29-case/13-profile kernel). Firmware/loader/Recovery changes still require their own current receipts; old v2 ISO-specific input receipts do not certify v3. Current v3 has four keyboard profiles, UEFI+BIOS staging/deletion/Recovery mouse and media/identity contracts. Linux HCD/core/generic HID and the complete final release gate remain unfinished; no deployment.

The subsequent v3 retest also passed all twelve Linux Boot mouse topologies (72 exact rendered XY checks, live upstream completion counters) and the complete Control Center guest checks. These receipts identify the v3 ISO, not v2. Kernel evidence is carried only through the exact-byte comparison recorded in `kernel-binary-equivalence-v3.json`; it is not reported as a newly run kernel suite.

## Historical v4 integration

Current v4 adds the full unmodified Linux EHCI `qtd_fill` function to native EHCI descriptor construction, not the full HCD. Host sanitizers cover 147672 page/DMA64/packet combinations plus rollback. UEFI+BIOS EHCI guests independently verify live qTD-build counters, upstream Boot mouse completions and rendered XY. Shared Intel mux validation now checks current routing registers before either write and confirms readback; six controller-family fixtures include Bay Trail 8086:0f35. These fixtures do not emulate Bay Trail electrical behavior or certify ASRock hardware. About displays 10.1.7 / 30B8 by the latest instruction. Kernel bytes changed, so the old v2/v3 exact-kernel equivalence must NOT be carried to v4. Remaining controller/core/generic-HID work and final release gates still block deployment.

## Historical v5: complete upstream Boot keyboard, not complete USB stack

The full byte-exact Linux v6.12 `usbkbd.c` is compiled through `compat/linux/usb_keyboard.c`. Native xHCI/EHCI/UHCI feed its interrupt URB. Its actual input events enter the sole Italian map; registered keyboards merge held-key bitmaps/modifiers without a six-key aggregate limit. The original upstream LED event/completion state machine now reaches native EP0 through a bounded deferred control adapter. Generation and submission-serial validation guard cancellation/disconnect/rebind while that native wait is outside the compatibility lock.

ASan/UBSan executes seven allocation rollback points, Italian Shift/AltGr/accent/euro/ISO keys, Caps/repeat, two-keyboard shared keys/modifiers and twelve held keys, invalid/rollover packets, interrupt cancel/reopen, LED coalescing/reentry, same-URB resubmit ABA, concurrent disconnect/rebind ABA, STALL and leak-free teardown. Six actual ISO guests pass: UEFI+BIOS on EHCI, UHCI companion and xHCI hub, with i8042 disabled. Each records 180 upstream IRQ completions/frames, two native LED controls and zero rejected/control-error/stale completions. This verifies virtual devices, not ASRock/Bay Trail hardware.

The v5 kernel differs from v4. Earlier full staging, recovery, media and runtime receipts must not be treated as current-binary certification. Linux HCDs, general USB core/hubs, generic HID and the complete final install/input/kernel/license gate still block deployment. Source synchronization only; no release or ISO upload.

## Historical v6: full upstream core/urb.c, still not a full Linux USB port

The entire byte-exact Linux v6.12 `drivers/usb/core/urb.c` now builds and supplies the actual URB API, replacing the handwritten allocation/free/submit/kill implementations. Source is GPL-2.0-only, separately identified in the source manifest. The compatibility layer supplies real reference/list/atomic/IRQ-lock/wait services; optional debug/sanitizer instrumentation is disabled as in non-debug upstream configurations. The backend remains a bounded native-HCD adapter (one interrupt-IN and EP0-OUT slot per binding, eight bytes maximum, no bulk/isoc/SG/IN-control), not Linux `hcd.c` or a full scheduler/core.

Host ASan/UBSan covers reference/size overflow, active-driver-put and callback-free lifetime, short packets, state/endpoint/packet/SG/control/interval checks, slot backpressure, kill/unlink/poison/block, every anchor API, real timeout and concurrent wakeup. Mouse/keyboard host and native maintenance/legacy suites pass. A missing `hid_linux_keys` declaration caused by the overlay's earlier reverse copy was restored in both canonical and overlay headers; the final builds compile and the overlay check is run read-only. Do not use `--apply` to copy canonical edits into the overlay: it copies in the opposite direction.

The final v6 ISO is independently reproducible and remains private. All twelve mouse plus six USB-only keyboard QEMU profiles were rerun on its exact KernelCache. Besides rendered input, each observes live upstream URB submissions/givebacks and the invariant `submits = givebacks + active`, with zero backend rejects. This is 18 virtual configurations, not physical ASRock/Bay Trail evidence. Older staging/Recovery/kernel receipts do not certify this changed kernel. Complete Linux controllers, remaining USB core/hubs, generic HID and final release tests still block completion and deployment.

## Historical v7: mouse button aggregation and controller error handoff

The Linux input adapter now merges held buttons across registered Linux-bound mice, while each report supplies only its own signed X/Y/wheel. Native xHCI/EHCI/UHCI error branches hand the failed logical interrupt URB back through real upstream core/usbmouse code; they no longer leave the affected mouse's held buttons stuck while native recovery is pending. Transient failures resubmit, terminal shutdown retires the URB. Native controller implementations are still not Linux HCDs; generic HID and the rest of USB core remain unported.

Host ASan/UBSan checks 1,024 paired five-button states, idle/release/unregister aggregation, transient STALL/EIO, all three terminal statuses, reopen, malformed status and leak-free teardown. The actual xHCI completion/reset path additionally verifies button release, native clear-halt/reset/rearm, resumed X/Y and terminal retirement with the real Linux mouse/URB attached. EHCI/UHCI error branches compile in the actual kernel; this is not a physical hardware fault-injection claim.

All twelve mouse and six keyboard profiles were rerun on v7, plus two UEFI/BIOS two-mouse profiles that check actual rendered Terminal-window movement across cross-device drag/release transitions. The first two-mouse harness attempt incorrectly used a flat font template on the glass titlebar; the next did not account for HMP's global button mask when switching the target mouse. Both failed receipts remain in `attempts/`. The corrected harness checks the pixel-exact rendered content rectangle and deliberately generates each selected device's button edge; no OS code was changed to accommodate the tests.

Full Linux controllers/core/generic-HID and the final install/kernel/Recovery release matrix remain outstanding. No deployment or ISO upload. The latest completed public release remains 10.1.6.

## Historical v8: genuine Linux PS/2 command/RX library; four-path scope explicit

Complete original Linux v6.12 `libps2.c` and header (GPL-2.0-only, hash-pinned) are now compiled through a separate compatibility wrapper. Kernel PS/2 keyboard setup and mouse wheel negotiation call original `ps2_command`; data and command responses pass original `ps2_interrupt`. Native i8042 I/O, Set-1 keyboard/PS2 mouse byte decoders and timer-safe LED machine remain native. This is not a complete `i8042`/`atkbd`/`psmouse` port. Blocking Linux PS/2 commands run during boot, not from the PIT handler; runtime LED ACKs use native nonblocking handling through Linux RX.

Host sanitizers cover ACK/NAK/resend, write failure, no-ACK mouse ID, keyboard ID, truncated response, partial reset, sliced command, bounded drain, real timeout, parity/timeout abort, independent RX locks/shared command mutex, interleaved input and callback-command rejection. UEFI+BIOS PS/2-only guests each execute nine real Linux setup commands, thirteen transmitted bytes, four-byte mouse wheel negotiation and zero command/RX errors. Actual Italian keyboard text/Caps/LEDs, six pixel-verified X/Y positions, and visible forward/back Terminal scrollback pass. The initial wheel harness used the opposite HMP direction; its failed receipts remain under `attempts/ps2-wheel-hmp-direction/`. Only the harness changed; OS code was unchanged for the passing rerun.

All twelve USB mouse, six USB keyboard and two mixed PS/2-keyboard/two-USB-mouse profiles were rerun on v8. The mixed cases observe original Linux PS/2 RX counters during enrollment, plus actual USB window dragging. Full Linux controllers, remaining USB core/hubs/HID, PS/2 class drivers and final release validation still block deployment. No physical Bay Trail result is claimed.

Workspace guard counts all files, reports both decimal MB and MiB, and enforces a conservative 120 MiB ceiling below the user's 128,000,000-byte limit. Private builds/ISOs and downloaded research remain under `/var/tmp`, not in snapshots; frozen 10.1.6 receipts remain untouched. Every verified source milestone is synchronized to GitHub before any future binary publication.

## Historical v9: original psmouse packet/detect functions, shared PS/2+USB pointer ownership

Native PS/2 arithmetic/wheel decoder replaced with byte-exact complete original Linux packet/report functions. IntelliMouse/Explorer boot detection also executes original Linux functions using real libps2 AUX commands. Vendor source+header and both extraction ranges are hash-pinned; GPL-2.0-only provenance preserved. Native i8042 and stream framing, keyboard decoder/LEDs, remaining psmouse lifecycle/touchpad protocols and full Linux USB HCD/core/hub/generic HID remain incomplete.

Two PS/2-only ISO profiles pass with sixteen Linux commands, twenty-six transmitted bytes, Explorer four-byte negotiation, zero command/RX errors, Italian keyboard/Caps/LED, six exact rendered XY positions and bidirectional wheel. The twenty USB/two-USB-mouse profiles were rerun on this ISO, plus two new real PS/2+USB pointer profiles: **24 current profiles pass** across UEFI+BIOS. The latter verify actual Terminal window drag and button ownership survive both cross-transport release orders, not merely counters. Host ASan+UBSan runs at O1 and freestanding O2 check 3072 sign/axis combinations, full signed IMPS wheel, Explorer five buttons and wheel formats, stream recovery, 1024 PS/2+USB button states and failed negotiation rollback. Desktop horizontal scrolling is not implemented; the input path now delivers its independent horizontal component in EV_SCROLL.b.

The first v9 boot failed under optimized compilation. Its receipts are preserved under `attempts/psmouse-adapter-optimized-guard/`; the initial private adapter used a redundant embedded-device identity guard and its optimized probe fell through after the first command. The private adapter is now explicitly bound to the sole native AUX port instead of that identity assumption; both freestanding O1/O2 sanitizer runs and actual BIOS/UEFI boots pass. The first packaging attempt also ran out of external temporary disk space, without touching the workspace or published release; a byte-verified redundant private ISO was removed before retrying. Old source-synced v7/v8 private output directories were removed only after replacement guest checks passed. Source and evidence remain in Git history.

Independent normal build and dualboot packaging match byte-for-byte. Current ISO optical/installed/About/Recovery identity and actual OpenCore quiet/firmware input-driver media contracts pass. Final full install/kernel/Recovery release gates are still false; no deployment or physical Bay Trail claim.

## Historical v10: complete Linux USB configuration/BOS core

Entire unchanged GPL-2.0-only core/config.c now compiles through an explicitly private descriptor ABI, separate from bounded URB objects. Native controllers now use original configuration/interface/endpoint caches for HID and hub-class selection at actual bus speed. Endpoint repairs, alternate settings, SS/SSP companions, original config/BOS reads and teardown execute real upstream code. The native transport gate rejects truncated/malformed class descriptor streams and duplicate selected HID endpoints. Native HCDs, hub power/reset/enumeration, 1024-byte/first-configuration/alternate-zero policy and non-Boot report decoder remain; this is not complete HCD/core/hub/generic HID porting.

Host ASan+UBSan freestanding O1/O2 pass allocation and transport rollback, 8-config cap, interface/alternate/endpoint caches, duplicates, all speed repairs, SS/SSP descriptors, BOS EXT/SS/container/SSP/PTM, replacement/free, malformed inputs, 4096 deterministic mutations, every truncated composite prefix and 1024 leak-free lifecycles. The first BOS fixture used a 16-byte SSP capability, whereas upstream's minimum is 20; the fixture was corrected to include both declared speed entries. No upstream source was modified.

All 24 actual-ISO input guest profiles pass on v10. All 18 USB mouse/keyboard profiles verify original config.c execution, descriptor reads, parsed endpoints and zero outstanding temporary allocations; nested hubs require successful parsing of both hub devices and the mouse. Native and upstream host suites pass.

Current development validation also passes: UEFI+BIOS desktop deletion/Finder/About/quiet-default checks, both Recovery firmware mouse profiles and four isolated Recovery Italian PS/2/USB keyboard profiles; Control Center; actual BIOS GUI installation followed by HDD-only BIOS boot, persistent data and verbose setting across reboot, and UEFI cross-boot of the same disk without CD. An independent FAT readback confirms the installed ESP's actual KernelCache equals the current build. Two independent normal builds/dualboot packages match byte-for-byte; current identity/OpenCore/media and firmware-keyboard host/object contracts pass.

The current production kernel passed 29 backend/runtime cases: eleven host/target checks, five freshly built disposable probes, thirteen real guest profiles (TCP+VM at 1/2 vCPUs, synchronization/TLS/managed threads at 1/4/8). Each of the five probe media independently verifies actual packed KernelCache bytes, kernel hash and current source hashes. Probe applications are QA-only and are never deployed. A first harness run attempted two network profiles concurrently on the same fixed local peer port; the second bind failed. Those failed receipts remain under `attempts/kernel-network-fixed-peer-port/`. Network profiles now run serially; all thirteen rerun profiles pass. OS code did not change to fix that harness.

Kernel guest tools now accept an external `TMC_QA_RESULTS` directory. Full probe frames/logs/manifests remain under `/var/tmp`; only small final JSON receipts are synchronized. Generated external OCR/snapshot PPM scratch was removed while retaining evidence PNGs and receipts. This avoids transient workspace growth above the user's budget, rather than merely shrinking files after tests finish.

Despite passing current development tests, complete Linux controller/core/hub/generic HID and PS/2 class-driver porting are not done. The final completed-port release gate remains false, as do physical-hardware claims; no deploy.


## Historical v11: genuine Linux HID parser function block, not full HID drivers

Preserved the complete original Linux v6.12 `hid-core.c` and HID header with hashes/provenance. Five exact extraction ranges compile the descriptor parser, report/field/collection allocation, open/close lifecycle, signed conversion and resolution multiplier functions. They run in a private parser ABI, not the older Boot-driver ABI. Native xHCI/EHCI/UHCI now derive pointer offsets, usages, ranges and report-ID framing from the Linux field model. Native packet/pixel event decoding remains separate; firmware Recovery is unchanged. No HID device model, bus registration, `hid-input` event dispatch or `hid-generic` driver is claimed complete.

The OS adapter uses real kernel heap allocation, a 2 MiB aggregate limit, explicit ownership and nonblocking serialized parsing. Failed allocations cannot silently produce a partial successful field model. A checked boundary validates feature-multiplier arithmetic before invoking original upstream evaluation. O1/O2 freestanding ASan/UBSan pass: relative/absolute XY, buttons/wheel, IDs, keyboard modifier/array/LED descriptors, feature reports, dynamic collection growth, truncation/malformed data, every allocation rollback, heap exhaustion, nested calls, four-thread owned-object lifetimes, 4096 deterministic mutations and 1024 reopen cycles. Peak bounded test heap is 2096926 bytes, with all allocations freed. The first test fixture incorrectly required the generic parser to reject syntactically complete empty prefixes; Linux permits those. The corrected test still requires every incomplete prefix to fail pointer selection. No original parser function was patched.

All **24 actual-ISO input profiles** pass again on v11; all 18 USB mouse/keyboard profiles independently observe original HID parser execution and matching allocations/frees, zero live parser bytes and zero parser failures. Both firmwares also pass installation/HDD-only persistence/reboot/UEFI cross-boot, desktop deletion/Finder/About/quiet defaults, Control Center, Recovery mouse and all four isolated Italian Recovery keyboard profiles. The fresh production-kernel suite passes **29 cases / 13 runtime guests**. Eight upstream host contracts plus native maintenance/legacy suites pass. Two independent builds/dualboot packages are byte-identical: ISO 225759232 bytes, SHA-256 `e570e952315f7ebe68d6042b5ef113bed93af4ed9609a186bfa2dacbaaf24de7`.

All component notices are now copied to both optical EFI.IMG and Install.esp, not only the optical root. Media tests read back all notices from all three FAT boot paths, and the actual GUI-installed disk's ESP readback independently matches all seven source notices, including Linux GPL. This fixes notice coverage, not the outstanding full corresponding-source/release audit.

Source-first milestone `4a166d63ee825c5295b42c6c9b098bba87bc1e2f` was publicly verified before completing the new guest matrix. Builds/evidence remain external; disposable OCR/snapshot PPMs were reclaimed while evidence PNGs and receipts were retained. Workspace remains below 128000000 bytes and the stricter 120 MiB guard. **Full Linux HCD/core/hub, HID bus/generic input drivers and PS/2 class-driver port remain incomplete. No binary deployment or physical ASRock certification. Stable 10.1.6 is unchanged.**


## Historical v14: original HCD lifecycle and USB2 hub debounce/reset

The four-part request is still **incomplete**. Whole original Linux generic/input remains active on native endpoint transports. This increment additionally replaces manual URB submission/unlink/direct-giveback and endpoint queue/state handling with complete byte-exact original functions from `hcd.c`. Physical xHCI/EHCI/UHCI DMA, schedulers, rings and most device/root-hub lifecycle are still native/unported. Merely vendoring full hcd.c is not compiling its remaining functions.

Original `hub.c` debounce/reset functions now run in native xHCI and EHCI USB2 hub child enumeration. Services consume actual EP0 byte counts, return real failures, preserve the 100ms stable/2000ms debounce policy and bounded reset retry/recovery delays, and share genuine reader/writer exclusion with native EHCI CONFIGFLAG writes. Other hub lifecycle, registration, disconnect/hotplug and SuperSpeed services remain unported; SuperSpeed is rejected by this adapter. Its private view is not a unified Linux USB device ABI.

O1/O2 ASan/UBSan cover original endpoint enable/device-submit/HCD-state gates, queue membership, unlink-status precedence, root rejection, reference and anchor ownership, callback requeue/free, USB2 bounce/timeouts/short status/error/overcurrent/disconnect, five reset attempts, reconnect retry, recovery delay, CONFIGFLAG writer exclusion and 1024 hub port lifecycles. Existing Boot/generic/input regressions remain required.

The current ISO and source hashes are in `linux-hid-generic-integration-result.json`; the specific new host proof is `linux-hcd-hub-contract-result.json`. Guest receipts observe actual original-HCD counters during rendered input, and original hub debounce/reset counters for hub/nested-hub profiles. Current generic HID has a separate 10-profile actual endpoint matrix. Older ISO/kernel receipts are historical, not certification of this binary. Kernel/install/Recovery/four-block final gates remain pending; PS/2 class work remains deferred. **No binary deployment, no physical ASRock certification; stable 10.1.6 unchanged.**

V14 validation: ISO 225810432 bytes, SHA-256 `6f693038ff80c009b2b7f06bf9900c145512359aa32ca8c672845de0c14ca59a`; all 34 current BIOS/UEFI input profiles, 11 source-bound host contracts, maintenance/legacy suites and identity/three-path media-license checks pass. Twelve mouse profiles additionally observe original HCD lifecycle execution; four hub/nested-hub profiles observe original hub debounce/reset execution. No full-stack/kernel/install/Recovery release gate is inferred.

## Historical v15: original synchronous Linux USB control pipeline

The complete requested controller/core/hub/HID macroblock is **still incomplete**. This increment compiles complete unmodified message.c synchronous control/send/recv/get-descriptor functions into the original URB/HCD pipeline and wires native xHCI/EHCI/UHCI enumeration, configuration, hub requests and generic HID EP0 through it. Native physical controllers are not relabeled Linux HCDs. Boot LED URBs keep their direct original-HCD worker to avoid recursive synchronous calls.

The message facade serializes up to 1024 bytes, copies only actual valid IN bytes, rejects late callbacks and overrun, preserves STALL/errors, bounds timeout/records and protects detach/reentrant/cross-thread ownership. Waiting is polled, not a sleepable scheduler, and cannot preempt native callbacks. Generic LED output defers while another EP0 worker owns transport. ASan/UBSan O1/O2 tests include source-exact descriptor retries, send/recv short-read policy, error and allocation rollback, cancel, record exhaustion and 1024 leak-free sessions. Final v15 guest/contract counts must be read from current ISO-bound receipts, not inferred from v14. No deployment; PS/2 class drivers remain last.

V15 final input validation: **34/34 BIOS+UEFI profiles**, **12 source-bound host contracts**, maintenance/legacy suites and identity/three-path OpenCore/license media checks pass. **28** USB mouse/keyboard/generic guest receipts observe the original message-control pipeline. Two independent normal builds and dualboot packaging are byte-identical: 225814528 bytes, SHA-256 `c2293f5037fa427b88e5a99632c7f4eb7958333c1200c44e07e476fbd8ada8c3`. This is an input increment, not complete physical Linux controllers/core/hub/usbhid, full kernel/install/Recovery final gate or deployment. Source-first commit: `53a81e344374f18e80b81d5b97460a13570993de`.

## Historical v16: whole original Linux DMA pool and native TD consumers

The requested complete macroblock is **still incomplete**. Entire original Linux v6.12 `mm/dmapool.c` is now compiled, not a function-shaped substitute. Real implemented services provide coherent contiguous memory, IRQ-safe locks, native registration/readout of the original pools attribute, and allocated managed resources with LIFO release. There is no claim of Linux sysfs/VFS, a sleepable scheduler, IOMMU or noncoherent DMA. EHCI/UHCI control and interrupt transfer descriptors now use these pools and use DMA bus handles in hardware links rather than assuming CPU pointers equal DMA addresses. The physical schedulers, QHs/frame lists and the xHCI DMA model still remain native.

Host O1/O2 ASan/UBSan contracts exercise the whole allocator, allocation failures, boundary holes, multi-page expansion, low-mask rejection, zeroing, poison errors, busy teardown/duplicate-free rejection, registered original read attributes, both managed entry points and release callbacks, four-thread allocation races and 1024 complete lifetimes. The actual native coherent allocation arithmetic and EHCI/UHCI descriptor rearm/rollback are tested separately, including deliberately different CPU and DMA addresses. Failed controller slots are reserved rather than reused with uncertain active DMA; interrupt TD allocation precedes class attachment. Remaining EHCI hub descriptor/power and USB2 endpoint-recovery bypasses now also traverse original message/URB/HCD control functions.

Final guest counts are valid only for the recorded v16 ISO hash. No complete physical Linux HCD/root-hub/core/hub/usbhid claim or deployment; PS/2 class drivers stay last.

V16 final input validation: **34/34 BIOS+UEFI guests**, **13 primary source-bound host contracts**, additional current EHCI qTD/Intel routing contracts, maintenance/legacy suites and identity/three-path OpenCore/license checks pass. **16** EHCI/UHCI/companion mouse, keyboard and tablet profiles observe actual original DMA pool allocation with live coherent TD memory and no allocator failures/rejections. The original message path is observed in **28** USB profiles. Two independent complete builds and dualboot packaging are byte-identical: 225822720 bytes, SHA-256 `5d4b5084b43095a7519c5cd93e97213c4d56cf224c3b6258e34f7b9af990bd39`. The source-first commit is `5fec8e546b63ffeb204780393f5669ac589d7292`. Requested full block remains FALSE, final kernel/install/Recovery release gate not inferred, no deploy, stable 10.1.6 unchanged.


## v19 — original xHCI controller/input validation (unpublished)

The original Linux v6.12 xHCI core, memory, ring, root-hub, PCI, extended-capability and debug translation units now replace the former native controller. The separate native UsbInput enumeration/class boundary remains: this milestone is not the complete Linux USB core/bus/hub/usbhid stack and is not release approval. PS/2 is excluded from the requested remaining acceptance scope.

The initial mouse regression was traced with actual QEMU hardware TRBs. The shared USB 3.0 HCD was missing the initial `hc_driver.flags & HCD_MASK` protocol assignment; its root-hub scan incorrectly reset already-enumerated USB 2.0 devices. Endpoint submission had actually succeeded; the early hypothesis of a pre-submit endpoint failure was disproved by checked return diagnostics and hardware traces. The protocol initialization is fixed. Host tests additionally caught and fixed interrupt endpoint re-add after stop, stale IRQ registration on controller removal, endpoint configuration rollback, and parent/TT reference ownership. Intel shared-port routing now precedes enumeration.

**30/30 current UEFI+BIOS USB guests** pass, including direct/hub/nested mouse, original EHCI/UHCI/companion regressions, generic relative/absolute input, Italian keyboard and two successful LED controls. The two-mouse drag tests now use a USB-only keyboard with i8042 disabled; no PS/2 acceptance dependency. **14 actual ISO profiles observe original xHCI submission/giveback counters increasing with rendered input**, without controller errors. Original xHCI host tests run at O1/O2 with ASan/UBSan (only original list-sentinel alignment instrumentation excluded), nonidentity DMA, real command/transfer/event-ring consumption, full/short/STALL/clear-halt, cancellation, timeout, 600 callback requeues with ring rollover, nested route/TT contexts, parent lifetime, 96 controller allocation budgets and 32 endpoint/configure/DMA failure budgets with successful retry and no leaks. The host model also verifies SuperSpeed EP0=512 and companion burst/ESIT contexts; that is not a physical or QEMU SuperSpeed HID test.

15 source-bound host suites / 16 receipts pass. The complete maintenance suite also passes after replacing obsolete native-xHCI fixture invocations with the original-controller model and actual shared enumeration tests. The matched old native driver/fixtures are losslessly retained with hashes under `tinymacOS/tests/historical/native-xhci-pre-linux.tar.xz`; they are not counted as current transport coverage.

Two full independent normal builds and dualboot packaging are byte-identical. Current ISO: **226019328 bytes**, SHA-256 **9a9bb1611116787246ba9cae1f1545dafe5110d283fe4d8693199f4c7a6d1601**. KernelCache: **522016880ffd1bc5676a7f8e9fce3c16cd0259321ce6ca88b5aaf9f9cfe4b0f6**. Exact records: `v19-validation.json`, `v19-reproducible-build.json`, `STATUS.json`, `staging-source-hashes.json`. Actual ISO identity/About 10.1.7/30B8 and all three OpenCore media/license paths pass. The source ZIP tool now includes `compat/`, complete pinned Linux vendor sources, notices and the pinned firmware build dependencies; all archive entries were byte-compared to working source.

No full USB-core/hub/usbhid, installation, final kernel, separate Recovery runtime or physical ASRock acceptance is inferred. No upload, push or release was performed; public main stays v17 and stable 10.1.6 is unchanged. The original USB core/driver/generic/port/endpoint and usbhid source/header inputs for the next integration were downloaded to `/var/tmp/tmc-usbcore-port/`; they are not compiled or claimed active. The accidental unsupported `ehci-hub` launcher invocation is a harness-profile error, excluded from the supported 30-profile matrix, not an EHCI product test.

## Canonical USB ABI implementation and original bus binding (development)

After the v19 controller matrix, implemented `compat/linux/usbcore_services.c`
and `usbcore-include/` instead of aliasing the incompatible older USB/config/HID
structures. Exact upstream `linux/usb.h` and `linux/usb/hcd.h` now share one
implemented platform device model. Added original core usb/driver/generic/
quirks/endpoint source units and local core headers. Twenty-six additional
upstream inputs are pinned in the separate `usbcore-manifest.json`; the
production provenance manifest is byte-identical to the frozen v19 copy.

`TMC_USBCORE_RECEIPT=... bash tinymacOS/tests/test-linux-usbcore.sh` PASS at O1
and O2 with heap/stack/leak ASan plus UBSan including alignment. The receipt
`usbcore-canonical-host-result.json` binds 49 actual dependency/source files.
Nine entire original units compile on the same canonical ABI. Two focused
executables link/call the relevant functions; this is NOT all-file execution.

Implemented/tested: bus/device/driver registration; parent and release refs;
device iteration safe against child deletion; original matching, configuration
parser and allocation-failure unwind, alternate/interface lookup, hub ownership
and URB references; managed resource rollback and unbind release; internal
attribute/unnamed metadata groups; bounded aligned allocation/reallocation;
work/timer cancel/rearm, callback-owned frees, completion deadlines and UTF-16.
BH givebacks have independent queues and can progress while a sleeping worker
waits; sleeping workers do not recursively dispatch one another, and workers
are not entered while a caller holds a mutex. Freestanding -m64/-mno-red-zone
service compile with -Wall/-Wextra/-Werror PASS.

The second executable actually calls original USB bus registration/probe/
unbind and dynamic-ID add/show/remove code. Per optimization: 16 probes,
13 disconnects, 30 release callbacks, 15 managed releases; authorization and
failed-probe rollback plus fourteen registration allocation-failure positions
PASS. It has NO USB endpoints or I/O: test-only HCD/notification dependencies
abort on every call, rather than return a fake success. The first executable
uses an explicit descriptor transport fixture, not a guest/hardware device.
ASan global registration alone is disabled to allow dead-section elimination
of unused upstream tables. Named/dynamic-visibility metadata groups fail
explicitly; this registry is not a sysfs filesystem. The service layer is
serialized CPU0, not an SMP/preemptive Linux scheduler.

No kernel build/link change: `build.sh` does not contain the canonical units,
and they cannot be linked beside the old conflicting private ABIs. Original
HCD/root-hub transport plus the full hub lifecycle and HID/usbhid ABI still need
to be wired to this model before actual USB enumeration/runtime can be gated.
Do not count the NO-I/O binding test as USB input, hub enumeration, usbhid,
installation, Recovery, physical-ASRock or release evidence. No push/release.

Fixed `refresh-status.py` so it no longer rewrites the frozen v19 source
manifest with a later working tree. v19 remains source-manifest SHA
15e2df7888e482f9b89605655b8ceb7ca8eedc1b6964062f7fea61fb7910d1dd,
30/30 guest profiles and 16/16 source-bound controller receipts PASS. The
separate canonical-development evidence is explicitly kernel_linked=false.
Only existing source-tree documentation changed; all added canonical code is
unlinked. Matching-source ZIP for v19 remains a frozen v19 archive, not an
archive of this newer working tree. Final full-stack gates are not implied.

## Newer milestone: canonical USB core/HCD/hub actually linked and running

This section supersedes the preceding **unlinked-only** development status.
It does NOT supersede the frozen v19 release/source receipts or approve a release.

`TMC_USB_CANONICAL=1` now selects an experimental kernel path:
- complete original canonical USB core/HCD/hub/common/buffer/port translation
  units, genuinely used for root registration, child enumeration/configuration,
  bus/class binding, URB submission/giveback, and hot unplug;
- original xHCI/EHCI runtimes reached only via typed opaque handles/byte buffers,
  never aliased canonical/private structs; original native class facade inactive;
- complete original usbkbd/usbmouse and a canonical-model OS input adapter,
  **not** the complete HID-core/usbhid transport. No generic/tablet claim.

The same new dualboot ISO passed **6/6** development guest cases: BIOS and UEFI,
xHCI two mice+keyboard, EHCI mouse+keyboard, xHCI external hub+mouse+keyboard.
Each case: actual media kernel matches build, USB-only rendered enrollment and
Terminal, pixel-level cursor assertion, armed-URB unplug, same-port reattach,
new actual mouse input, original controller counters advancing without errors,
canonical error counter zero, historical native class allocation/probe counters
remain zero. All use i8042=off. This is QEMU evidence, not physical ASRock proof.

Artifacts under `/var/tmp/tmc-10.1.7/linux-usbcore-live`:
- dualboot ISO SHA256 `101efdd502151905b7976655c341532c805626dd47c49bec0ec8d5ba90a6cff0`;
- native ISO SHA256 `d41b6a08e2e0f9a15864048edda9b38ce5db776d2e97a8b7199476202d093f50`.
Exact kernel hashes, frozen source manifest, undefined-symbol allowlist and all
six receipt hashes: `usbcore-kernel-integration-result.json`.
Revalidate: `python3 workspace/release-10.1.7-30B8/refresh-usbcore-progress.py`.

Full original core/HCD/hub explicit-controller-MODEL runtime O1/O2 ASan/leaks/
UBSan also PASS: enumeration, original generic configuration/binding, 3 real
interrupt URBs, hot unplug, root teardown and allocation-baseline equality.
Existing canonical binding/NO-I/O contracts and original xHCI sanitized O1/O2
contracts PASS. Model and guest receipts are intentionally distinct.

Important fixes: driver release before checking hub child emptiness; private
xHCI standard DeviceRequest uses the correct high-byte encoding; genuine BOS
bytes/length forwarded; actual xHCI-assigned device address exposed; EHCI uses
canonical SET_ADDRESS instead of xHCI-style address_device hook; only the USB2
xHCI root advertises integrated TT; kernel-style -fno-strict-aliasing mandatory;
orphan undefined symbols removed after ld-r GC, then a strict symbol allowlist
prevents accidental linkage to an incompatible private Linux ABI.

Still experimental: sleeping work drains in serialized kernel input/IRQ polling,
only control/interrupt-IN transport is accepted, entropy ingestion has no kernel
pool API, init/endpoint failure rollback and integrated reset need more coverage.
Full canonical HID-core/usbhid and final installation/Recovery/UI/license gates
are NOT certified. Default build remains the earlier private path; no publication.

Unlinked HID follow-on scratch at `/var/tmp/tmc-hidcanonical`: original whole
usbhid/hid-core.c syntax now compiles against exact original hid.h and canonical
USB ABI; full HID-core/input service work remains in scratch error logs. No
private HID/input structs were aliased into the new USB device model.

## Documentation and development commit

Updated root/system READMEs, architecture/validation/quickstart and the Linux
boundary documentation to distinguish the default private ABI from the opt-in
canonical xHCI/EHCI/core/hub/Boot experiment. Added `docs/HARDWARE-TEST.md`:
physical live attempt only on a noncritical machine, exact tested optical ISO,
no USB-DD/Rufus/Ventoy guarantee, disconnect valuable disks, no installation,
USB Boot peripherals, Recovery separate, no physical-board validation claim.

The six tested media/kernel receipts and their source snapshot were NOT
rewritten to describe documentation added later. The progress verifier reports
`source_snapshot_current=false`, `runtime_sources_current=true` and explicitly
lists the documentation delta, while still checking media and receipt hashes.
No runtime source changed during this documentation update. Development commit
only: no push, tag, release, installer approval or hardware PASS follows.
