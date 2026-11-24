# Validazione

## Livelli di prova

I contratti host verificano parser e invarianti; le prove guest esercitano codice eseguito in QEMU; il collaudo fisico riguarda soltanto l'hardware realmente provato. Nessun livello sostituisce gli altri.

Ogni esito deve indicare sorgenti e media effettivi. Una compilazione riuscita, un controllo statico o il PASS di una vecchia ISO non certificano una nuova release.

## Candidata corrente: 10.2 / 30B11

Vedere [NOTE-10.2.md](NOTE-10.2.md) per correzioni UI e ricerca sul mouse Recovery. I contratti host `tests/recovery_usb.c` e `tests/recovery_firmware_pointer.c` esercitano il trasporto e il polling reali, non una riscrittura del comportamento. Prove guest e stato hardware sono distinti: nessuna certificazione della Q1900M/J1900 senza nuovo riscontro fisico. Le sezioni successive restano evidenze storiche.

## Milestone: HID-core/usbhid canonici

Il collegamento completo dei file HID originali è attivo. Vedere [USBHID-PORT.md](USBHID-PORT.md) e `usbhid-validation.json` per i nuovi hash e risultati. Control/interrupt-IN/interrupt-OUT, tablet assoluto e worker kernel sono successivi alle prove Boot qui sotto. Le vecchie ricevute restano immutate e non certificano la nuova ISO.

## Milestone storica: core USB/HCD/hub con classi Boot

La modalità opt-in `TMC_USB_CANONICAL=1` esegue xHCI/EHCI originali attraverso un confine opaco, core USB/HCD/hub originali e driver Boot originali. **Non** esegue ancora tutto HID-core/usbhid. PS/2 è escluso. L'ultima release pubblicata resta 10.1.6 / 30B7.

Evidenze in `workspace/release-10.1.7-30B8/`:

- `usbcore-kernel-integration-result.json`: artefatti, hash della fotografia sorgente, confine dei simboli, limiti e sei ricevute dello stesso ISO dualboot.
- `usbcore-dualboot-{bios,uefi}-{xhci-two-mice-kbd,ehci-kbd,xhci-hub-kbd}.json`: **6/6 PASS**. Tastiera solo USB con i8042 disabilitato, login/Terminale, cursore a pixel, URB attive allo scollegamento, ricollegamento sulla stessa porta e contatori originali; vecchio adattatore di classe inattivo.
- `usbcore-enumeration-model-result.json`: intero core/HCD/hub sul modello esplicitamente dichiarato, O1/O2 ASan/leak/UBSan, enumerazione/binding/completamenti/scollegamento/teardown. **Non** è prova hardware.
- `usbcore-canonical-host-result.json`: contratti ABI e binding/NO-I/O, distinti dall'enumerazione reale.

I test non certificano hardware fisico, installazione, Recupero firmware sul nuovo artefatto o una release. I sorgenti eseguibili e i media devono corrispondere alle ricevute. Le modifiche solo documentali successive vengono riportate separatamente senza riscrivere la fotografia sorgente storica.

[Guida alla prova live su hardware](HARDWARE-TEST.md). Le sezioni successive descrivono milestone storiche: affermazioni come «HCD nativo» si riferiscono alle rispettive ricevute, non al percorso canonico appena descritto.

## 10.1.7 / 30B8 in sviluppo

Le nuove prove sono in `workspace/release-10.1.7-30B8/`, dalla radice del repository:

