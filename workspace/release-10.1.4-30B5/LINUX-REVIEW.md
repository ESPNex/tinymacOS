# Confronto sorgente Linux → tinymacOS 10.1.4

Riferimento esplicito e riproducibile: **Linux v6.12** (non un’affermazione che sia il kernel Linux più recente). Letti i sorgenti upstream; inventario con URL e SHA-256 in `linux-reference-manifest.json`. Il codice dei driver Linux non è stato importato o compilato dentro tinymacOS: sono stati applicati invarianti del protocollo con implementazioni locali e test sul vero codice.

| Riferimento Linux | Verifica nel nostro codice | Modifica 10.1.4 |
|---|---|---|
| [`xhci-ring.c`, `giveback_first_trb`, righe 3368+ e costruzione Control TD](https://github.com/torvalds/linux/blob/v6.12/drivers/usb/host/xhci-ring.c#L3368) | Prima `push()` pubblicava subito il Cycle del Setup mentre Data/Status non erano ancora completi. Un xHC già attivo può leggere prima del prossimo doorbell. | Accodamento iniziale con ciclo non posseduto; barriera di memoria; pubblicazione del primo TRB solo dopo la costruzione dell’intero TD. Conservato il ciclo originario anche al wrap del ring. |
| [`xhci-ring.c`, `process_ctrl_td`, righe 2286–2390](https://github.com/torvalds/linux/blob/v6.12/drivers/usb/host/xhci-ring.c#L2286) | Prima la lunghezza effettiva dei trasferimenti di controllo non era tracciata; il completamento Short sul Data veniva scartato. | ISP per Data IN, lunghezza richiesta meno residuo dal Data Short, attesa dello Status prima del successo; verifica delle lunghezze di device/config/report descriptor e GET_PROTOCOL. Un campo non ricevuto non è una conferma di protocollo. |
| [`ehci-q.c`, `qh_update`, righe 76–103](https://github.com/torvalds/linux/blob/v6.12/drivers/usb/host/ehci-q.c#L76) | Prima il data-toggle interrupt era sempre software e il QH veniva azzerato/riscritto con schedule periodico attivo. | DTC solo per endpoint di controllo, data-toggle interrupt mantenuto dall’hardware e conservato dal QH. Quiescenza PSS prima delle riscritture dell’overlay, riavvio dopo il batch. CLEAR_FEATURE resetta il toggle quando serve, non ad ogni completamento. |
| [`ehci-q.c`, costruzione QH, righe 870–935](https://github.com/torvalds/linux/blob/v6.12/drivers/usb/host/ehci-q.c#L870) | Retry/NAK count non pertinente ai QH periodici. | RL rimane per il controllo, zero per gli interrupt. Conservati TT, porta hub, maschere split e routing companion. |
| [`hid/usbhid/hid-core.c`, `usbhid_parse`, `usbhid_start`, `hid_irq_in`](https://github.com/torvalds/linux/blob/v6.12/drivers/hid/usbhid/hid-core.c) e [`hid/hid-core.c`, estrazione campi](https://github.com/torvalds/linux/blob/v6.12/drivers/hid/hid-core.c) | Un’etichetta Boot dell’interfaccia non descrive i bit dei report; Report ID e campi X/Y devono essere separati dal trasporto. | Preferenza al descriptor e a Report verificato, fallback Boot negoziato. Aggiunta una protezione comune alle tre vie per framing ID/16 bit, descritta sotto. |

## Riproduzione del solo asse verticale

Pacchetto relativo con Report ID 4 e assi firmati a 16 bit:

```text
04 00 40 00 00 00 00  → descriptor: X=64, Y=0
                       vecchi offset Boot: X=0, Y=64
04 00 00 00 40 00 00  → descriptor: X=0, Y=64
```

Il nuovo `pointer_boot_read()` conserva un descriptor valido anche nel fallback Boot. Se arriva un pacchetto relativo **numerato, completo, di lunghezza esatta e con ID esatto**, usa i campi dichiarati anziché trattare ID e pulsanti come assi. Non sceglie un formato dal verso del movimento; nessuna inversione/scambio X/Y, VID/PID o regola ASRock. Gli ID estranei o i frame lunghi incompleti non diventano movimenti Boot. I pacchetti Boot corti regolari restano Boot. I report senza ID non vengono riconosciuti tramite euristiche di movimento.

Questa protezione di framing è una correzione specifica del nostro percorso di fallback, **non codice copiato da Linux**. La stessa sequenza di byte passa attraverso xHCI, USB2 e Recovery nei contratti positivi. Il vero sorgente 10.1.3, ricompilato con gli stessi byte, fallisce le asserzioni X/Y in tutti e tre i percorsi. Quattro ulteriori controlli negativi coprono pubblicazione TD, lunghezza Short, toggle EHCI e quiescenza dell’overlay EHCI. Quest’ultimo verifica installazione, completamento/riarmo e mancata scrittura del QH se PSS non si spegne entro il timeout.

## Confine Recovery/kernel

La Recovery continua a usare EFI_USB_IO e i servizi firmware per i controller: non avvia il kernel/HCD nativo mentre il firmware possiede disco e tastiera. Condivide il parser/decodificatore dei pacchetti con il nativo e conserva acquisizione del solo figlio mouse, coda di delte relative e ripresa asincrona della 10.1.3. I test sui controller nativi non sono spacciati per test del controller firmware.

## Limiti

Il nuovo riscontro dell’utente è **Recovery funzionante ma solo verticale**; non si afferma che la 10.1.3 fosse corretta. La riproduzione sopra dimostra un errore software con quel sintomo, non che il mouse fisico invii sicuramente quei byte: non abbiamo acquisito il suo traffico USB. I risultati ASan/UBSan e QEMU restano distinti dalla verifica fisica ASRock Q1900M/J1900. Nessuna ulteriore foto F11 richiesta.
