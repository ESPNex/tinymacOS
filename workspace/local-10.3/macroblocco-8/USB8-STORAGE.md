# M8 — storage USB canonico: checkpoint implementato e verificato

## Stato corretto

Trasporto bulk e classe BOT/SCSI **ora implementati**, collegati al block layer comune. Build integrata e due guest reali QEMU PASS (xHCI ed EHCI). **Non equivale al completamento di M8**, né a certificazione di hardware USB 3.1/SuperSpeedPlus. Nessun cambio versione, tag, release o video finale.

ISO testata, esterna al workspace:
`/var/tmp/tmc-block8-candidate/tinymacOS-10.2.6-x86_64.iso`

SHA-256: `7bf58df557d3f322e4c2a1d4733468a24e5cc20f286d8e8c1bbf2ac9f510d841`.

## Implementazione

- Nuove API bulk additive nei runtime **originali** Linux xHCI/EHCI. URB, mapping DMA, ring/schedule e teardown sono quelli del percorso originale: nessun HCD alternativo, nessuna riscrittura nativa di ring/qTD.
- Bridge canonico: code bulk separate dalle interrupt, bounce IN/OUT, limite 64 KiB per URB, actual_length anche sugli errori, cancellazione, clear-halt/reset del toggle. Rifiuto esplicito di SG, streams e ZERO_PACKET non supportati.
- Classe `usbcore_storage.c`: BOT, CBW/CSW con signature/tag/residue/status controllati, timeout, STALL e reset recovery nell'ordine class reset → clear IN → clear OUT. Nessun replay automatico di scritture fallite.
- SCSI INQUIRY, TEST UNIT READY con tentativi limitati/REQUEST SENSE, READ CAPACITY(10/16), READ/WRITE(10/16), SYNCHRONIZE CACHE. Capacità e bounds a 64 bit; geometrie logiche 512/4096 byte. ABI comune a coordinate 512 byte; su 4Kn accessi parziali con read/modify/write serializzato e preservazione dei byte circostanti.
- Dispatch comune `sata_*`: AHCI, NVMe, USB, con geometria effettiva. Stack USB avviato una volta **prima** della ricerca dei filesystem; implementazione `input_init` e handler HID invariati.
- Probe BOT differito fuori dai sleeping worker hub, evitando attese ricorsive nel dispatcher. Hotplug marca offline immediatamente; refresh lazy dell'inventario invalida gli FD esterni tramite generazione.
- Indici USB mai rinumerati o riutilizzati durante il boot: dopo un distacco il vecchio slot resta una tombstone a capacità zero; un nuovo dispositivo non può ricevere scritture da un vecchio riferimento numerico.

## Prove eseguite

| Prova | Esito e confine |
|---|---|
| `tests/test-usb8-bulk.sh`, O1/O2, ASan/UBSan | PASS: HCD originali su modelli DMA nonidentity xHCI/EHCI, 64 KiB IN con guardie, 32 KiB OUT esatti, short, STALL/reset, cancel/non-completion, requeue e URB interrupt simultanee. Modello, non hardware. |
| SuperSpeed xHCI sul modello | PASS: MPS 1024, burst 15 nel contesto hardware, trasferimento 64 KiB. Non certificazione fisica 3.1. |
| `tests/test-usb8-storage.sh`, O1/O2, ASan/UBSan | PASS sul codice BOT/SCSI reale con modello wire: capacity10/16, READ16 oltre 32 bit, split 128 KiB, letture/scritture esatte, flush, 4Kn unaligned/RMW, overflow, CBW corto, CSW corrotti/tag/residue/phase, timeout/STALL/reset, sense senza replay, fallimento reset→offline, ref lifetime al distacco. |
| Build completa live + Recovery | PASS, ISO sopra. Il semplice build di Recovery non prova ancora un disco USB in Recovery. |
| Guest xHCI, periferica QEMU `usb-storage` | PASS: disco disponibile alla prima discovery, FAT32 read/write/copy, smonta/flush/rimonta, readback, detach/reattach stessa porta, nuovo `disk1s1` anziché riuso `disk0s1`, tastiera USB e cursore CUR originali funzionanti, lettura indipendente mtools del file scritto. |
| Guest EHCI, tastiera/mouse USB2 sullo stesso controller | PASS: stessa sequenza storage e hotplug, input e verifica mtools. |
| `tests/test-foreignfs.sh`, normale + ASan/UBSan | PASS: regressione dei filesystem reali TMCFS2/FAT32/exFAT/NTFS e namespace/mount/export. Questi host test NON sostituiscono il guest USB su tutti e quattro i filesystem. |
| M0 e pin sorgenti Linux | PASS con eccezione M8 esplicita descritta sotto; nessuna sorgente upstream modificata. |

I guest usano comandi Terminale tramite tastiera USB reale emulata e confronto pixel del cursore da `Normal.cur`, non il vecchio sprite incorporato. I JSON `usb8-{xhci,ehci}-guest.json` registrano l'ISO effettiva e gli esiti. PNG originali in `evidence/{xhci,ehci}-usb8-storage-hotplug-input.png`. Log del build e delle suite in `usb8-qa/`.

## Protezione input: audit esplicito, non refresh nascosto

`input-baseline.json` resta identico: **466/473 file byte-identici**; le sette differenze sono soltanto quattro file di integrazione/runtime, due header con nuove dichiarazioni bulk e `build-usbcore.sh`. Nessun handler tastiera/mouse/HID modificato, nessun driver ritirato ripristinato.

Gli hash storici in `tests/fixtures/m0-contract.json` **non sono stati cambiati**. I cinque file M0 di integrazione che devono estendersi per il bulk sono registrati separatamente in `usb8-bulk-extension.json`: hash originali conservati, hash dell'estensione revisionata, hash indipendenti delle funzioni interrupt/control/LED rimaste identiche. `usb8-extension-contract.py` rifiuta nuove modifiche e verifica il rifiuto con un mutation test. M0 dichiara correttamente **563 file byte-locked + 5 estensioni bulk esplicite**, non «568 tutti identici». Ricevuta `usb8-input-preservation.json`.

## Limiti attuali e prossimo lavoro

1. Completare il guest USB con **TMCFS2, exFAT e NTFS** oltre al FAT32 già verificato; stessa matrice per 4Kn, Recovery e installato. Host RMW e regressioni quattro filesystem passano, ma non vanno descritti come tale matrice guest già eseguita.
2. Aggiungere stress prolungato bulk con eventi input durante I/O sostenuto e guasti/disconnessione durante scrittura. Il modello verifica URB interrupt simultanee, i guest verificano input durante l'uso storage e dopo hotplug; non è ancora un benchmark di latenza sotto carico.
3. Sono supportati BOT/SCSI transparent e solo dispositivi single-LUN. Multi-LUN rifiutati, UAS/streams non supportati. Geometrie diverse da 512/4096 rifiutate. Otto slot complessivi nel block ABI: le tombstone non si riusano; dopo esaurimento è necessario riavviare. Non dichiarare hotplug illimitato.
4. Poi `hdiutil` RAW/IMG/ISO/DMG effettivo, accettazione integrata Recovery/live/installato e misure prestazioni finali. Solo dopo: versione 10.3, release GitHub, backup indipendente e video M8.

Le prime prove del nuovo harness sono state corrette senza toccare il prodotto: percorso OVMF dell'ambiente, confronto con il CUR M7 invece del vecchio sprite, ricreazione del block backend QEMU dopo device_del (QEMU elimina quello legacy automaticamente). I risultati PASS citati sono le ripetizioni finali, non i tentativi falliti.
