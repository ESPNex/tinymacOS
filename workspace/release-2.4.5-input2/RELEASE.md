# tinymacOS 2.4.5 Levante — PS/2 e USB 2.0

**Build 21B7 · revisione input 2 · stessa release v2.4.5, ISO sostitutiva**

Questa revisione è stata richiesta dopo la prova fisica negativa della 21B6: tastiera PS/2 e mouse USB funzionavano in Recovery ma non al login. Il firmware e il kernel usano driver distinti. I precedenti test con tastiera USB/xHCI non coprivano quella configurazione.

## Implementato

### Tastiera e mouse PS/2, nativi i8042

- Inizializzazione separata delle due porte, test di interfaccia e attese limitate. Nessuna chiamata BIOS/UEFI per l’input del desktop.
- Tastiera Set 2 con traduzione i8042 a Set 1: make/break, modificatori sinistri/destri, Caps Lock, Num Lock, tastierino, tasti funzione e navigazione estesa. I frammenti PrintScreen/Pause non lasciano Shift o Ctrl premuti. Nessuna confusione fra il byte `0xAA` di rilascio Shift e un messaggio di reset.
- Aggiornamento LED con macchina a stati ACK/RESEND non bloccante e timeout; il timer non resta in attesa di un ACK LED.
- Mouse standard a tre byte, segno a nove bit, inversione Y, tre pulsanti e scarto overflow. Negoziazione IntelliMouse per la rotella a quattro byte.
- Un solo lettore del controller sul tick CPU0, IRQ tastiera/mouse disabilitati nell’i8042 per evitare consumatori concorrenti. Il driver non dipende dall’emulazione USB Legacy/SMM.

### EHCI USB 2.0 e companion UHCI, per input HID

- Handoff BIOS/OS, arresto/reset controllato del controller, DMA persistente e allineato sotto 4 GiB, configurazione delle porte radice.
- EHCI: controllo asincrono, interrupt HID periodici su albero di frame, indirizzamento/enumerazione e configurazione di mouse e tastiere, decoder Boot o Report validato, recupero limitato degli endpoint in halt.
- UHCI: controller companion per i dispositivi low/full-speed che EHCI non gestisce direttamente sulle porte radice; controllo e interrupt periodici nativi, gestione toggle e pacchetti corti. Copre anche controller UHCI standalone.
- I mouse preferiscono Report quando il descriptor è valido e il protocollo viene accettato, per conservare rotella e report-ID; rimane il fallback Boot. Nessun decoder scelto alla cieca.
- EHCI contiene anche il percorso per enumerare hub USB2 e impostare i campi transaction translator per figli low/full-speed. Questo percorso non viene presentato come verificato su hub USB2 reale: QEMU in uso non fornisce quel modello di hub.
- xHCI e il supporto preesistente restano presenti. xHCI gestiva già anche le velocità USB 1.x/2.0; non era sufficiente aggiungere una semplice opzione “USB2”.

### Desktop e identità

- **Eliminati pannello, pulsante e scorciatoia della diagnostica F11 al login.** F11 dopo l’accesso resta il comando preesistente per aprire l’installer nella live, non la diagnostica rimossa.
- Corretto un secondo difetto: gli eventi `EV_SCROLL` separati dei mouse relativi arrivavano dal driver ma non venivano inoltrati dal compositor alle app. Ora rotella relativa e rotella dei puntatori assoluti seguono lo stesso instradamento sulla finestra sotto il cursore.
- Conservato il cursore con danno limitato al login, senza ridisegno completo per ogni movimento.
- Versione prodotto ancora **2.4.5**, build **21B7** in app, metadati, comandi e Recovery. Nessun Safari/WebKit funzionante incluso.

## Limiti: cosa NON significa “completo”

Il supporto aggiunto riguarda **l’input HID** richiesto, non un driver EHCI general-purpose certificato per ogni classe USB. Trasferimenti bulk/isochronous per dischi, audio, webcam ecc. non sono implementati da questo nuovo stack. **OHCI non è implementato**: un PC con companion OHCI, anziché UHCI, può ancora richiedere un ulteriore driver.

L’enumerazione avviene a freddo: collegare i dispositivi prima di avviare. Hotplug, rimozione/riconnessione e tutti i layout HID non sono coperti. Capacità limitate: fino a quattro EHCI, otto UHCI, otto dispositivi per controller e quattro interfacce HID per dispositivo; parsing Report limitato ai layout supportati. Il percorso EHCI hub/TT resta non validato su hardware. I tasti multimediali e tutti i protocolli proprietari PS/2 non sono una funzionalità dichiarata.

**Non è ancora una conferma sul PC fisico dell’utente.** La verifica automatica usa controller e periferiche QEMU reali per il guest, non trasporti host sostitutivi, ma non prova compatibilità universale. I contratti host ASan/UBSan sono prove aggiuntive distinte.

