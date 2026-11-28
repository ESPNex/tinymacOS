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
- [x] Input: tutti i 473 file della baseline USB/input ancora byte-identici; guard pre-authentication invariato.

## Ancora da completare — M8 NON concluso

1. Storage USB canonico: estensioni bulk solo negli adapter originali EHCI/xHCI e bridge, classe BOT/SCSI, block layer, init/hotplug, flush/errori/timeout/stall/reset e geometrie, test input simultaneo. Non cambiare handler input/HID, non ripristinare driver ritirati. Gli attuali test mount su NVMe NON provano USB storage.
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

Rapporto corrente: `MISSION-CONTROL-OTTICA.md`. Nessuna attività prosegue automaticamente dopo la fine di un checkpoint. Nessun tag/release/version bump/video M8 finale; M8 resta aperto.
