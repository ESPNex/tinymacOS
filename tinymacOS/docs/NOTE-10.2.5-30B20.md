# tinymacOS 10.2.5 — build 30B20

Aggiornamento della stessa release 10.2.5, con una sola ISO BIOS+UEFI.

## Mouse: recupero degli errori USB, non un altro rollback nominale

L'utente conferma che il mouse fisico resta fermo anche dopo la 30B19, mentre
sullo stesso PC funziona con la 10.2.4. Il ripristino byte per byte di
`desktop/main.c` e `drivers/input.c` non è bastato. Resta applicato e il
trasferimento file drag-and-drop resta disabilitato.

Riprodotto con i callback effettivi degli adattatori xHCI/EHCI/UHCI un difetto
concreto: dopo quattro completamenti con errore temporaneo il codice convertiva
l'errore in shutdown per la classe mouse e smetteva definitivamente di armare
l'endpoint. Anche un invio fallito o un CLEAR_HALT fallito potevano interrompere
permanentemente le letture. La tastiera su un altro endpoint poteva restare attiva.

Il recupero ora:

- non converte una sequenza di errori temporanei in uno shutdown fittizio;
- programma un solo tentativo per polling alla scadenza, con ritardo crescente
  limitato a 800 tick e contatore saturato, senza cicli di attesa attiva;
- ritenta invii falliti temporaneamente e conserva il recupero pendente se il
  CLEAR_HALT fallisce; invia CLEAR_HALT solo dopo un vero STALL;
- interrompe invece i tentativi su cancellazione, rimozione, shutdown o endpoint
  non valido; non reimposta controller né cambia routing delle porte attive;
- non sintetizza movimento o ripete vecchi delta; mantiene il rilascio dei
  pulsanti della classe interessata e non ferma l'endpoint della tastiera.

Le sorgenti Linux vendorizzate restano inalterate. Il difetto di recupero era
presente anche nei sorgenti 10.2.4: **averlo corretto non dimostra quale differenza
abbia innescato la regressione fisica nella 10.2.5**. Nessuna diagnosi certa di
conflitto NVMe/USB e nessun cambiamento speculativo all'ordine di avvio, al
supporto NVMe o ai payload 4Kn. La conferma sul PC reale resta necessaria.

## Recovery senza tinymacOS installato

L'utente ha confermato di aver usato «Reinstalla» di Recovery anche senza
un'installazione precedente. Il vecchio messaggio «Serve un’installazione
nativa con tabelle integre» mescolava assenza di sistema nativo e fallimento
delle verifiche: **non provava una GPT Windows corrotta**.

Ora l'assistente distingue i casi e spiega che per una nuova installazione
occorre avviare il desktop live della ISO. Recovery conserva la reinstallazione
del solo sistema esistente; non viene autorizzata a formattare un disco Windows
né viene inventato un payload di installazione non presente nella Recovery.

Per installare da zero: riavviare la ISO normalmente, **senza scegliere Recovery**,
arrivare al desktop live e aprire «Installa tinymacOS Tramontana». Se nel desktop
compare di nuovo una protezione della tabella, è un controllo diverso e va
valutato con il suo motivo preciso, senza trasformare RAW/MSR/NTFS in spazio libero.

## Preservato

Font UI forniti e Mono Regular per Terminale/panic, nessun fallback DejaVu,
avanzamento Terminale 8 px; FS2, installazione/boot 4Kn, cifratura sullo stesso
extent e Recovery; Finder Taglia/Copia/Incolla e appunti Terminale. Nessuna
modifica alla visibilità EFI/Recovery. Restano le protezioni GPT/MBR della 30B19.

Nessun Safari/WebKit completo incluso. ISO non USB-DD ibrida. I test QEMU e le
fixture sintetiche non certificano Windows reale né il mouse del PC dell'utente.
Esiti freschi e hash: `VALIDAZIONE-10.2.5-30B20.md` e
`workspace/local-10.2.5/hotfix/verification-30B20.json`.
