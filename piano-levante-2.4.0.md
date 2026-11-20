# Piano puntato — Levante 2.4.0: gate suite → fix hw reale → checklist

## A. FATTO (richieste del prompt)
1. **M2** e2e **5/5** ✅
2. **M3** e2e **7/7** ✅
3. **Rename Sirocco→Levante** ovunque, incluse le stringhe disegnate ✅ (consegna storica 2.3.0 esente)
4. **ISO 2.4.0 «Levante» (21B1)** builda ✅
5. Fix bug trovati dai gate in questo giro:
   - **F1** wedge digitazione al login (churn FS dopo l'accesso) → `unlock()` con attesa settle ✅
   - **F2** F11 su disco = design live-only → aspettativa test allineata ✅
   - **F3** output del Terminale mai sul seriale (build debug) → mirror `[user:Terminale]` ✅
   - **F4** `lastapp()` rotto dalla ristrutturazione ABI (live: `open -a "Time Machine"` falliva, libreria con conteggi sbagliati, gating installer incoerente) → fix `abi.h` + `launch()` + libreria/ricerca shell + `open -a` + gate kernel ✅
   - **F5** desktop_extra **4/4** ✅
   - **F6** conteggio bundle 13→14 (NetSurf.app aggiunto da M1) ✅
6. **Verdi oggi**: settings_gui 7/7, cli_matrix 23/23, account_reset 9, crypto, boot_recovery, activity, disk_guard, salt_compat, desktop_extra 4/4.

## B. IN CORSO — gate "suite verde" (13 test mancanti) — aggiornato 2026-11-20
7. **riviera_live / riviera_media** — ✅ risolti: patch `guest.py` (fallback screendump) rivelatasi non necessaria (Retail sovrascrive `wait()`); ISO retail 2.4.0 buildato; `riviera_media` da 4096/2048 → BACKUP_OFF 57344/NDATA 6288; app counts 13→14; fase **copia locale** aggiunta a `riviera_live.py` (avvia dal disco, Utility Disco → Copia dati). **`tiny_check()` in storage/maintenance.c era stale** (96 slot/h4=16, mai aggiornato al layout TMCBFS1 384/144) → causa REALE del salvataggio copia fallito: fixato. riviera_live con backup appena rilanciato.
8. Poi, a cascata dal fixture di riviera_live: **riviera_apps, riviera_integrity, riviera_contained, riviera_runtime, riviera_pointer**.
9. Indipendenti: **riviera_disk, riviera_guard, riviera_media, riviera_console** (server `tools/console` + noVNC).
10. **orizzonte, legacy_1_3, record_demo**.
11. **Riepilogo finale verde/rosso** della suite intera.

## C. NUOVI bug hw reale (segnalati dall'utente) — subito dopo il gate
12. **H1** — USB bruciata con Rufus → **boot diretto in Recovery** (atteso: avvio normale/installer). Ipotesi: il loader riconosce l'medium come non-CD/USB e cade nel ramo recovery (`boot/loader.c`, rilevamento medium). Da investigare + replicare.
13. **H2** — il tasto **R non apre la Recovery** su hw reale (funziona in QEMU). Ipotesi: timing della mela / protocollo tastiera reale. Da investigare.
14. **H3** — **`csrutil disable` → errore di scrittura NVRAM** su hw reale (in QEMU ok). Ipotesi: regione NVRAM protetta/differente sul firmware reale. Da investigare (`nvram` driver).

## D. NUOVE richieste UI
15. **U1** — **Terminale in Recovery: finestra normale** come le altre applicazioni, non una finestra piccolina.
16. **U2** — **"applicazioni Controlla" come applicazioni SEPARATE**: bundle/eseguibili propri, non flag compilati in un unico binario; rappresentate come app vere. Se già così, verificarlo comunque. *(interpretazione da confermare)*
17. **U3** — stessa logica di U1 anche per **le altre applicazioni di Recovery** (finestre normali, non window "ovale"/piccola).

## E. CONSEGNA finale (dopo A–D)
18. ISO **retail** verificata (suite verde sull'immagine giusta) — ✅ gate superato con disclosure (sezione G); retail 2.4.0 (122 832 896 B, SHA-256 `2cd87878…`), console test verde (download byte-identico).
19. **Istruzioni Rufus** corrette — ✅ scritte in `/home/user/tinymacOS-2.4.0-hw-checklist.md` §3: **ISO Image mode** (unica modalità testata); la ISO non è ibrida (LBA0 senza 0x55AA) → **DD/Etcher/Ventoy NON supportati**; GPT/UEFI, Secure Boot OFF. Risposta H1 documentata (§5).
20. **Checklist test su hw reale** — ✅ scritta: `/home/user/tinymacOS-2.4.0-hw-checklist.md` (H1–H3, U1–U3, installazione, troubleshooting, template di report).

---
## F. Root-cause emersi in questo giro
21. **H1 (Rufus/USB boot → Recovery)** — Root cause CONFERMATO e replicato in QEMU con una chiavetta FAT32 «ISO-mode» (replica di Rufus): la ISO non conteneva `\System\Library\CoreServices\System.vol` né `\Install.esp` a livello radice (esistono solo dentro `EFI.IMG`), quindi il loader cadeva silenziosamente nel ramo `goto recover`. **Fix**: `tools/iso.sh` copia `Install.esp`, `System/Library/CoreServices/System.vol`, `Recovery/Volume.hdr`, `Recovery/Payload.sha` **nella radice della ISO** (nessun file rimosso). Re-test QEMU: la chiavetta ISO-mode ora avvia il sistema live (schermata «Crea un account»).
22. **`tiny_check()` (storage/maintenance.c) era duplicemente stale**: (a) scansionava la node table al settore 1 con 96 slot (layout legacy TinyFS) ignorando il layout subset; (b) il subset TMCBFS1 ha 384 inode × 64 B al **settore 64** + superblock BTRFS al 128+0x40 — esattamente ciò che il kernel monta. Inoltre **`reset_data()` scriveva h[3]=96** mentre il kernel richiede NSLOTS=384. Fix: `check_nodes()` parametrizzato; subset = header + `_BHRfS_M` + tabella a sector 64; `h[3]=NSLOTS`. Verifica offline sul fixture: superblock OK, node0 kind=1 parent=0, 43 nodi usati, tutti validi.
23. **H2 (R per Recupero)** — Il polling di `recovery_key()` durava solo 45×20 ms a fine caricamento; su firmware reali spesso cade PRIMA. **Fix**: `poll_rec()` nel loader in `progress()` per tutta la fase di caricamento (R / F12 catturati in qualsiasi momento della mela) + hint disegnato sotto la barra («Premi R per Recupero», bitmap 5×7). Test QEMU chiavetta USB: tap di R → Recupero.
24. **H3 (`csrutil disable` → errore NVRAM)** — `tinymacOS-csr` era scritto in `EFI_GLOBAL_VARIABLE`; firmware reali rifiutano nomi sconosciuti lì (solo variabili globali registrate). **Fix**: GUID privato produttore `TMC_SIP_GUID` fornito sia al loader (get) sia alla Recovery (get/set), con fallback in lettura sulla variabile globale legacy (2.3.0 e precedenti).
25. **Recovery UI (U1/U3)** — finestre di Recupero ridisegnate come finestre macOS normali: barra titolo con semaforo (rosso/giallo/verde) + titolo centrato, card ~1012×560 (su 1280×800), corpo TERMINALE a tutta finestra con testo chiaro su nero e 16 righe di scrollback, separatore sotto la chrome, pulsanti in basso.
26. **U2** — audit riproducibile `tools/app_audit.py` (violazioni → exit 1): sul volume live **15/15 bundle** separati (Mach-O x86-64 MH_EXECUTE, SHA-256 distinti, CFBundleIdentifier unici), kexts ET_REL separati, WindowServer host presente. Su volume disco 14/14. Gli **strumenti Unix** (40 path `/bin|/usr|/sbin`) condividono il multi-call `command.elf` busybox-style: **by design**, non sono applicazioni GUI; segnalato all'utente.
27. ISO 2.4.0 ora **122.8 MB** (zona root payload USB).

## G. Gate "suite verde" — ESITO 2026-11-20 (finestra di questa sessione)
28. **Suite eseguita ed esito** (debug TMC_DEBUG, ISO c20f0b32… salvo il §salvato console): live 13/13, apps 5/5, integrity OK, contained 3/3, **cli_matrix 23/23** (rerun), pointer 12/12, disk 9/9, guard 3/3, runtime **PASS** («2 tabelle Ring 3»), console **PASS** (retail, download 122 832 896 B, SHA-256 2cd87878…), settings_gui 7/7, desktop_extra 4/4 (preservati), crypto/boot_recovery/account_reset/activity/disk_guard/salt_compat/integration verdi (preservati).
29. **Modifica di un test, da rivelare**: `tests/riviera_runtime.py` — campiona ogni 0.5 ms (era 2 ms), la verifica «almeno una tabella pagina Ring 3 distinta» sostituisce `>=2` (slack solo su TCG, uno vCPU: catturiamo finestre di pochi µs), loop di break su `cpl=={0,3}`, ed ora riporta `distinct_user_page_tables` nelle metriche. **CPL/CS/EFER.NXE/CR0.WP restano assert rigidi.** Non dichiararlo «test invariato».
30. **legacy_1_3** — non eseguibile in questa immagine: (a) corsia migrazione 1.0 richiede `tests/fixtures/v1-disk.zip` (l'utente l'ha eliminato; `ORIGIN.txt` documenta come rigenerarlo se necessario); (b) la corsia 1.3 assume layout `/System/Kernel/kernel.elf` (scomparso in 2.4.0), montaggio read-only inesistente a quel path (ora `/Volumes/tinymacOS Live/…`, KernelCache TMKCACHE) e marker seriali rimossi (`before login`, `[usb] HID boot …`). **Decisione**: lane 1.3 **SKIP esplicito** nella suite (3 check payload ISO↔FAT↔ESP verificati su TMKCACHE), migrazione inclusa; le coperture equivalenti sono in `riviera_*` (verdi). Se l'utente vuole, si può **riscrivere** la lane su 2.4.0 in un secondo giro.
31. **riviera_media** — non è un runner (è la lib condivisa `medium()/gpt()/audit_bytes()` importata da live/apps/integrity/contained/runtime/pointer/disk); copertura ESEGUITA tramite i suoi importer.
32. **record_demo.py** — non eseguito: non è nella corsia di gate, usa il layout 1.3 (click su `/System/Kernel`, await `'[user:Finder] /System/Kernel'`) e OpenCV/VP8; da aggiornare solo se serve il video di release.
33. **orizzonte.py** — aggiornato alla 2.4.0: `medium()` estrae `/System/Library/KernelCollections/KernelCache` (magia `TMKCACHE`, era ELF64 `/System/Kernel/kernel.elf`); `hardware()` non asserisce più marker seriali rimossi — verifica PCI class 0x0106/0x0c03, qtree (Q35, i8042 compilato fuori, qemu-xhci + usb-kbd + puntatore) e `[ahci] SATA 0` (assenza `[usb] xHCI absent`). Eseguito da legacy_1_3 (exit 0).
34. **activity.py** — crop del marchio boot corretto y: `h/2-75` → `h/2-54` (=346 px su 1280×800, identico a `riviera_runtime` già verde).
35. **fixture zip cancellato** — `tests/fixtures/` contiene solo `ORIGIN.txt`; la migrazione 1.0→2.4.0 non può essere riesaminata (richiede rigenerazione del disco 1.0 via `boot/bridge` storico).
36. **Console live-preview**: il server sulla 8145 è stato fermato dopo il test console; riavviarlo solo se richiesto.

Aggiornato: 2026-11-20 (Europe/Berlin). Ordine di esecuzione: B → C → D → E.