Restano i limiti del supporto di avvio: ISO ottica BIOS/OpenDuet/OpenCore e UEFI, non USB-DD ibrida; Secure Boot non supportato. La selezione con R del Recovery ottico BIOS non è dichiarata risolta da questa patch. Il ripristino locale da disco ha una prova separata.

## Download sostitutivo

- Release: https://github.com/ESPNex/tinymacOS/releases/tag/v2.4.5
- File: `tinymacOS-2.4.5-OpenCore-Installer-x86_64.iso`
- Build da controllare: **21B7**, non 21B6.
- Dimensione: **221509632 byte**
- SHA-256: `2d164de30042f026f107d573d485398e1930a96b692de11a9934976abd62e17a`
- Checksum: `SHA256SUMS.txt`

Il nome del file e la pagina restano gli stessi su richiesta dell’utente. Riscaricare la ISO: una copia precedente non viene aggiornata automaticamente. La sostituzione è considerata completata soltanto con il receipt `github-release-verification.json`, dopo un nuovo download e controllo SHA-256.

## Riproduzione

Dalla directory `tinymacOS`, con la toolchain ripristinata:

```sh
TOOLROOT=/var/tmp/tmc/tools/root TMC_BUILD=/var/tmp/tmc245-input2-final bash build.sh
TOOLROOT=/var/tmp/tmc/tools/root python3 tools/dualboot-iso.py \
 /var/tmp/tmc245-input2-final/tinymacOS-2.4.5-x86_64.iso \
 /var/tmp/tmc245-input2-final/tinymacOS-2.4.5-OpenCore-Installer-x86_64.iso
```

L’output della conversione deve essere un file nuovo. Dischi e build QA restano file esterni usa-e-getta, mai dispositivi fisici. `run-media-qa.sh` esegue i profili input e l’installer; le regressioni kernel sono eseguite separatamente da `../webkit-tmc/tools/run-tests.py --guest`. Il gate `collect_qa.py` rifiuta risultati mancanti, falliti o riferiti a una ISO diversa.

## Collaudo del candidato finale

**16 profili nativi PASS, 117 verifiche input; regressioni kernel/runtime 29/29 PASS.** Tutti i receipt di questa revisione fanno riferimento alla ISO con SHA-256 indicato sopra.

| Area | Esito |
|---|---|
| Live UEFI: PS/2 tastiera + mouse, PS/2 + EHCI 480 Mb/s, PS/2 + companion 12 Mb/s, UHCI standalone, tastiera USB su EHCI/companion, regressione xHCI | 7 profili PASS |
| Live BIOS: PS/2 tastiera + mouse, PS/2 + EHCI, PS/2 + companion | 3 profili PASS |
| Installato senza ISO: PS/2, EHCI e companion, ciascuno su BIOS e UEFI | 6 profili PASS |
| Per profilo: sprite al login, digitazione password e clic accesso, trascinamento esatto/rilascio, clic destro, comando Terminale e build; rotella dove dichiarata | PASS |
| Diagnostica F11 assente dall’eseguibile desktop | PASS |
| Installer grafico BIOS, ISO rimossa, riavvio BIOS e stesso disco avviato UEFI | PASS |
| Kernel danneggiato su clone: Recovery locale, ripristino sigillato, documenti e ponte OpenDuet invariati, nuovo avvio | PASS |
| Identità 2.4.5/21B7: volumi, bundle, app disegnate, comandi, documento iniziale e Recovery | PASS |
| PS/2 e HID USB2: contratti host ASan/UBSan, fallback/short descriptor e frammenti tastiera | PASS, distinti dai test controller nativi |
| Kernel/runtime: rete, VM, sync, errno/TLS, thread/TSD e C++ freestanding, inclusi profili 1/4/8 CPU | 29/29 PASS |
| Kernel ELF di produzione confrontato con i cinque build probe | Identico |
| ISO ricostruita indipendentemente | Identica byte per byte |

La matrice registra la topologia QEMU, inclusa l’assenza di qualunque tastiera USB nei profili PS/2. I test EHCI non usano un mouse xHCI come sostituto. RAM test: 512 MiB, al massimo due guest in parallelo; il primo batch sovraccarico è stato interrotto e non classificato PASS. Dettagli in `harness-history.md`.

Receipt: `final-qa.json`, `input-*.json`, `installed-*.json`, `kernel-regression-result.json`, `kernel-production-match.json` e risultati installer/Recovery. Screenshot originali selezionati in `evidence/`, senza ridisegno o resize. I probe kernel usano volumi comando diagnostici separati: non sono la ISO sostitutiva distribuita e non validano un browser.
