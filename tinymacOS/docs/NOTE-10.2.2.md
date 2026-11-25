# tinymacOS 10.2.2 Tramontana · 30B13

## Novità

- **FileVault nativo per l'intero volume dati.** Cifra documenti, nomi dei file, metadati, account, snapshot e copie locali sul volume. Il formato è proprio di tinymacOS: non legge o scrive FileVault/APFS Apple.
- **Sblocco prima del desktop**, con password o chiave di recupero indipendente. Dopo lo sblocco con recupero è ancora richiesta la password dell'account per entrare nella sessione.
- **Gestione da Impostazioni e Recovery:** attivazione, stato e avanzamento, disattivazione, cambio password del volume, rigenerazione della chiave di recupero e selezione del disco in Recovery. Disponibili anche `fdesetup status` e `fdesetup manage` nel Terminale.
- **Conversione sul volume esistente**, senza formattazione automatica. Journal, header ridondanti e progresso persistente consentono la ripresa dopo un'interruzione. È comunque indispensabile una copia esterna prima di operare su dati importanti.
- **Cifratura autenticata XChaCha20-Poly1305**, chiave volume casuale, keyslot separati, PBKDF2-HMAC-SHA256 a 600.000 iterazioni e header autenticati. Primitive Monocypher 4.0.2, con sorgenti, provenienza e licenza inclusi.
- **Finder: basta il doppio tap.** Il primo seleziona e il secondo apre file o cartelle, anche con un piccolo spostamento del puntatore. Gli eventi conservano il timestamp di cattura: la coda e il rendering non devono trasformare un doppio clic in tre clic.

## Conservato dalla 10.2.1

Recovery con kernel separato, barra dei menu, tema scuro opaco e sfondo nero con alone neutro; font identico al desktop. Desktop Liquid Glass, dialoghi senza contenitore aggiuntivo e senza nascondere le altre applicazioni. Plist reali, input USB nativo e installazione ottica BIOS/UEFI.

## Utilizzo e sicurezza

Per attivare: **Impostazioni → Sicurezza e privacy → FileVault**. Salvare la chiave di recupero su un supporto esterno e confermare gli ultimi otto caratteri. In Recovery: **Utility → Terminale**, eseguire `fdesetup manage`, selezionare il disco nella sezione FileVault di Impostazioni e sbloccarlo prima delle operazioni sui dati. La vecchia cifratura per-file deve essere disattivata prima dell'attivazione del nuovo formato.

EFI, partizioni di avvio e sistema Recovery restano pubblici. La cifratura protegge i dati a riposo, non RAM, firmware o sessioni già compromesse. Non impedisce il ripristino di una vecchia immagine completa del disco. Senza password e chiave di recupero non è previsto un accesso alternativo. Le copie locali sullo stesso disco non sono un backup contro i guasti fisici.

Dettagli del formato e limiti: [FileVault nativo](FILEVAULT-10.2.2.md).

## Collaudo e limiti

La release allega `VALIDAZIONE.json` con il commit, la SHA-256 della ISO e i risultati specifici dei test effettivamente eseguiti. Le suite comprendono confronto crittografico indipendente con hashlib/libsodium, fault injection host, prove sulla ISO in QEMU e regressione del doppio clic Finder. Non costituiscono un audit esterno o una certificazione hardware.

Sistema sperimentale x86-64: non è macOS e non esegue applicazioni macOS. Browser WebKit non incluso; compatibilità USB/HID non universale; nessun supporto PS/2. ISO ottica BIOS/UEFI, **non USB-DD ibrida**. Usare macchine virtuali o dischi di prova vuoti dedicati. La ISO non aggiorna automaticamente una precedente installazione.

**ASRock Q1900M/J1900 non collaudata fisicamente.** Le verifiche virtuali non dimostrano compatibilità con quella scheda. Il formato cifrato è nuovo e sperimentale: non usarlo come unica copia di dati importanti.

La pubblicazione 10.2.2 è separata: non sostituisce la 10.2.1.

## Rimpiazzo della stessa 10.2.2

FileVault è ora una **sezione interna di Impostazioni di Sistema**, raggiungibile da Sicurezza e privacy: nessuna app FileVault separata e nessun avvio di Utility Disco per gestire la cifratura. La schermata mantiene solo stato, azioni, progresso e istruzioni essenziali per le credenziali. Le indicazioni tecniche restano nella documentazione.

In Recovery la voce FileVault è rimossa dal menu Utility. L'accesso avviene dal Terminale con `fdesetup status` e `fdesetup manage`; quest'ultimo apre la stessa sezione integrata in Impostazioni per inserire i segreti nei campi mascherati, senza salvarli negli argomenti o nella cronologia della shell.

Versione 10.2.2 e build 30B13 conservate; ISO, sorgenti, impronte e validazione della release sono sostituiti. Le modifiche sono integrate nel ramo `main`. Nessuna modifica al formato cifrato e nessuna sostituzione della 10.2.1.
