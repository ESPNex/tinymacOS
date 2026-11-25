# tinymacOS 10.2.1 Tramontana · 30B12

## Novità

- **Liquid Glass uniforme sul desktop:** finestre, barra dei menu, Dock, menu, widget, fogli e dialoghi passano dallo stesso materiale del Centro di Controllo: tinta, trasparenza, blur e rifrazione comuni, in chiaro/scuro e nel fallback opaco. Il Recupero è escluso: usa solo superfici opache in tema scuro.
- **Recupero con kernel nativo separato:** BaseSystem contiene `RECOVERY.ELF` e `RECOVERY.VOL`, non un'applicazione EFI. I bootstrap verificano l'immagine, caricano l'ELF e chiamano `ExitBootServices`. Il kernel Recovery inizializza memoria, scheduler, AHCI e la medesima pila USB/HCD/hub/HID nativa del desktop.
- **Sessione Recovery isolata:** home temporanea in RAM, nessuna autenticazione automatica dell'account installato e nessun montaggio della sua home. Ripristino, reinstallazione locale, copia dei dati e inizializzazione lavorano sul disco SATA selezionabile nella conferma. Terminale e Utility Disco sono applicazioni native, aperte sopra il pannello.
- **Recupero classico, scuro e opaco:** barra dei menu superiore funzionante, nessun Dock, nessuna trasparenza, blur, rifrazione o riflesso Liquid Glass, anche nelle finestre native. Niente doppio contenitore. Conservato lo sfondo nero con alone bianco/grigio soffuso, procedurale e precalcolato.
- **Finestra Recovery essenziale:** soltanto Ripristina, Reinstalla, Utility Disco, descrizioni brevi e Continua. Rimossi titolo duplicato, versione e testi tecnici dalla finestra principale. Terminale, disco di avvio, copia locale e inizializzazione sono nel menu Utility; riavvio e spegnimento nel menu Apple. Versione consultabile da Info sul Recupero. Font identico, byte per byte, a quello del desktop; nessun font alternativo.
- **Plist effettivi:** metadati di sistema/app in XML e kext in binary plist; anche le preferenze di aspetto sono ora `com.tinymacos.appearance.plist`, con dizionario XML e valori tipizzati. Lettura delle vecchie preferenze binarie per la migrazione, senza rinominare semplicemente un blob.
- **Login:** il caret non compare al semplice focus o clic; si attiva dopo la prima digitazione effettiva del campo, mantenendo lampeggio, navigazione e inserimento UTF-8.
- **Dialoghi:** viene disegnato direttamente solo il pannello, senza cornice proprietaria o cattura rettangolare che possa coprire la scena. Le altre app restano aperte, visibili e utilizzabili; Apri/Salva e navigazione sul filesystem restano operativi.
- Font accettato invariato. Il riferimento `0bd59h.zip` non è integrato.

## Avvio e Recupero

ISO ottica BIOS/OpenDuet/OpenCore e UEFI. Premere **R** o **F12** durante la mela. Dopo una nuova installazione, il Recupero è disponibile anche dallo stesso disco senza CD. La ISO non aggiorna automaticamente una precedente installazione.

Nel Terminale Recovery, `csrutil enable` / `csrutil disable` salva una preferenza SIP verificata sul **primo disco tinymacOS trovato**, applicata al riavvio. La configurazione usa il settore BootConfig preallocato e verificato; non richiede servizi di avvio UEFI né scritture arbitrarie sul filesystem firmware. Il desktop normale può consultare SIP, ma non modificarlo con questo comando.

Le copie locali sul medesimo disco non proteggono da un guasto fisico. Reinstallazione e ripristino usano la copia locale sigillata: non scaricano macOS o componenti Apple.

## Limiti

Sistema sperimentale, non macOS e non compatibile con applicazioni macOS. Nessuna promessa di compatibilità USB/HID universale. Browser WebKit non incluso; ISO non USB-DD ibrida. Usare dischi virtuali o vuoti dedicati, mai dati da conservare.

**ASRock Q1900M/J1900 non collaudata fisicamente:** il nuovo percorso Recovery elimina la dipendenza dall'input firmware, ma i risultati QEMU non dimostrano che il mouse dell'utente funzioni già su quella scheda. Non viene dichiarato risolto un test fisico mai eseguito.

I risultati della ISO pubblicata, il commit e le impronte SHA-256 sono allegati alla release in `VALIDAZIONE.json` e `SHA256SUMS.txt`.

## Collaudi della candidata pubblicata

- Recovery nativo su ISO UEFI/xHCI, BIOS/xHCI e UEFI/EHCI con mouse e tastiera USB: oltre 75 secondi per profilo, coordinate del cursore verificate sui pixel dopo ciascun evento, apertura via mouse di Terminale e Utility Disco.
- Installazione grafica BIOS su SATA virtuale, rimozione ISO, riavvio BIOS e cross-boot UEFI. Recovery avviato dal disco senza CD: copia locale, ripristino e reinstallazione; documento e credenziali conservati. Preferenza SIP persistente e modifica negata dal desktop ordinario.
- Dodici controlli Apri/Salva e login, più glass chiaro/scuro e trasparenza su sfondi diversi. Il test host del vero shader verifica uguaglianza pixel per pixel dei sei tipi di superficie rispetto al Centro di Controllo, incluso fallback opaco.
- Tutti i plist dei tre volumi di sistema validati con `plistlib`; lettura indipendente anche dei due plist realmente persistiti dal kernel sul disco installato.
- Suite host USB/HID e preferenze con ASan/UBSan: 79 righe di contratti PASS. Restano test software, non certificazione hardware.

## Rimpiazzo della stessa 10.2.1

La release e il tag `v10.2.1` vengono aggiornati mantenendo versione 10.2.1 / build 30B12. La precedente variante Recovery con Liquid Glass è sostituita dalla versione opaca, scura e con barra dei menu. La 10.2 non viene modificata.
