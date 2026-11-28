# M8 — ordine aggiornato dalla richiesta dell’utente

## Priorità eseguite prima del resto

- [x] Revert dell’ottimizzazione palette-only nel loop desktop: percorso originale accento + sfondo + transizione. Il mapping kernel tema→sfondo non era stato rimosso ed è rimasto invariato.
- [x] Libreria app: niente velo scuro sul desktop, niente ombra propria catturata dal materiale; nuova variante frosted senza spesso bordo rifrangente/aloni interni.
- [x] Calcolatrice: eliminata soltanto la seconda lente full-bleed del tastierino, causa del bordo interno in basso a destra. Cronologia e cornice esterna non modificate.
- [x] Mission Control: oscuramento finale anche sulla barra dei menu, stessa fase dello sfondo e una sola applicazione; ripristino all’uscita verificato.
- [x] Build e prove reali nella VM, screenshot caricati e hash/size verificati su Drive: https://drive.google.com/drive/folders/13rcn8izgQOjVSbgxALro1ftP9oqwmj3R

## Prosecuzione già eseguita dopo gli screenshot

- [x] Backend mount/unmount per TMCFS2/FAT32/exFAT/NTFS: stato effettivo, flush prima di chiudere, file aperti→rifiuto, volume di avvio protetto, rollback su errore, nessun force-unmount, stato offline conservato nei rescan.
- [x] Comandi nativi `mount`, `umount`, `diskutil mount`, `diskutil unmount`.
- [x] Clic destro Monta/Smonta nel Finder; in Utility Disco pulsante nella toolbar accanto a S.O.S., con query di stato al disegno e al clic e ricontrollo nel kernel.
- [x] Host normali + ASan/UBSan sui quattro filesystem reali; guest retail su partizione FAT32/NVMe disposable: Terminale, menu Finder e toolbar Utility Disco, assenza azioni mount nel menu Utility, rimontaggio/readback, SHA-256 dell’intero disco invariata.
- [x] Al checkpoint grafico/mount precedente, tutti i 473 file baseline erano byte-identici. Dopo l’estensione bulk: 466 identici e 7 integrazioni storage esplicite, handler input/HID invariati; ricevuta `usb8-input-preservation.json`. Guard pre-authentication invariato.

## Ancora da completare — M8 NON concluso

1. Storage USB canonico **implementato e verificato parzialmente**: bulk negli adapter originali EHCI/xHCI/bridge, BOT/SCSI, block layer e bootstrap, flush/errori/timeout/reset, geometrie e hotplug. Due guest QEMU USB FAT32 xHCI/EHCI PASS: write/readback, smonta/rimonta, stesso-port detach/reattach su nuovo slot, tastiera/cursore e verifica mtools. Host O1/O2 ASan/UBSan e regressioni quattro filesystem PASS. **Ancora da fare prima di chiudere USB**: guest USB TMCFS2/exFAT/NTFS, 4Kn, Recovery/installato, stress input durante I/O sostenuto e guasti/disconnessione durante write. Nessun handler input/HID modificato; audit bulk esplicito senza refresh degli hash M0 originali. Vedere `USB8-STORAGE.md` e limiti single-LUN/BOT/8 slot non riutilizzati. Nessuna certificazione hardware USB3.1.
2. `hdiutil` nativo RAW/IMG/ISO/DMG: attach/detach/backend effettivo, parsing con bounds, scope varianti DMG esplicito. BaseSystem è già UDIF vero e viene verificato con decoder indipendente, non è una funzionalità hdiutil completata.
3. Accettazione integrata completa, Recovery/live/installato e regressioni; rimisurare le prestazioni sul prodotto finale, senza riutilizzare i vecchi numeri come benchmark della build corrente.
4. Solo dopo i gate: versione 10.3, commit/push, backup indipendente e release GitHub con ISO/checksum; video nuovo su Drive, soltanto M8.

## Ultima richiesta eseguita

- [x] Commit di sicurezza `80537612eeea68bda1efe8cc4b643d0cbc9c3937` pushato e SHA remoto verificato, prima di altre modifiche.
- [x] Backup Git storico + incremento M8 + metadati boundary/sorgenti originali su Drive; size/MD5 confrontati, ripristino indipendente e fsck PASS. Bundle locali rimossi dopo verifica; repository di lavoro conservato. La storia rimane shallow al confine documentato, non si inventano antenati assenti.
- [x] Mission Control: riferimento visivo macOS Sonoma esaminato; Dock visibile e luminoso mantenuto intenzionalmente, nessun velo globale sopra le miniature. Nuova prova pixel: nuclei opachi Dock invariati al 100%, barra oscurata al 100% del blend atteso.
- [x] Tre screenshot nuovi su Drive, stessa cartella delle priorità; size e MD5 verificati.

## Correzione successiva da screenshot utente

