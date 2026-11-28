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
- [x] Clic destro Monta/Smonta secondo lo stato nelle sidebar di Finder e Utility Disco, con ricontrollo nel kernel.
- [x] Host normali + ASan/UBSan sui quattro filesystem reali; guest retail su partizione FAT32/NVMe disposable: Terminale e i due menu, rimontaggio/readback, SHA-256 dell’intero disco invariata.
- [x] Input: tutti i 473 file della baseline USB/input ancora byte-identici; guard pre-authentication invariato.

## Ancora da completare — M8 NON concluso

1. Storage USB canonico: estensioni bulk solo negli adapter originali EHCI/xHCI e bridge, classe BOT/SCSI, block layer, init/hotplug, flush/errori/timeout/stall/reset e geometrie, test input simultaneo. Non cambiare handler input/HID, non ripristinare driver ritirati. Gli attuali test mount su NVMe NON provano USB storage.
2. `hdiutil` nativo RAW/IMG/ISO/DMG: attach/detach/backend effettivo, parsing con bounds, scope varianti DMG esplicito. BaseSystem è già UDIF vero e viene verificato con decoder indipendente, non è una funzionalità hdiutil completata.
3. Accettazione integrata completa, Recovery/live/installato e regressioni; rimisurare le prestazioni sul prodotto finale, senza riutilizzare i vecchi numeri come benchmark della build corrente.
4. Solo dopo i gate: versione 10.3, commit/push, backup indipendente e release GitHub con ISO/checksum; video nuovo su Drive, soltanto M8.

Nessuna attività prosegue automaticamente dopo la fine di un checkpoint. Nessun commit/tag/release nuovo pubblicato finora. I report precedenti documentano stati storici, non il risultato finale.
