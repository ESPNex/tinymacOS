# tinymacOS 2.4.5 Levante — revisione 21B9

Sostituzione della stessa release `v2.4.5`, richiesta per **tastiera solo italiana** e mouse USB che sul PC fisico si muove soltanto in verticale. Safari/WebKit resta fuori da questa ISO.

**DEPLOY COMPLETATO E VERIFICATO:** stessa release ID `453129451`, build **21B9**. ISO riscaricata da GitHub sotto il nome definitivo e checksum confrontato. Tag `v2.4.5` al commit sorgenti `bdf6837e3cc6a1ebfa56c4f7647795aceb1fadd7`; sorgenti precedenti conservati nel tag `v2.4.5-21B8`. Ricevuta `github-release-verification.json`. Le ricevute 21B8 restano storiche.

## Tastiera: ITA, non un’etichetta sopra la mappa US

- Unica mappa fisica italiana ISO condivisa fra HID USB e PS/2: punteggiatura, lettere accentate, Maiusc, Bloc Maiusc, AltGr, euro e tasto ISO `<>`. Rimossa la traduzione US dal decoder; nessun selettore o fallback US.
- AltGr destro non viene più scambiato per Ctrl. Conservati Ctrl destro/sinistro e le scorciatoie già presenti.
- Eventi di testo Unicode e buffer UTF-8: nome e password al login, Terminale, TextEdit, Finder, Impostazioni, Calendario, Note e Memo. Inserimento, cancellazione e posizioni del cursore rispettano i confini dei caratteri; gli accenti non vengono confusi con i codici dei tasti speciali. Validazione UTF-8 dei campi account; hash/password continuano a usare i byte esatti, senza cambiare i vecchi account ASCII.
- **Recupero:** non rimappa alla cieca `UnicodeChar` del firmware. Assume la proprietà delle sole interfacce tastiera HID boot e del figlio PS/2 ACPI PNP0303, senza scollegare controller USB, mouse o disco. Decodifica i tasti fisici con la stessa mappa ITA. Conserva navigazione, ripetizione, Bloc Maiusc e LED. I trasporti dei puntatori di Recupero restano indipendenti dal kernel.

Tasti principali: `AltGr+E` → `€`; `AltGr+ò` → `@`; `AltGr+à` → `#`; `AltGr+è/+` → `[]`; aggiungendo Maiusc → `{}`.

Il font comprende anche il glifo euro in tutte le dieci facce, incluso il sottoinsieme del Recupero: non basta salvare il byte UTF-8 corretto se poi sullo schermo compare `?`. Verificati dati e resa grafica.

## Mouse: X e Y estratti dal layout reale

xHCI preferisce ora il Report descriptor validato anche per mouse che si dichiarano Boot, invece di scegliere in anticipo i byte fissi di un pacchetto Boot. Conferma il protocollo; il fallback Boot richiede una negoziazione riuscita. EHCI/UHCI conservano la preferenza Report già esistente.

Il parser HID mantiene offset separati per ciascun Report ID e gestisce Push/Pop globali con limiti espliciti. X/Y con segno, larghezze fino a 16 bit, pulsanti e rotella vengono letti dai campi dichiarati. Report estranei non spostano gli assi del mouse. Anche il percorso diretto USB del Recupero usa il descrittore quando disponibile.

**Controllo negativo riprodotto:** un report con ID e coordinate a 16 bit, contenente solo X=64, letto erroneamente come Boot produce X=0/Y=64. Il decoder corretto produce X=64/Y=0; verificata separatamente la sola Y. È una riproduzione tecnica del sintomo, **non la lettura del descrittore del mouse fisico dell’utente**.

Conservati hub USB2 xHCI, routing Intel/Bay Trail, PS/2, EHCI/UHCI, correzione alone Dock/Mission Control, allineamento mela/menu e rimozione della diagnostica F11 al login.

## Gate prima della pubblicazione

- Contratti host sui sorgenti reali con AddressSanitizer/UndefinedBehaviorSanitizer: italiano, AltGr, UTF-8, assi indipendenti, report-ID, stack HID, code, recupero endpoint, hub e routing Intel.
- Ricompilazione pulita indipendente: ISO nativa e OpenCore byte per byte identiche.
- Matrice live/installato BIOS e UEFI con selezione esplicita del trasporto del mouse; profili tastiera USB senza i8042. Prove pixel separate orizzontali e verticali; rotella anche su xHCI.
- Italiano su login/password, Terminale, TextEdit e Finder: file UTF-8 e nomi accentati verificati da processi distinti, SHA-256 calcolato nel sistema ospite; blocco e nuovo accesso con password accentata.
- Tastiera italiana e cancellazione UTF-8 nel Recupero, BIOS/UEFI e PS/2/USB senza fallback PS/2 nei profili USB.
- Installer reale, stesso disco avviato BIOS/UEFI, ripristino locale sigillato; regressioni kernel complete e confronto del kernel delle cinque immagini probe con quello di produzione.
- Sei cicli GUI senza alone e allineamento mela/File entro un pixel.

I risultati effettivi sono raccolti in `final-qa.json`, non dedotti da questo elenco.

## Limiti e verifica fisica

Le prove automatiche non certificano una ASRock Q1900M/J1900 o ogni mouse USB. Il funzionamento orizzontale sul PC dell’utente non è stato misurato da remoto. Nessun supporto universale HID/USB: parser e capacità statici limitati, enumerazione a freddo, OHCI e hub SuperSpeed non implementati. In Recupero la tastiera diretta richiede HID boot oppure PS/2 PNP0303: non viene riattivata una mappa firmware US se il dispositivo non è supportato. La ISO è ottica BIOS/UEFI, non un’immagine USB-DD ibrida. Il collaudo tastiera del Recupero BIOS usa la partizione locale del disco installato: la selezione del Recupero ottico BIOS con R non è certificata.

## Risultati finali della build

- **27/27 profili input** (17 live, 10 installati), **236 verifiche raggruppate**; sprite effettivo controllato per sola X e sola Y, oltre a clic/trascinamento/rotella.
- **4/4 profili italiano app** BIOS/UEFI × PS/2/USB, 20 verifiche; **4/4 Recupero italiano**. Altri cinque controlli su Note, Memo, Calendario, ricerca Impostazioni e ricerca Scrivania.
- **29/29 regressioni kernel PASS**. Le cinque immagini probe eseguono il kernel ELF di produzione identico; non sono le ISO pubblicate.
- ISO nativa e OpenCore identiche a una seconda build pulita. Installer BIOS, avvio dello stesso disco UEFI e ripristino locale sigillato PASS.
- Sei cicli Mission Control con zero pixel di alone/differenza al ritorno; centro visibile mela e File entrambi a y=14,5.
- Euro presente nelle dieci facce del font; **2060 glifi preesistenti conservati byte per byte**. Dati UTF-8 e pixel effettivi controllati.

ISO: `tinymacOS-2.4.5-OpenCore-Installer-x86_64.iso` — **221532160 byte**.

SHA-256:
```text
00b15326b3772de4a86a428a4035699fd2b141d1c47e24dd45651cec0c9fd8a0
```

`final-qa.json` è il gate eseguibile per questa precisa ISO. Nessuna conferma fisica del PC dell'utente è implicita nei PASS.

[Scarica la nuova ISO](https://github.com/ESPNex/tinymacOS/releases/download/v2.4.5/tinymacOS-2.4.5-OpenCore-Installer-x86_64.iso) · [SHA256SUMS.txt](https://github.com/ESPNex/tinymacOS/releases/download/v2.4.5/SHA256SUMS.txt). Repository e download privati: serve accesso GitHub autorizzato.
