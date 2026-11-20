# Collaudo · tinymacOS 1.2 Aurora

**48 verifiche di accettazione superate sullo stesso CD avviabile**, non su un mock. SHA-256 del CD: `5d9ec579fceb3ba149221a56c809a484a7acb1857bf420439346e76501378316` (25.014.272 byte). Ricompilato da una seconda directory sorgente con directory oggetti diversa: `cmp` byte per byte positivo. Nel workspace restano sorgenti, prove e registrazione: il CD di collaudo è stato generato in `/tmp`.

## Cosa è stato controllato

- **Avvio:** OVMF, El Torito, loader PE32+, long mode, separazione di memoria e Ring 3. QMP ha catturato **35 larghezze differenti** della parte chiara della barra, da 0 a 192 pixel, sotto la mela sullo sfondo nero. I frame sono quelli del framebuffer GOP. I tempi non sono una promessa per altri host.
- **Accesso:** creazione al primo boot; password troppo corta e conferma diversa rifiutate. Prima dell’accesso nessuna finestra o scorciatoia di avvio; un processo CPL3 riceve il rifiuto sulle chiamate a file, avvio app e verifica credenziali. Password errata al blocco, successivo sblocco corretto e ripresa di una riga di Terminale lasciata a metà, senza ricevere i tasti del login. Riavvio da menu e avvio a freddo richiedono di nuovo l’accesso. Il test usa esclusivamente credenziali inventate su dischi usa e getta.
- **Disco account:** lettura indipendente di entrambi i settori; il verificatore corrisponde a `hashlib.pbkdf2_hmac('sha256', …, 120000)`, salt non nullo, password della prova assente nei byte su disco e nelle trascrizioni. Un record di backup valido consente l’accesso se il primario è danneggiato. Due record non vuoti ma non validi non consentono il login né una nuova configurazione; dopo lo spegnimento il disco di prova è invariato. Separatamente, `tests/crypto.py` confronta SHA-256 su 9 lunghezze e PBKDF2 su 4 conteggi, compreso 120.000, con la libreria standard.
- **Finder:** navigazione da Posizioni, lettura integrale dell’ELF, del loader, della mappa e del sorgente; prima pagina, tasto Fine e ultima pagina dell’ELF. Gli hash sono prodotti nel guest, in Finder, leggendo i descrittori. La fonte di confronto è il **FAT incorporato nella entry El Torito del CD avviato**, estratto indipendentemente con `xorriso` + `mcopy`; le copie esterne nel filesystem ISO coincidono. Anche la copia dei sorgenti nel catalogo è stata confrontata col tree di build. Il Terminale ricalcola lo stesso hash ELF dopo tentativi rifiutati di scrittura, cancellazione, creazione e spostamento. Una copia troppo grande per TinyFS non lascia un file troncato.
- **Scrivania e nove app:** apertura insieme come processi separati, focus, trascinamento, resize, zoom, chiudi/contrai/ripristina, Dock, Cerca, Libreria e panoramica. Provati scrittura/lettura e dialoghi in Finder e TextEdit, promemoria reali del Calendario, calcolo `12.5*4−8=42`, monitor con dati del kernel, Galleria e quattro sfondi differenti, aspetto/vetro-opaco/movimento ridotto e relativo salvataggio. Minimize: **14 frame / 45 tick PIT**, ripristino: **11 / 38** in questa esecuzione. Una zona del pannello mostra 8 colori distinti in vetro e 1 in opaco.
- **Persistenza e live:** confronto offline dei byte dei documenti, note e preferenze. Un nuovo processo QEMU con lo stesso disco recupera i documenti. Due avvii solo-CD usano RAM e ricominciano senza account. Un vero disco creato da TinyOS 1.0 è stato provato su una copia: iscrizione dell’account, conservazione dei file utente e delle preferenze precedenti, aggiornamento dei manifest. Sette trascrizioni COM1 senza eccezioni inattese o panic. Non è un audit di sicurezza, una validazione per Mac fisici o una misura di equivalenza grafica pixel per pixel con macOS.

## Oggetti effettivamente letti

| File nel guest | Byte | SHA-256 dei byte integrali |
|---|---:|---|
| `/System/Kernel/kernel.elf` | 7.492.024 | `29e8f8c631dd71b1c13e631fb5c751cdbbe24aca44014aa0168f624950d5328c` |
| `/System/Kernel/Avvio/BOOTX64.EFI` | 196.772 | `b89a1c7959b74d38a953b506417a189f34815cbc52ffea2f5d51850e6540d4fc` |
| `/System/Kernel/kernel.map` | 32.509 | `30955dcfc88809a2e08f09bffa6789d46a5ca1dabc626855463307975c3816d3` |
| `/System/Kernel/Sorgenti/kernel/main.c` | 754 | `83e30c7b7c70b4ef6068cf738875cc0e195ac1a6e14e4a0b2c9fa352bdd97c81` |

## Ripetere e vedere la prova

```sh
./build.sh
python3 tests/crypto.py
python3 tests/integration.py --evidence /tmp/aurora-evidence
# Opzionale: dipendenze HOST aggiuntive OpenCV (VP8), NumPy, Pillow
python3 tests/record_demo.py /tmp/avvio-login-finder.webm
```

Configurazione di riferimento: Linux Debian amd64, GCC 14.2, binutils 2.44, QEMU 10.0.13 in TCG, OVMF 2025.02, `-machine pc -cpu max -smp 1 -m 384 -vga std -nic none`, tastiera/mouse PS/2. Le prove impostano l’RTC della VM su 18 novembre 2026 per risultati di calendario ripetibili. `tests/evidence/results.txt` contiene l’elenco completo, `measurements.json` le misure; la cartella di lavoro include anche catture intere e log seriali. Nello ZIP dei sorgenti sono inclusi i due risultati testuali; la suite può rigenerare tutto il resto.

**Video consegnato:** `consegna/Aurora-avvio-login-Finder.webm`, 1024×640, 272 frame VP8 decodificati, 13,60 secondi a 20 fps da 268 campioni temporizzati. Si vede un **secondo avvio** su un disco di prova già configurato, password mascherata, apertura di Finder, intestazione e fine dell’ELF e ritorno al blocco schermo. SHA-256 del video: `82e139475bcc790af1ae8c893a48b846800a53da87688b918b2fbccf45081fb4`.

Anche la console host facoltativa è stata provata da Chromium locale: connessione noVNC, tastiera del browser, creazione account in una VM isolata e apertura dell’ELF. L’area dell’ispettore catturata dal canvas remoto coincide pixel per pixel con la stessa area della cattura QMP (confronto RGB su 429×316 px). La chiusura del client elimina il guest e lo stato temporaneo.
