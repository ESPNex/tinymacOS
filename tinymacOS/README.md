# Sviluppare tinymacOS

Guida alla compilazione dell'albero di sistema. Presentazione, stato hardware e download sono nel README alla radice; i cambiamenti di ciascuna versione sono nelle note di rilascio.

## Prerequisiti

Base Debian 13 amd64 compatibile, GCC, binutils, Python 3 con Pillow e `dpkg-deb`. Gli strumenti aggiuntivi sono fissati in `tools/debian-amd64.lock.json`; `local-tools.py` li verifica con SHA-256 ed estrae in un prefisso utente senza installazione di sistema. Il provisioning richiede rete.

## Compilazione

Dalla directory `tinymacOS/`:

```sh
python3 tools/local-tools.py /var/tmp/tmc/tools/root
export TOOLROOT=/var/tmp/tmc/tools/root
export TMC_BUILD=/var/tmp/tmc-current
bash build.sh
```

La build produce `tinymacOS-10.1.7-x86_64.iso` nel percorso esterno indicato. Questo primo supporto usa l'avvio UEFI nativo. Per aggiungere l'avvio ottico BIOS/OpenDuet/OpenCore, usare un output distinto:

```sh
mkdir -p /var/tmp/tmc-optical
python3 tools/dualboot-iso.py \
  "$TMC_BUILD/tinymacOS-10.1.7-x86_64.iso" \
  /var/tmp/tmc-optical/tinymacOS-10.1.7-x86_64.iso
```

Toolchain, immagini disco e output non devono finire nel checkout. Il font e gli asset richiesti sono inclusi; `tools/control-svg.py` compila gli SVG in maschere alpha deterministiche, senza un interprete SVG a runtime.

## Avvio QEMU

Dalla stessa shell, una sessione UEFI live senza disco di destinazione:

```sh
STATE=/var/tmp/tmc-vm LIVE=1 bash run.sh
```

Il launcher usa Q35/AHCI, 2 vCPU, 1 GiB RAM, OVMF, tastiera USB e tablet xHCI. `TMC_POINTER=mouse` seleziona invece il mouse relativo. Il prefisso degli strumenti supporta l'avvio headless; per un display interattivo occorre un frontend QEMU disponibile sull'host, oppure aggiungere `-display vnc=127.0.0.1:0` e usare un client VNC locale.

Per utilizzare il supporto BIOS/UEFI appena generato, impostare `ISO=/var/tmp/tmc-optical/tinymacOS-10.1.7-x86_64.iso`. Il launcher `tools/run-bios-qemu.sh` seleziona il percorso BIOS. Per installazione e Recupero seguire `docs/QUICKSTART.txt`; non usare dischi fisici.

## Organizzazione tecnica

- `boot/`, `recovery/`: loader e runtime UEFI di Recupero.
- `arch/`, `kernel/`, `drivers/`: architettura x86-64, kernel e dispositivi.
- `desktop/`, `userspace/`: compositor, login, applicazioni e comandi.
- `fs/`, `storage/`: namespace, volumi e manutenzione disco.
- `assets/`, `licenses/`, `vendor/`: risorse, licenze e componenti esterni fissati.
- `tests/`, `tools/`: contratti, prove guest, build e confezionamento.

Gli overlay e gli header ABI sotto `workspace/webkit-tmc/` sono copie d'integrazione: non eliminarli come duplicati e non sincronizzarli alla cieca sopra il codice autorevole. Le directory `sources/` delle vecchie manutenzioni sono snapshot, non un secondo sistema da compilare.

[Architettura](docs/ARCHITECTURE.md) · [Dischi e TinyFS](docs/TINYFS.md) · [Validazione](docs/VALIDATION.md)

Le licenze sono specifiche dei componenti: vedere `licenses/` e i file vendor. I sorgenti corrispondenti Syslinux, con patch Debian, sono nel branch `vendor/syslinux-6.04` della stessa repository.

## USB Linux: percorso predefinito e percorso canonico sperimentale

I controller UHCI/EHCI/xHCI utilizzano ora sorgenti originali Linux v6.12, con runtime privati e DMA/polling implementati. La build predefinita mantiene il confine di enumerazione/classe `UsbInput`; non equivale al port completo di USB core/hub/usbhid.

Per compilare il percorso canonico **xHCI/EHCI + core/HCD/hub Linux + Boot mouse/tastiera**, dalla directory `tinymacOS/` e con i prerequisiti già predisposti:

```sh
export TMC_BUILD=/var/tmp/tmc-usbcore-test/build
TMC_USB_CANONICAL=1 bash build.sh
```

La selezione avviene in compilazione: non basta cambiare una preferenza su una ISO precedente. I simboli dei servizi canonici sono isolati e un'allowlist impedisce collegamenti involontari agli ABI privati. Il vecchio percorso di classe non viene avviato in questa modalità; PS/2 non è attivato dal percorso canonico.

Questo esperimento **non collega ancora il completo HID-core/usbhid**. Sono attivi i driver Boot originali `usbkbd`/`usbmouse`, non tablet/generic HID universale. Usare `tools/run-input-qa.sh` con `TMC_TEST_INPUT=xhci-two-mice-kbd`, `ehci-kbd` o `xhci-hub-kbd`, non il tablet del launcher predefinito.

Verifiche ripetibili:

```sh
bash tests/test-linux-usbcore.sh
bash tests/test-linux-usbcore-enumeration.sh
# Richiede una build/media esterni; non usa dischi fisici:
TMC_CANONICAL_STAGE=/var/tmp/tmc-usbcore-test \
  python3 tests/linux_usbcore_guest.py uefi xhci-two-mice-kbd
```

La matrice salvata copre 6 casi sullo stesso ISO dualboot: BIOS/UEFI × xHCI con due mouse e tastiera, EHCI con mouse/tastiera, xHCI con hub esterno e mouse/tastiera. Ogni caso verifica input renderizzato, KernelCache del supporto, contatori dei controller e hot unplug/replug. I test host O1/O2 ASan/UBSan restano distinti dalla prova guest.

Limiti: CPU0 serializzata a polling; lavoro USB che può attendere ancora drenato nel contesto input/IRQ; trasporto control/interrupt-IN, senza bulk/SG/iso/interrupt-OUT; rollback/reset e contesto scheduler da completare; nessun collaudo fisico, IOMMU, IRQ hardware o PM di sistema certificato. L'hook d'ingestione entropia non dispone ancora di un pool kernel collegato e non accredita entropia.

[Prova live su hardware reale](docs/HARDWARE-TEST.md) · [Evidenze e limiti](docs/VALIDATION.md). La 10.1.6 / 30B7 resta l'ultima release pubblicata; questo è un commit di sviluppo, non una release.