- [x] Eliminato il replay rettangolare di vecchio sfondo attorno al Dock, causa della fascia chiara sul Calendario.
- [x] Mission Control: vetro e sheet interni campionano la scena alla destinazione attuale, anche durante entrata/uscita, non le vecchie posizioni. Rimossa la cache AN_MC congelata; contenuto nativo conservato.
- [x] Normale + ASan/UBSan sul compositore/materiali reali; guest con tre finestre: spostare il Finder nativo non altera più le ottiche delle altre miniature (100% invarianti); regione Calendario/Dock stabile (100%). Nuovo PNG originale su Drive verificato.

Rapporto corrente: `USB8-STORAGE.md`; ottica pubblicata precedente: `MISSION-CONTROL-OTTICA.md`. Nessuna attività prosegue automaticamente dopo la fine di un checkpoint. Nessun tag/release/version bump/video M8 finale; M8 resta aperto.


## Nuove richieste utente — pianificate, implementazione non avviata

- [ ] Drag & drop: miniatura/icona effettiva di cartella o file al posto del quadrato generico.
- [ ] Dispositivo USB e volumi nella sidebar Finder, sincronizzati con hotplug ed espulsione.
- [ ] Menu contestuale Finder **Espelli**: vera espulsione device-wide, distinta da Smonta, con controllo volumi occupati/protetti e flush; errori espliciti, nessun cambiamento HID/input.
- [ ] Prima di iniziare queste aggiunte, rispondere sullo stato immagini disco: attach/mount RAW/IMG/ISO/DMG generico e navigazione contenuti **non disponibili attualmente**. Il vero BaseSystem UDIF per il boot non è un hdiutil funzionante.

Stato di questa richiesta: aggiornato soltanto il piano; nessuna nuova modifica al codice o build. In attesa del seguito dell’utente dopo il chiarimento preventivo.


## Priorità aggiornata — sospendere nuovi test immagini

Prima dei nuovi test: backend file-backed a 64 bit per RAW/IMG/ISO9660/DMG >128 MB, rimozione del cap provvisorio 64 MiB e supporto DMG compressi. Chiarire separatamente codec/contenitori, filesystem Apple e cifratura/password. Implementazione attuale non ancora conforme; non dichiarare supporto universale o M8 concluso. Vedere ultima sezione di `PIANO.md`.


## Perimetro immagini confermato — scelta apple_fs

L’utente ha scelto anche **HFS, HFS+ e APFS in sola lettura**, oltre ai contenitori RAW/IMG/ISO9660/DMG compressi e >128 MB. Cifratura/password escluse. Nuovi test sospesi fino all’implementazione dell’estensione. Il precedente backend snapshot da 64 MiB non è accettabile: sostituzione con lettura posizionale, cache limitata, pinning della sorgente e protezione dalle mutazioni. Distinguere riconoscimento della firma, decodifica del contenitore e reale navigazione dei file. Nessuna dichiarazione di supporto universale, release o pubblicazione anticipata.


## Checkpoint successivo — estensione scritta, non ancora accettata

Vedere `IMMAGINI-ESTENSIONE.md` per stato corrente e limiti effettivi. Il vecchio snapshot da 64 MiB è stato sostituito nel codice con file backing nativo/bootfs a 64 bit, cache limitate e pin sorgente; aggiunti multi-blkx, ADC/zlib/bzip2/LZFSE/XZ-LZMA2, HFS/HFS+/HFSX, APFS v2 RO iniziale e Apple Partition Map RO. Compilate soltanto 12 unità e dipendenze decode-only (log esterno `compile-only.log`), senza diagnostiche: **nessun nuovo test, sanitizer, guest, link/build ISO, commit o push**. Non supporto universale: restano sorgenti esterne, APFS sealed/compressione file/Unicode completo, varianti UDIF non XML/segmenti e limiti decoder da completare/valutare prima dei test. I PASS storici non verificano questo codice. Main e ISO pubblicati invariati. Input/HID non modificato.


## Checkpoint corrente — rimozione limiti una per una

Fa fede la checklist aggiornata `IMMAGINI-ESTENSIONE.md`, non le descrizioni storiche sopra. Codice: registri immagini/block estensibili, mappe DMG radix senza tetti run/XML, indirizzamento immagini u64 separato dai dischi scrivibili, sorgenti esterne/snapshot/annidate con pin/freeze e stack dedicati, allocazioni grandi da pagine fisiche, crescita LZFSE/dizionario XZ non cappati provvisoriamente, plist binario/resource fork UDIF, resource fork HFS e bootstrap overflow esteso, nomi reali APFS con alias stabili, FAT12/16 e runlist NTFS senza 64-run cap. Compilazione/link incrementale riusciti: **non accettazione**. Restano checksum/segmenti/NDIF, funzioni Apple avanzate/compressione/Unicode/namespace, attribute list/compressione NTFS, reattività, thumbnail contenuto e matrice finale. Nessun nuovo test/guest/commit/push o ISO. Non avviare il test finale dichiarando risolte le voci ancora aperte.
