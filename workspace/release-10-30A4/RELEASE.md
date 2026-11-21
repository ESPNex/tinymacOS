# tinymacOS 10 · Tramontana — manutenzione 30A4

Versione **10**, build tecnica **30A4**. ISO: `tinymacOS-10-x86_64.iso`. v10 resta normale/Latest; le quattro release precedenti rimangono Pre-release. Download privato con account GitHub autorizzato.

## Correzioni richieste

- **tinymacOS Recovery** è ora il testo esatto sia nella barra superiore sia nel titolo della schermata principale. Rimossa la dicitura «Tastiera ITA»; la mappatura della tastiera resta italiana, senza aggiungere altri layout.
- **Applicazioni nel Finder** usa la propria icona dedicata anche nella radice del volume, sia in griglia sia in elenco, coerente con la barra laterale. Non è più rappresentata dalla cartella generica. La regola riguarda `/Applications`, non cartelle utente omonime; il simbolo di sistema rimane procedurale, senza sostituzioni con App Store.
- **Reinstalla tinymacOS in Recovery** usa la stessa icona procedurale Tramontana dell'installer sulla scrivania: stessa risorsa di origine, confronto byte per byte a 64 px, poi ridimensionamento alla dimensione di visualizzazione. Non viene più incorporata la vecchia icona dell'installer. Tutte le altre icone Recovery mantengono la politica precedente: procedurali, tranne Terminale importato.

## Verifiche

I controlli della nuova ISO comprendono framebuffer effettivo di Finder in griglia/elenco e barra laterale, titolo Recovery senza la vecchia dicitura, confronto dei cinque simboli Recovery disegnati, risorse effettive del filesystem/atlas e identità di versione. Conservati i test delle dieci combinazioni tema/modalità, associazioni sfondi, wordmark e Mission Control.

Gate completi legati allo SHA-256 della nuova ISO: due build indipendenti identiche; 17 profili input live e 10 installati senza ISO; tastiera italiana nelle app e in Recovery; installer e ripristino locale sigillato; Recovery ottico BIOS con R/F12, tastiere EHCI/xHCI e mouse/tablet USB (otto combinazioni), Recovery ottico UEFI; contratti host ASAN/UBSAN e 29 regressioni kernel.

Le prove ottiche BIOS restano test funzionali, non benchmark della tastiera: l'harness usa impulsi ordinari da 300 ms e impulsi di selezione da 120 ms per il polling del firmware emulato. Il controllo negativo storico 30A2 usa lo stesso harness, immutato; i PASS positivi sono nuovamente prodotti dalla ISO 30A4. Le sonde kernel non sono un browser.

Ricevute: `final-qa.json`, `assets-result.json`, `appearance-result.json`, `recovery-visual-result.json`, `supplementary-selector-result.json`, `artifact.json` e `SHA256SUMS.txt`. Il deploy e i download remoti sono attestati separatamente in `github-release-verification.json`.

## Limiti invariati

**ASRock Q1900M/J1900 non riprovato fisicamente: l'ultimo riscontro è mouse fermo sia nel live sia in Recovery. La soluzione su quella macchina non è confermata.** I PASS QEMU non sono una certificazione hardware, né del selettore firmware con una tastiera PS/2 fisica.

Sistema sperimentale con kernel proprio, non macOS Apple. Safari/WebKit sospeso e non incluso. Nessuna nuova dichiarazione di supporto per OHCI, hotplug generale, hub SuperSpeed o HID universale. La ISO ottica BIOS/UEFI non è un'immagine USB avviabile in modalità DD.
