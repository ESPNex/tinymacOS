# tinymacOS 10.2.5 — validazione 30B20

Aggiornamento della stessa release 10.2.5, una sola ISO BIOS+UEFI.

## Correzione mouse USB

Gli adattatori nativi xHCI/EHCI/UHCI non trasformano più il quarto errore
transitorio in uno shutdown definitivo della classe mouse. Un invio fallito
in modo temporaneo e un CLEAR_HALT fallito restano recuperabili. I tentativi
sono distanziati (100–800 tick, contatore saturato), uno per scadenza/polling,
non un loop attivo. CLEAR_HALT solo per STALL; stop per cancellazione, rimozione,
shutdown ed endpoint invalido. Nessun reset del controller o remux della tastiera.

Riproduzione negativa sul codice pubblicato 30B19: fallimento al quarto errore
in tutti e tre gli adattatori. Stesso contratto dopo la modifica: PASS normale
e ASan/UBSan, 12 errori consecutivi, invio ENOMEM, CLEAR_HALT fallito, ripresa dei
report, tastiera su endpoint distinto ancora armata e stop sui codici terminali.
Il test esegue i callback/arm/poll degli adattatori reali con trasporto e notifiche
di classe controllati; non è una simulazione del dispositivo fisico dell'utente.
I test separati eseguono anche URB/usbmouse/generic/HCD Linux effettivi e DMA.

**Il difetto di mancato recupero esisteva anche nella 10.2.4.** La riproduzione
non dimostra quale cambiamento abbia provocato il blocco fisico nella 10.2.5,
né dimostra che questo aggiornamento lo risolva su quel PC. Non si dichiarano
provati conflitti NVMe/USB, non si cambia speculativamente l'ordine di avvio.
Desktop/main e input.c restano byte-identici alla 10.2.4; DnD file disabilitato.

## Recovery senza installazione precedente

L'utente ha confermato di usare Reinstalla di Recovery senza tinymacOS installato.
Quel messaggio non era una prova di GPT Windows corrotta. L'interfaccia distingue
ora assenza di installazione nativa e sistema rilevato ma non reinstallabile;
indica il desktop live per installare da zero. Nessun controllo disco disabilitato,
nessuna formattazione nuova esposta dalla sola Recovery senza il relativo payload.

Per una nuova installazione: avviare normalmente la ISO, **senza Recovery**,
entrare nel desktop live e aprire «Installa tinymacOS Tramontana».

## Controlli freschi della build finale

| Gruppo | Esito |
|---|---|
| Suite host | 8/8 |
| Callback USB nativi xHCI/EHCI/UHCI con fault injection | normale + ASan/UBSan |
| Riproduzione negativa su sorgenti pubblicati precedenti | 3/3 fallimenti attesi al quarto errore |
| USB input/URB/generic/HCD, teardown e allocazioni | PASS |
| Wizard Recovery/live e scanner GPT | normale + ASan/UBSan |
| Installer effettivo FS2/4Kn, cifratura, preservazione partizioni | normale + ASan/UBSan |
| Suite guest della ISO nuova | 8/8 |
| Recovery ottica nativa: movimento mouse relativo, clic, guida senza sistema | PASS; zero modifiche ai metadati/bordi occupati |
| Installer GPT senza 55AA: consenso, annullamento, preservazione e cold NVMe | PASS, fixture sintetica |
| Finder context Cut/Copy/Paste e Terminale/appunti con mouse relativo | PASS; persistenza a freddo |
| Installazione/avvio 4Kn, Recovery/reinstall senza modificare dati | PASS |
| Conversione/deattivazione cifratura 4Kn in-place, account/file/snapshot | PASS |
| Avvio ottico SATA BIOS e UEFI | PASS |
| Mouse relativo in setup/desktop BIOS e UEFI | PASS QEMU; clic e trascinamento finestre |
| Audit font/versione/panic live, installato e Recovery | PASS |
| Riproduzione font, contratto rollback, source-sync e cleanup | PASS |

8 suite host: usb-retry-native, input-maintenance, linux-xhci, linux-usbhid,
installer-flow, windows-gaps, native4k-install, fs2-install.
8 suite guest: Recovery senza installazione, PMBR, appunti/trasferimenti senza
DnD, media ottici, installazione 4Kn, conversione 4Kn, puntatore UEFI e BIOS.
I conteggi precedenti non vengono riattribuiti alla 30B20. Le normali prove VM
non iniettano fault nei driver del prodotto: la riproduzione degli errori è host.

756 sorgenti/header/assembly/font selezionati e build.sh congelati prima del
rebuild e ricontrollati; pin vendor e audit packaging verificati separatamente.
Font UI forniti, Mono Regular panic/Terminale, avanzamento 8 px e nessun fallback
DejaVu conservati. FS2, supporto NVMe/4Kn/cifratura e visibilità EFI/Recovery
inalterati. L'helper NVMe senza payload OS è conservato byte per byte dalla 30B18;
il suo precedente boot test resta storico, non una nuova prova della 30B20.

## Limiti e distribuzione

**Mouse fisico ancora da confermare; nessuna certificazione Windows reale.**
Fare un backup prima dell'installazione. RAW/MSR/NTFS restano occupati; GPT
ambigue protette. Nessun Safari/WebKit completo. ISO non USB-DD ibrida. Restano
le limitazioni di licenza degli ZIP font forniti, documentate in licenses/FONTS.txt.

- ISO: `tinymacOS-10.2.5-x86_64.iso`
- Byte: 329510912
- SHA-256: `f73d31321df316bd5d5c7502f1819ef0fe448718665d5b3f14538d8006b65907`
- TFN2: `24add187754c57f6d8f160fe0a2608a9b946979713eec2b92dc653cb4daf97a6`

Prove: `workspace/local-10.2.5/hotfix/evidence-30B20.tar.xz` e
`verification-30B20.json`. Download/pubblicazione controllati separatamente in
`publication-readback-30B20.json`.
