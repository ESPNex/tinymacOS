# Ricerca storica e correzioni 10.1.1 / 30B2

## Riscontro utente autorevole per questa manutenzione
L’utente riferisce una release precedente indicata come «mouse fix»: Recovery funzionante; sistema normale con movimento solo verticale. Nelle versioni successive il puntatore non funziona né nel Recovery né al login sul PC fisico. Questo riscontro non viene sostituito dai PASS QEMU.

## Ricerca eseguita
- Esaminate tutte le 5 release GitHub presenti, la storia dei commit input e gli 8 tag esistenti, comprese le revisioni archiviate 21B6/21B7/21B8 della 2.4.5.
- Cercati i file tinymacOS su Drive e, separatamente, i nomi contenenti mouse/Mouse/MOUSE anche nel cestino. Nessun file attualmente denominato letteralmente «mouse fix».
- Ritrovata **2.4.3 Levante 21B4**, ISO `tinymacOS-2.4.3-x86_64.iso`, SHA-256 `65b0191f0932e2ba3bdf815bac5da1a9d962b45a8baa2515b52bab90a21fefd1`. La nota conservata `workspace/maintenance-2.4.3/CONSEGNA.md` riporta esplicitamente il Recovery funzionante sul PC dell’utente. File Drive identificato `1xpSCzs3WpJqzVbWzxp3C5XnfOJ_bjCJ4`; metadati dimensione e SHA-256 corrispondono. La copia storica dei sorgenti è conservata sotto `sources/` nella stessa directory di manutenzione.
- Nelle note iniziali della **2.4.5 / 21B6** è ancora riportata la differenza fra Recovery firmware funzionante e login nativo fermo. Commit `b18c4737301f5ba360d14f9e075c0a0b004246ca`, conservato nel tag `v2.4.5-21B6`.

Questi sono riscontri storici verificabili, **non l’identificazione certa del soprannome «mouse fix»** né una ripetizione della prova sul PC fisico. I sorgenti sono stati confrontati senza ripristinare alla cieca kernel, mappe tastiera o vecchi limiti.

## Cambiamenti effettivi
1. **Titolo finestra Recovery:** usa `TMC_CHROME_TEXT_Y=12` condiviso con WindowServer, invece di y+7. Stessa faccia Bold 2, centratura sulla larghezza della finestra, non sullo schermo; resta centrato dopo il trascinamento.
2. **Recovery e firmware:** un’interfaccia USB diretta registrata ma senza eventi non esclude più SimplePointer/AbsolutePointer. I dispositivi firmware vengono individuati dopo il passaggio di proprietà delle sole tastiere, per non conservare oggetti protocollo precedenti al detach. Report inattivi/duplicati non mascherano i percorsi firmware; pressioni e rilasci restano eventi.
3. **Sistema nativo:** conservato il parser HID con X/Y indipendenti e Report ID; nessun ritorno cieco ai vecchi tre byte Boot che potevano trasformare sola X in sola Y. Dopo le scansioni xHCI/EHCI/UHCI, se non esiste alcuna interfaccia HID USB nativa configurata, viene ritentato una sola volta il mux USB2 sui controller Intel commutabili già supportati, limitandosi alla maschera di porte documentata. Nessun remux con un mouse o una tastiera USB già configurati; niente reset di slot/hub già enumerati. Non è un driver specifico ASRock.
4. Nessuna nuova diagnostica F11 al login. Italiano conservato anche in Recovery. Centro di Controllo 10.1 conservato.

## Sicurezza credenziali
Archivio cifrato locale `private-backup/github-access.enc`, escluso da Git e dalle ISO. Chiave casuale consegnata separatamente all’utente, mai salvata nel repository. Verificati autenticazione dell’archivio, round trip e login reale dopo ripristino. I token decifrati e Git rimangono sotto `/var/tmp`.

## Pubblicazione autorizzata
Nuova **10.1.1** normale/Latest. **10 e 10.1 Pre-release**, secondo conferma esplicita dell’utente. La 10.1 (30B1) viene archiviata con gli stessi byte già compilati e collaudati, SHA-256 `32e8ba640c4ba45e8df01db56ad69dfe27bed30497c0ea512a2cb51140b00275`, sorgenti al commit `0965f921147062721fed7e7adea9a750cd0beceb`. Sync sorgenti prima del rilascio; esito effettivo attestato soltanto dalle ricevute finali di pubblicazione.

Le correzioni non costituiscono una certificazione del mouse ASRock Q1900M/J1900 fisico. Non dichiarare il problema hardware definitivamente risolto prima della prova dell’utente.

## Controllo source-first prima degli upload
Il gate di pubblicazione ha rilevato nel primo inventario un file generato `tinymacOS/state/vars.fd`, stato NVRAM QEMU ignorato da Git e non usato dalla build ISO. Nessuna release era ancora stata modificata. L’inventario è stato corretto a **574 file sorgente tracciati**, con gli stessi hash originari; nessun sorgente del sistema né byte della ISO è cambiato. Il vecchio inventario è conservato nel commit 7d204993e5671839c9668f114cd55a94c6b7bcb2. Il gate ripete il confronto con ogni blob del nuovo commit prima degli upload.