- `deletion-contract-result.json`: codice reale di rimozione e parser comandi, ASan/UBSan, protezioni e guasti I/O simulati.
- `media-contract-result.json`: file OpenCore effettivamente estratti dai media e configurazioni verbose.
- `staging-uefi-result.json`, `staging-bios-result.json`: avvio quiet, identità, cancellazione CLI/Finder e Recovery; leggere sempre `passed` e i singoli controlli.
- `pointer-scale-contract-result.json`: conversione reale counts/mm del firmware con residui firmati e 100.000 confronti al modello indipendente, ASan/UBSan.
- `recovery-firmware-*-result.json`: movimento XY e clic osservati sul framebuffer tramite i protocolli firmware. In quella milestone il trasporto USB diretto non era collegato; nelle release successive è tornato sulle sole interfacce libere.
- `identity-contract-result.json`: metadati ottici/installati 10.1.7 / 30B8, anche nella finestra Info; layout About e provenienza storica delle icone invariati.
- `reproducible-build-result.json`: due build indipendenti della ISO di sviluppo, identiche byte per byte.
- `linux-usbmouse-contract-result.json`: driver Boot mouse upstream Linux v6.12 byte-esatto, shim implementato, ASan/UBSan, guasti di allocazione e 4.000 cicli concorrenti probe/report/disconnect.
- `linux-usbmouse-{uefi,bios}-*.json`: dodici topologie guest su xHCI, EHCI, UHCI, companion, hub e hub annidato. Il kernel reale della ISO deve coincidere con quello compilato; contatori vivi del driver e cursore a schermo sono verificati indipendentemente. Il trasporto HCD rimane nativo.
- `linux-posix-feasibility.json`: misura iniziale dei sette sorgenti Linux core/HCD contro il solo shim POSIX preesistente, **non** un port HCD riuscito. Non include il successivo port `usbmouse`.

Queste prove storiche non costituiscono il gate finale della 10.1.7; per il successivo collegamento controller/core/hub vedere la milestone canonica sopra. Il completo HID-core/usbhid e i gate finali restano da completare. La tastiera Recovery usa ora OpenUsbKbDxe e una variante dichiarata di Ps2KeyboardDxe, senza trasporto nativo: quattro profili firmware/tastiera verificano la mappatura italiana. I precedenti tentativi con regressioni restano archiviati, non promossi a PASS. La matrice completa di installazione/input va rieseguita sul risultato definitivo. Non creare ricevute di pubblicazione prima della pubblicazione effettiva.

## Ultima release pubblicata

La 10.1.6 / 30B7 resta documentata in `workspace/release-10.1.6-30B7/`. Il suo gate di 82 ricevute riguarda esclusivamente quella ISO, non la 10.1.7. Include anche i tentativi intermittenti conservati e descritti nelle sue note QA.

`final-qa.json` e `github-release-verification.json` distinguono collaudo e pubblicazione. Non riscrivere i JSON storici per adattarli al codice nuovo.

## Contratti ripetibili

Dalla directory di sistema:

```sh
bash tests/test-input-maintenance.sh
bash tests/test-legacy-input.sh
```

Le prove guest richiedono toolchain, build esterna e dischi virtuali usa-e-getta. Leggere i parametri di ciascun harness. Il backend sperimentale ha test separati sotto `workspace/webkit-tmc/tools/`: non certificano Safari/WebKit.

La variante PS/2 è identificata separatamente da OpenCore ufficiale; sorgenti, patch e lock sono in `vendor/tmc-ps2-keymap/`. `firmware-keyboard-contract-result.json` copre queue/overflow e il consumer reale con ASan/UBSan, deduplicazione, Shift/AltGr, Caps Lock, repeat/rilascio e assenza di istruzioni I/O nell’oggetto Recovery. I quattro `recovery-keyboard-*.json` restano la prova guest indipendente.

### Ulteriore milestone 10.1.7: tastiera Boot Linux

`linux-usbkbd-contract-result.json` esegue il driver originale completo e il vero consumer italiano con ASan/UBSan, inclusi LED differiti e annullamento/disconnessione durante EP0. Le sei ricevute `linux-usbkbd-{uefi,bios}-{ehci-kbd,companion-kbd,xhci-hub-kbd}.json` verificano il KernelCache della ISO, tastiera solo USB, testo italiano renderizzato e contatori IRQ/LED reali. Restano trasporti HCD nativi: non è il port completo Linux e non prova compatibilità fisica Bay Trail.

### Core URB originale, confine del backend

`linux-urb-contract-result.json` esegue tutto `usb/core/urb.c` di Linux 6.12 (GPL-2.0-only), senza sostituirne le funzioni: lifecycle, validazione, cancellazione, riferimenti, anchor, timeout e wakeup concorrenti. I 18 profili mouse/tastiera attuali verificano anche contatori submit/giveback reali. Il backend controller resta nativo e limitato: **non** certifica HCD Linux, core USB completo, generic HID o hardware fisico. La release resta bloccata.
