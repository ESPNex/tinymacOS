# 10.1.5 / 30B6 — correzione della regressione 10.1.4

## Riscontro fisico, non interpretazione della VM

L’utente riferisce **mouse completamente fermo al login e in Recovery sulla 10.1.4**. La 10.1.3 aveva Recovery solo verticale; la 10.1.2 aveva login fermo e breve movimento poi blocco in Recovery. La 2.4.5 Levante / 21B9 resta il riferimento esatto con Recovery funzionante e login verticale.

La 10.1.4 non viene considerata un successo sulla base dei suoi 62 ricevuti PASS. Non abbiamo una traccia dei pacchetti del mouse ASRock, né accesso al controller fisico: non è possibile attribuire con certezza il riscontro a uno solo dei seguenti difetti. Non viene richiesta un’altra foto F11.

## 1. La scelta Report non doveva sostituire automaticamente Boot

La 10.1.4 imponeva Report quando un descriptor era interpretabile e SET/GET_PROTOCOL confermava Report. Questo non verificava la consegna dei pacchetti IN. Un dispositivo Boot che continua a emettere il formato corto può risultare enumerato ma produrre **zero eventi utilizzabili**, perché `pointer_read()` richiede tutti i byte del descriptor.

La 10.1.5 negozia prima Boot per i mouse che lo dichiarano. Conserva il descriptor come struttura per i frame numerati già supportati. Se Boot è rifiutato, un descriptor valido e Report verificato restano un fallback; i dispositivi non-Boot e i tablet mantengono il proprio decoder. Tastiere e loro protocollo non sono cambiati.

Non è un semplice ripristino della 10.1.3: la decodifica Report-ID che conserva X/Y indipendenti rimane, con i contratti precedenti. Resta un limite per formati intrinsecamente ambigui fra Boot e Report: non si pretende di riconoscerli tutti senza osservare il dispositivo.

## 2. Scarto sistematico dei frame con padding

In 10.1.4, se era presente un descriptor relativo numerato più lungo di quattro byte, il percorso Boot scartava tutti i pacchetti lunghi che non avessero **esattamente** ID e lunghezza del descriptor. Esempi riprodotti:

```text
00 25 00 00 00 00 00 00  → Boot X=37/Y=0, padding zero: prima nessun evento
04 00 25 00 00 00 00 00  → Report ID 4, X=37/Y=0 + padding: prima nessun evento
```

Ora un frame numerato completo accetta il padding finale soltanto se zero. Il fallback esplicito Boot a otto byte richiede pulsanti standard e quattro byte riservati finali zero. Frame troncati, ID estranei e dati riservati non validi restano esclusi fuori da queste forme. Non si usa il verso del movimento per scegliere il decoder e non si scambiano X/Y.

I primi tre casi (stream Boot nonostante ACK Report, Boot con padding, Report numerato con padding) passano attraverso **xHCI, USB2 e Recovery** nei test. I medesimi test compilano sia il trasporto sia il decoder originali 10.1.4: falliscono su asserzioni di valore/evento, non su errori di compilazione. Sono nove riproduzioni, non nove mouse fisici.

## 3. Recovery: non distruggere il percorso firmware

La 21B9 non disconnetteva i proprietari delle interfacce USB già reclamate. Le versioni successive prendevano in carico il figlio mouse prima di sapere se il loro stream fosse effettivamente utilizzabile, invalidando SimplePointer del firmware.

La 10.1.5 **non chiama DisconnectController sul mouse**: mantiene il proprietario firmware, scopre e interroga i suoi protocolli pointer. Il diretto EFI_USB_IO serve alle sole interfacce non reclamate. Mantiene polling di tutti i puntatori, filtro dell’attività idle, accumulo relativo corretto e ripresa asincrona del diretto.

Il decimo controllo negativo dimostra che 10.1.4 disconnette il proprietario nello stesso fixture in cui 10.1.5 lo lascia intatto. Non dice che il firmware di qualunque scheda fornisca necessariamente un SimplePointer corretto. UsbMouseDxe OpenCore resta disabilitato come nella configurazione approvata; non è stato ripetuto il precedente tentativo fallito di abilitarlo.

## 4. EHCI: evitare lo stop ad ogni poll

La protezione degli overlay della 10.1.4 fermava il periodico anche quando nessuna TD era completata e nessuna riscrittura fosse necessaria. Questo aggiunge attese e può interferire con le opportunità di polling su controller reali. Ora si osservano prima le TD: **schedule invariata sui poll a vuoto**, quiescenza soltanto prima di installazione/riarmo/recupero effettivi.

L’undicesimo nuovo controllo negativo verifica sul codice 10.1.4 lo stop del periodico a endpoint attivo e senza dati; 10.1.5 lascia PSE/PSS attivi e non esegue attese. Conservati gli altri quattro contratti Linux-reviewed della 10.1.4 (pubblicazione TD xHCI, Short Data/Status, toggle hardware EHCI, quiescenza overlay). Il riferimento upstream e la sua provenienza restano nell’archivio 10.1.4; non è stato importato codice Linux.

## Invarianti conservati

- SVG originali del Centro di Controllo, vetro e deduplicazione: nessuna modifica agli artwork o al renderer.
- Tastiera italiana ISO, PS/2, routing Intel e controller generali; nessuna eccezione VID/PID/ASRock.
- Niente nuovo pannello F11, nessun browser Safari/WebKit finto.
- Sorgenti sincronizzati prima degli upload; vecchie release mantenute pre-release conservando ISO, altri asset, note e tag.

## Cosa non si può dichiarare

La 10.1.5 corregge casi software nuovi e riproducibili che i vecchi test non coprivano. **Non si dichiara risolto il mouse fisico.** Stato dei gate in `final-qa.json`, pubblicazione e download anonimo completo solo in `github-release-verification.json` quando presente.
