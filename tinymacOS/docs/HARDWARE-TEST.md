# Prova live su hardware reale — USB canonico sperimentale

**Si può tentare, ma non è ancora hardware validato.** La 10.1.7 / 30B8 con `TMC_USB_CANONICAL=1` è una build di sviluppo, non la release stabile né una build qualificata per l'installazione. Il collegamento HID-core/usbhid/hid-input/generic è attivo; non significa supporto HID universale. Il successo in QEMU non prova compatibilità ASRock Q1900M/J1900 o con altre schede reali.

## Quale immagine

L'esperimento verificato usa il supporto **ottico BIOS/UEFI**:

- nome: `tinymacOS-10.1.7-usbhid.iso`;
- dimensione: **226215936 byte**;
- SHA-256: `427463c2e0a9ab923aa5fce82799d53edaaa8795420cd007d0c34a1a16b24584`;
- staging locale: `/var/tmp/tmc-10.1.7/linux-usbhid-live/tinymacOS-10.1.7-usbhid.iso`;
- provenienza: `workspace/release-10.1.7-30B8/usbhid-validation.json`.

Questo percorso locale non è un link pubblico né un asset pubblicato. Il download «latest» continua a indicare 10.1.6 / 30B7. Una build ricompilata dopo modifiche può avere hash diverso e richiede proprie verifiche; non eredita i PASS dell'immagine sopra.

**Non è una ISO USB-DD ibrida.** Non scriverla con `dd` su una chiavetta aspettandoti un avvio supportato. Rufus, Ventoy, copia dei file su FAT e catene di boot alternative non sono stati qualificati. Per il primo tentativo usare un DVD masterizzato come **immagine**, non come semplice file, e un lettore ottico avviabile riconosciuto dal firmware. Anche un lettore ottico USB introduce una combinazione firmware/controller non ancora verificata.

## Prima di avviare

1. Usare un PC x86-64 non critico, con almeno 1 GiB di RAM come configurazione prudente e video compatibile con il percorso firmware del progetto.
2. Fare un backup indipendente e **scollegare fisicamente i dischi interni/esterni con dati da conservare**. Una sessione live non è una garanzia di sola lettura: il kernel dispone di codice storage e utility distruttive. Non collegare un disco di destinazione per questa prova.
3. Annotare le impostazioni firmware prima di cambiarle. Secure Boot deve essere disattivato; il percorso storage implementato è SATA/AHCI. Non modificare la modalità SATA di un sistema di lavoro per provare questa build.
4. I runtime controller attuali non sono un port IOMMU/DMA-remapping generale. Non assumere compatibilità con un IOMMU attivo e non disabilitare protezioni su un computer critico solo per questo test.
5. Collegare una tastiera USB Boot italiana e un mouse USB Boot semplici, preferibilmente cablati e direttamente alle porte della scheda madre. Iniziare senza hub, dock, ricevitore wireless, KVM o dispositivi compositi. Il tablet QEMU è verificato, ma tablet fisici/compositi arbitrari non lo sono. PS/2 non è il percorso da verificare.

## Sequenza breve e non distruttiva

1. Avviare esplicitamente il lettore ottico dal menu firmware; annotare se l'ingresso scelto è UEFI o BIOS/legacy. Se il supporto non compare, fermarsi: non inizializzare dischi per renderlo avviabile.
2. La prima prova può essere fatta senza toccare l'installer. Annotare l'ultimo passaggio visibile: firmware/OpenCore, mela, login o desktop. Un blocco può richiedere lo spegnimento manuale.
3. Nella live usare solo credenziali di prova, non una password personale riutilizzata. Verificare movimento, clic, digitazione italiana e F2 per aprire Terminale. `sw_vers` controlla l'identità, ma il solo numero 10.1.7 / 30B8 non distingue le diverse ISO sperimentali: conservare anche l'hash.
4. Solo se il sistema è stabile, provare scollegamento/ricollegamento del mouse. Il PASS QEMU su alcune topologie non garantisce hotplug generale su hardware.
5. Non aprire «Installa tinymacOS», «Inizializza», ripristino o comandi di scrittura su disco. Spegnere al termine della prova e ripristinare le impostazioni firmware annotate.

Recupero è un runtime UEFI separato e usa i driver input firmware: **mouse/tastiera funzionanti nel desktop non implicano input funzionante in Recovery**, o viceversa. Non usare questa build come unico mezzo di recupero di dati reali.

## Cosa riportare

- modello esatto scheda madre/PC, CPU, versione firmware e modalità di avvio;
- hash ISO, quantità RAM, Secure Boot e stato IOMMU se noti;
- tastiera/mouse e porte usate, eventuale hub o lettore USB;
- ultimo schermo raggiunto, input funzionante/non funzionante, eventuale foto;
- distinguere desktop, hotplug e Recovery; non trasformare un singolo avvio in una certificazione hardware generale.

I sei test già superati riguardano **QEMU** BIOS/UEFI con xHCI, EHCI e hub esterno. Restano limiti di polling CPU0, contesto input/IRQ del lavoro USB, soli control/interrupt-IN, rollback/reset da ampliare e assenza del completo usbhid. L'ultima release pubblicata resta **10.1.6 / 30B7**.
