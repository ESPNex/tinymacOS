# tinymacOS 10.1.1 Tramontana · build 30B2

Release di manutenzione normale / **Latest**. Le versioni **10** e **10.1** sono archiviate come **Pre-release**; la ISO 10.1 conserva gli stessi byte compilati in precedenza.

## Correzioni
- **Recovery:** titolo centrato nella barra con geometria condivisa con le finestre desktop, anche dopo il trascinamento; Terminale sulla stessa linea di testo.
- **Puntatore Recovery:** un’interfaccia USB diretta inattiva non esclude più i dispositivi firmware. Individuazione dei protocolli dopo l’assegnazione delle tastiere; i report senza movimento né variazione dei pulsanti non nascondono le altre sorgenti.
- **Input nativo:** preservati X/Y indipendenti, Report ID, EHCI, PS/2, hub e tastiera italiana. Solo in assenza di interfacce HID USB native configurate viene ritentato una volta l’instradamento USB2 documentato sui controller Intel già supportati. Nessun reset degli slot/hub già enumerati; nessun workaround legato al nome della scheda madre.
- **Centro di Controllo 10.1 conservato:** pannello in vetro completo, controlli reali, temi e sfondi. Nessun pannello diagnostico F11 al login.

## Verifiche e limiti
La ISO è stata prodotta da due build indipendenti e confrontata byte per byte. Le ricevute `final-qa.json` e i risultati specifici sono nei sorgenti della release: test ASan/UBSan dell’arbitraggio Recovery con controllo negativo, input nativo, controlli grafici reali, installazione BIOS con avvio da solo HDD e cross-boot UEFI, ripristino locale, tastiera italiana e regressioni kernel/runtime.

**Il mouse sull’ASRock Q1900M/J1900 fisico non è stato verificato.** I test QEMU e le correzioni concrete non dimostrano che il problema segnalato su quel PC sia definitivamente risolto. La ricerca storica ha ritrovato la 2.4.3 con il riscontro dell’utente «Recovery funzionante», ma non ha identificato con certezza la versione soprannominata «mouse fix».

tinymacOS è un sistema sperimentale distinto da macOS Apple. **Safari/WebKit non è incluso** e non è stato sostituito da un browser fittizio. I test runtime non sono test di accettazione di un browser.

## Download
- `tinymacOS-10.1.1-x86_64.iso` — 225.617.920 byte; ISO BIOS/UEFI.
- `SHA256SUMS.txt` — verifica con `sha256sum -c SHA256SUMS.txt` nella cartella del download.

SHA-256:
```
fbffff11eb1c0aa6856c8e6f74dbedc2fb10df5698e4fb49de9b860eac5ca508
```

Sorgenti sincronizzati prima della pubblicazione; commit e verifica indipendente degli asset scaricati attestati nella ricevuta `github-release-verification.json`, aggiunta dopo il rilascio. Repository privato: i download GitHub richiedono l’account autorizzato. Credenziali e chiave escluse dal repository e dalle ISO.
