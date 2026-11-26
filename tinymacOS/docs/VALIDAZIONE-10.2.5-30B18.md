# tinymacOS 10.2.5

**Tramontana · build 30B18 · rebuild completo con font UI e Mono forniti dall'utente.**
La versione resta 10.2.5. Una sola ISO x86_64 BIOS+UEFI, nella release esistente.
Safari completo non è incluso.

## Font nuovi, nessun fallback DejaVu

- UI: Regular 400, Medium 500, Semibold 600 e Bold 700 da `tinymacosfonts.zip`.
- Terminale: vero `tinymacOSMonoRegular.otf` del secondo pacchetto, 13 px,
  altezza 21 px, celle e avanzamento fissi da 8 px.
- Panic: stesso Mono Regular a 12 px, bitmap ASCII 8×15, raddoppio a video
  invariato nel kernel. Verificato il bitmap esatto nei tre kernel estratti.
- Nessuna famiglia di fallback, DejaVu o faccia bitmap ereditata. Command,
  ricerca e backspace usano disegni geometrici originali. Soft hyphen usa il
  trattino della stessa famiglia; nel Mono NBSP usa lo spazio e gli apici
  numerici 1/2/3 sono cifre ridotte della stessa famiglia. Altre mancanze
  bloccano la conversione: nessun ripiego sul font installato nell'host.
- Rimossi generatori obsoleti; rigenerare gli artwork non sovrascrive più il
  font. Il lock del packager rifiuta anche il precedente asset 30B17.

TFN2: 2.178.265 byte, 11 facce, 207 glifi per faccia. SHA-256:
`24add187754c57f6d8f160fe0a2608a9b946979713eec2b92dc653cb4daf97a6`.

Il pacchetto OTF fornito non include una licenza esplicita. I metadati
`tinymacOS` non dimostrano autore, derivazione da Inter o diritti di
redistribuzione. La provenienza e questa limitazione sono documentate in
`licenses/FONTS.txt`; nessuna licenza è stata inventata.

## Verifiche fresche della 30B18

| Gruppo | Esito |
|---|---:|
| Regressioni OS host, batch interamente seriale | 13/13 |
| Runtime/backend host | 18/18 |
| Riproduzione TFN2 e panic con allowlist di cinque soli OTF | PASS |
| Lettore metriche C effettivo, normale + ASan/UBSan | PASS |
| Rifiuto dell'override con il precedente font 30B17 | PASS |
| Suite guest della nuova ISO e fixture installate da essa | 14/14 |
| Audit indipendente dei media finali | PASS |

Le 14 suite guest comprendono: avvio ottico UEFI e BIOS; mouse USB relativo
UEFI e BIOS; installazione GUI/avvio a freddo/Recovery interna; trasferimenti
reali Finder e Terminale con `usb-mouse`; layout Windows sintetico con gap
sano e GPT discordante; installazione 4Kn; conversione/deattivazione cifratura
in-place 4Kn e 512 byte; helper NVMe con firmware privo di NVMe; importazione
esterna; installazione/reset cifrati; backup/ripristino UI; reset da Recovery.

L'audit controlla il TFN2 esatto nei cataloghi live/installato/Recovery,
versione 30B18, KernelCache live/installato comune e il bitmap panic esatto
nei kernel live/installato/Recovery. I tre cataloghi della ISO dual finale
corrispondono byte per byte alla build ottica nativa appena ricostruita.
Le prove della 30B17 restano storiche: non vengono riciclate per questi esiti.

### Tentativi iniziali e ripetizioni

Il primo smoke ottico ha completato UEFI; il launcher BIOS ha poi rifiutato
la configurazione SATA prima di avviare QEMU perché il runner gli passava
anche l'helper NVMe. Corretto soltanto l'ambiente QA, limitando l'helper al
suo test NVMe; ripetuta l'intera suite ottica, con esito positivo BIOS e UEFI.
Log e ricevuta originali sono conservati insieme alla ripetizione. Non sono
stati modificati la ISO o gli assert del sistema operativo per farla passare.
Il primo build aveva omesso l'opzione FS2: il test GUI ha rilevato il vecchio
filesystem nel superblock e ha bloccato la pubblicazione. Candidata scartata,
hash e prove conservati. Corretto `build.sh` per imporre FS2 in produzione,
ricostruita interamente la ISO e ripetuti tutti i 14 test guest sulla nuova
candidata; nessun risultato del build rifiutato è contato come accettazione.
Con il nuovo Mono il vecchio decoder pixel della chiave di recupero non ha
riconosciuto due righe complete e ha fermato il test 4Kn. Modificato solo il
lettore QA: 64 celle fisse, soglia di correlazione almeno 0,98 e margine di
univocità almeno 0,005, senza leggere memoria guest o registrare segreti.
Regressione sintetica uppercase/lowercase, contrasto chiaro/scuro e cella
corrotta rifiutata: PASS. La conversione 4Kn riparte dalla preparazione non
confermata della stessa fixture, con i successivi controlli di credenziali,
hash, riavvio e deattivazione mantenuti. Tentativo e replay sono distinti.
Non è un unico lancio senza tentativi: gli esiti sono chiusure documentate.

## Funzioni conservate e limiti

Restano le correzioni mouse/eventi/drag-and-drop e inventario dei veri gap
su GPT protette della 30B17. MSR, NTFS e RAW non vengono considerati liberi.
Una GPT non sicura resta protetta dalle scritture; nessuna riparazione forzata.
Conservate le basi runtime 10.3 verificate, non un browser incompleto.

**Nessuna certificazione del mouse fisico segnalato o dual boot Windows reale.**
Le prove sono QEMU e layout sintetici. Il contenuto del bitmap panic è verificato
nei binari e mostrato nelle anteprime asset, ma non si dichiara qui un test di
kernel panic provocato sul PC fisico. Supporto ottico, non ISO USB-DD ibrida.
ABI disco fino a 1 TiB; modifica tabelle su 4Kn già popolato e resize cifrato
non supportati. Nessuna prova di cancellazione fisica sicura.

## Integrità e sorgenti

`tinymacOS-10.2.5-x86_64.iso` — **329.510.912 byte**

SHA-256: `65e34370c7dca1bd43db3ed659c2a5e717778874fdbfbaa87f16670b9f3bda9a`

`SHA256SUMS` copre ISO, sorgenti, helper, questo documento e archivio delle prove.
L'helper NVMe resta byte-identico alla revisione precedente, senza payload OS.
`VERIFICA-FINALE-10.2.5.tar.gz` contiene manifest per file, log originali/ripetuti,
ricevute e schermate non alterate. Nei sorgenti:
`workspace/local-10.2.5/hotfix/verification-30B18.json` e
`tinymacOS/docs/NOTE-10.2.5-30B18.md`.
