# Architettura

## Avvio

Il supporto nativo avvia un loader UEFI; il confezionamento ottico aggiunge il percorso BIOS ISOLINUX/MEMDISK → OpenDuet/OpenCore → loader nativo. L'installazione prepara anche il ponte MBR per l'avvio BIOS del disco GPT, mantenendo il percorso UEFI.

Il loader verifica KernelCache e catalogo di sistema, prepara memoria e framebuffer ed esegue `ExitBootServices` prima del kernel. Il desktop non continua a usare i servizi UEFI. Il formato KernelCache è proprio del progetto, non quello Apple e non una firma Secure Boot.

## Kernel e processi

Il kernel gestisce paginazione x86-64, isolamento Ring 3, interrupt, scheduler e syscall native. La VM multi-CPU comprende coordinamento delle mutazioni e invalidazione TLB. La presenza delle primitive di thread non implica compatibilità POSIX/pthread completa.

Le applicazioni sono eseguibili statici nel catalogo di sistema, organizzati in bundle. Non vengono forniti dyld, Cocoa o ABI Darwin. Il compositor accede ai servizi grafici attraverso il kernel; font e immagini sono risorse effettive del filesystem.

## Dispositivi e input

AHCI serve lo storage SATA. I driver nativi comprendono PS/2, xHCI, EHCI e UHCI; routing Intel e topologie USB hanno limiti espliciti. La decodifica distingue delta relativi e coordinate assolute, verifica i report HID supportati e usa una mappa italiana ISO condivisa per tastiere USB e PS/2.

Nel desktop l'input passa dai driver kernel. In Recovery, che è un runtime UEFI separato, il puntatore usa i protocolli firmware disponibili o USB I/O sulle interfacce non reclamate. Questi percorsi non sono intercambiabili: un test del decoder o del controller nativo non certifica il firmware Recovery.

## Recupero e integrità

`tools/baseimage.py` crea il BaseSystem UDIF non compresso con il runtime di Recupero. I loader verificano il file prima di eseguirlo. L'installer copia lo stesso BaseSystem nella partizione Recovery; la sorgente sigillata del desktop serve invece alla reinstallazione offline.

I digest rilevano corruzione, non proteggono contro chi può riscrivere sia dati sia sigilli. La geometria corrente dei volumi è descritta in `TINYFS.md`; le vecchie dimensioni dei documenti storici non vanno applicate ai media attuali.

## Riferimenti nel codice

| Funzione | Sorgenti |
| --- | --- |
| Loader e bootstrap Recovery | `boot/` |
| Runtime UEFI di Recupero | `recovery/` |
| Processi, memoria e syscall | `kernel/`, `arch/`, `include/` |
| Controller e parser HID | `drivers/` |
| Cataloghi e volumi | `tools/system_volume.py`, `fs/`, `storage/` |
| Compositor e applicazioni | `desktop/`, `userspace/` |

I percorsi della tabella sono relativi alla directory `tinymacOS/`.
