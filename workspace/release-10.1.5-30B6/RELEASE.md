# tinymacOS 10.1.5 Tramontana · build 30B6

**Riscontro dell’utente sulla 10.1.4: mouse fermo sia al login sia in Recovery.** La 10.1.4 è una regressione sul PC fisico e i suoi PASS virtuali non vengono considerati una soluzione.

## Correzioni 10.1.5

- **Mouse Boot: Boot negoziato prima di Report.** Un descriptor leggibile e SET/GET_PROTOCOL riusciti non provano che il mouse invii uno stream Report utilizzabile. Il descriptor resta disponibile per i frame numerati già supportati; Report rimane per dispositivi non-Boot e fallback verificato quando Boot è rifiutato.
- **Pacchetti con padding non più scartati sistematicamente.** Gestiti i frame Boot a otto byte con pulsanti validi/padding riservato zero e i report numerati completi con padding finale zero. Nessuno scambio degli assi o riconoscimento basato sul verso del movimento. Restano limiti per formati intrinsecamente ambigui.
- **Recovery non sottrae più il mouse al firmware.** Non disconnette il proprietario del dispositivo: SimplePointer esistente resta disponibile; EFI_USB_IO diretto serve soltanto le interfacce non reclamate. Ripristinata questa proprietà non invasiva della 21B9, conservando il polling di tutti i puntatori e la ripresa asincrona.
- **EHCI non si ferma sui poll a vuoto.** La schedule periodica viene quiesciuta solo quando occorre davvero modificare un QH o recuperare un errore. Conservati toggle hardware e protezione degli overlay, oltre alle correzioni TD/Short xHCI.

[Analisi, riproduzioni e limiti](https://github.com/ESPNex/tinymacOS/blob/main/workspace/release-10.1.5-30B6/HISTORY-AND-CHANGES.md). Nessuna regola riservata a ASRock o a un VID/PID; tastiera italiana ISO, PS/2 e controller generali conservati.

## Prove nuove e correzione della QA

**Undici nuovi controlli negativi sul vero codice 10.1.4:** nove casi di pacchetti/stream attraverso xHCI, USB2 e Recovery, uno sulla proprietà firmware e uno sullo stop EHCI senza dati. Si compilano sia il trasporto sia il decoder precedenti e si richiede il fallimento di asserzioni di valore, non errori di compilazione. Conservati altri quattro contratti HCD già presenti e le prove X/Y precedenti.

È stato scoperto anche un difetto della pipeline kernel: alcuni launcher cercavano ancora un vecchio nome ISO QA. Corretti nomi e verifiche **prima di ogni avvio**, con hash del media effettivo e KernelCache estratto dalla ISO, confrontato con la build di produzione corrente. Le regressioni sono rieseguite con questa associazione, non certificate riutilizzando i vecchi media. [Dettagli QA](https://github.com/ESPNex/tinymacOS/blob/main/workspace/release-10.1.5-30B6/QA-NOTES.md).

Due build indipendenti identiche. **Gate finale superato: 80 ricevute**, 27 profili input live/HDD con 236 asserzioni, 18 avvii Recovery mouse/tablet e 29 regressioni kernel. Cinque media QA e tredici profili guest verificati contro il kernel di produzione. Installazione BIOS, riavvio HDD, crossboot UEFI e ripristino locale senza ISO superati; 96 cicli prolungati X/Y e 400 verifiche esatte del cursore fra nativo e Recovery. ASan/UBSan e contratti UI superati. `final-qa.json` vincola i risultati agli hash: **non è una ricevuta di verifica fisica o pubblicazione**.

## Centro di Controllo e altre invarianti

Conservati **13 SVG originali**, vetro, un solo Sfondo e un solo accesso alle Impostazioni. Nessun nuovo artwork o redesign in questa manutenzione. Niente nuovo pannello diagnostico F11. Safari/WebKit non incluso.

## Limite fondamentale

**La 10.1.5 non è verificata sul PC fisico ASRock Q1900M/J1900.** Sono corretti difetti software riproducibili che possono bloccare input valido; non abbiamo una traccia USB del mouse dell’utente per dimostrare quale fosse la causa reale. Non si promette il funzionamento hardware sulla base di QEMU. Nessuna richiesta di un’altra foto F11.

## Download

`tinymacOS-10.1.5-x86_64.iso` (**225.710.080 byte**) e `SHA256SUMS.txt` negli asset. **ISO ottica BIOS/UEFI, non immagine USB-DD ibrida**; la ISO non aggiorna automaticamente un sistema già installato. Sperimentare soltanto su dischi virtuali vuoti dedicati, non su dati importanti.

SHA-256:
```text
0021de17c27fc0b23417ef015542adde7b773ed89a1f9028a769ee2724b64653
```

Le dieci release precedenti vengono mantenute **pre-release**, conservando ISO, asset, note e tag. Sorgenti sincronizzati prima degli upload. Solo `github-release-verification.json`, dopo la pubblicazione, certifica Latest e download anonimo completo verificato.

tinymacOS è un sistema sperimentale distinto da macOS Apple.
