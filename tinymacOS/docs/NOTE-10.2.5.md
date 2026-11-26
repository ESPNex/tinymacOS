# 10.2.5 Tramontana · 30B16

> **Stato attuale 30B19:** [rollback mouse, DnD disabilitato e riconoscimento GPT](NOTE-10.2.5-30B19.md).
> I risultati e le funzionalità sotto descrivono build precedenti, non la nuova accettazione.

> **Revisione 30B17:** questo documento descrive anche la precedente 30B16.
> Per font ripristinato, puntatore/drag, inventario Windows e validazione della
> nuova candidata vedere [HOTFIX-10.2.5-30B17.md](HOTFIX-10.2.5-30B17.md).

## Novità

- **Finder:** menu contestuali Taglia/Copia/Incolla e trascinamento tra finestre,
  su cartelle e Scrivania; copia ricorsiva e spostamenti reali, non simulazioni
  grafiche. Terminale con `cp -R`, `mv`, `pbcopy`/`pbpaste` e appunti condivisi;
  copia/incolla del testo con Ctrl+Maiusc+C/V, preservando Ctrl+C come interrupt.
- **FS2:** filesystem proprietario con blocchi immutabili da 16 KiB, indirizzi e
  dimensioni a 64 bit, directory/cataloghi dinamici e snapshot CoW condivisi.
  Usa l'extent dati effettivo, non il vecchio volume fisso da 8–32 MiB.
- **NVMe:** driver nativo anche dopo ExitBootServices; namespace 512 e **4Kn
  nativi**. Installazione su disco 4Kn vuoto, GPT nelle unità native, ESP e
  Recovery con BPB FAT da 4096 byte, cold boot UEFI e Recovery interna.
- **FileVault FV003:** installazione direttamente cifrata e conversione di un
  volume FS2 esistente sullo **stesso extent**, attivabile e disattivabile da
  Recovery senza perdere account, documenti o snapshot. Password e chiave di
  recupero indipendenti, autenticazione dei blocchi e journal di ripresa.
- Backup locale e ripristino CoW, importazione di checkpoint da un altro disco
  e reinizializzazione logica/cifrata da Recovery, con conferme esplicite.

Non sono state aggiunte modifiche alla visibilità/montaggio di EFI e Recovery.

## Conversione: prima una copia esterna

Aprire Terminale in Recovery e usare `fdesetup manage`. Salvare la chiave fuori
 dal disco e confermarne gli ultimi otto caratteri. Il volume deve essere offline
e avere una coda non referenziata sufficiente per la copia temporanea di tutti
gli ID di blocco conservati; non vengono eliminati snapshot o compattati gli ID.
Una preflight insufficiente o una credenziale errata non scrive dati.

La rilocazione, dopo la preparazione verificata, è **riprendibile, non rollback**.
Se la preparazione si interrompe prima dei journal, ripetere l’operazione sul
prefisso originale intatto. La deattivazione può già lasciare plaintext nella
coda durante la preparazione: non considerare il supporto interamente cifrato
dopo averla confermata. Durante la rilocazione, dopo un’interruzione riaprire
FileVault in Recovery e autenticarsi. Durante il processo il kernel impedisce
il mount; dopo la verifica della destinazione ripulisce logicamente la coda,
compresi i residui nei blocchi liberi, prima di dichiarare conclusa l'operazione.
L'attivazione riduce la capacità utile per i record autenticati; la disattivazione
la ripristina. Non è secure erase fisico né protezione dal replay di immagini
precedenti. [Dettagli](FS2-CONVERSION-4KN-10.2.5.md).

## Supporti distribuiti e avvio

- **Unica ISO distribuita:** `tinymacOS-10.2.5-x86_64.iso`, supporto ottico
  BIOS/OpenDuet/OpenCore **e UEFI**, senza dover scegliere tra due download.
  SHA-256: `1ce54378e933ca388f0fa1ba2471612422a826a2ec033722b29daab4bbfcb30a`.
  Il file è invariato: non è una nuova build e conserva le proprie prove di
  avvio BIOS e UEFI.
- La variante solo UEFI è stata rimossa dai download per evitare duplicazioni.
  Rimane documentata come artefatto storico della matrice completa
  installazione/Recovery/FS2/4Kn/cifratura. I 35 file comuni, inclusi EFI.IMG
  e payload di installazione/Recovery, sono stati estratti e confrontati byte
  per byte; le due immagini complete avevano hash distinti. Le ricevute
  originali restano storiche e non vengono riscritte come se i test fossero
  stati eseguiti su un unico hash ISO.
- Helper NVMe USB/SATA: necessario sui BIOS privi di accesso firmware NVMe;
  non contiene il sistema operativo e non modifica il firmware della macchina.

La ISO non è USB-DD ibrida. Per 4Kn è verificata l'installazione su disco vuoto;
resize/modifica di partizioni 4Kn esistenti e resize cifrato restano rifiutati.
Il disco di destinazione resta limitato dall'ABI driver a 1 TiB. FS2 non è Apple
APFS né Btrfs Linux. Nessuna certificazione hardware fisico/Windows, nessun
browser WebKit né compatibilità con applicazioni macOS.

## Validazione e build

Ricevuta: [gate finale e hash](RELEASE-READINESS-10.2.5.md). Gli asset includono
sorgenti corrispondenti, report di validazione e SHA256SUMS. Le prove interrotte o
fallite dei driver QA sono distinte dalle riesecuzioni accettate, senza attribuire
ai vecchi artefatti la validazione del nuovo.

Per ricostruire il backend distribuito: `export TMC_FS2_BACKEND=1` e
`export TMC_USB_CANONICAL=0` prima di `bash build.sh`; gli strumenti rimangono
esterni al checkout. Il percorso USB canonico sperimentale è un'opzione distinta.
