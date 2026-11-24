# tinymacOS 10.1.8 / 30B9 — note della build locale

Questa revisione non e una pubblicazione automatica su GitHub.

## Interfaccia
- Impostazioni: sei icone laterali con sorgenti SVG originali in
  `assets/settings/`, compilate in maschere alpha antialias e renderizzate
  nativamente. Nessun ricorso ai vecchi sprite per queste icone; mappatura
  preservata anche filtrando la ricerca. Simboli bianchi su riquadri colorati.
- Risoluzione: menu contestuale compatto, larghezza calcolata sulle voci reali
  (132 px con il profilo corrente, invece di 246), righe di 24 px, colonna
  separata per il segno di spunta e ancoraggio al valore selezionato.
- Materiale Liquid Glass e colori semantici del tema, chiaro/scuro e opaco.
- Wordmark: `tiny` e `macOS` sono adiacenti usando gli avanzamenti del font
  caricato. Rimosso lo spostamento fisso di 72 px: il profilo utente richiede
  57 px. Restano distinti il peso normale e il Semibold dritto.
- Dock: vetro piu uniforme sulla superficie completa.

## Video e Recovery
- Titoli delle cinque card nella schermata principale Recovery: Regular reale
  a 14 px, senza Bold/Semibold. Gli altri titoli non cambiano peso.
- Nessuna chiamata GOP dopo ExitBootServices. Cambio runtime limitato al
  backend emulatori riconosciuto; sulle altre GPU scelta salvata e verificata
  in Recovery/BOOTCFG.BIN, applicata dal loader al successivo avvio.
- Persistenza disponibile su disco installato, non su ISO live. Errori visibili;
  impostazione verbose preservata anche aggiornando la risoluzione e viceversa.
- Recovery preferisce i protocolli firmware e non scollega HID gia gestiti;
  collegato il fallback UsbIo per mouse non rivendicati, con nuova enumerazione
  periodica e fallback per assi attivi con risoluzione firmware nulla.

## Font e riproduzione
`tools/interface-font.py` rasterizza quattro tagli esterni dritti 400/500/600/700
senza simulare pesi o riscrivere metadati. Il profilo viene selezionato con
`TMC_FONT` per cataloghi live/installato e Recovery. I font binari forniti
restano esterni alla repository; la build standard usa gli asset liberi
preesistenti. La provenienza/licenza dei file utente non e certificata come
Inter/OFL. Il successivo archivio di riferimento ap2ky2.zip risultava troncato
nel taglio Semibold e non e stato integrato. L'archivio 0bd59h.zip e stato
fornito solo come riferimento e, su conferma esplicita dell'utente, non viene
usato. Questa build mantiene esattamente il profilo della ISO UI-fix precedente
(o24pkl.zip; TFN SHA-256 b2dc1065c12eb43378e02d824af58697369760c268f2dbc64542dcac04a514d2).

## Verifiche e limiti
- Lettore metriche reale testato con ASan/UBSan: font standard e fornito,
  letture parziali, cache, UTF-8, header errati/troncati e apertura negata.
- Controlli della ISO reale: menu compatto/click, cambio scanout, temi e
  confronto del wordmark con i glifi effettivi; cataloghi e identita 30B9.
- Le prove precedenti su USB/HID e installazione restano pertinenti solo ai
  componenti verificati byte-identici: non e una nuova certificazione fisica.
- ASRock Q1900M/J1900 da riverificare dall'utente, in particolare Recovery.
- ISO ottica BIOS/UEFI, non USB-DD ibrida. Nessun browser WebKit incluso.
