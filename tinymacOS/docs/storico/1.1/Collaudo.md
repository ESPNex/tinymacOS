# tinymacOS 1.1 · Prisma — verbale di collaudo

**Immagine verificata:** `tinymacOS-1.1-x86_64.iso`, **22.939.648 byte** (21,88 MiB).  
**SHA-256:** `5e334ff3fdc91a449807b5446995da8f74cf13da54938f3850a078f1481c66e0`  
**ISO pubblica:** <https://drive.google.com/uc?id=1NQbwZfDWBPF1qfyAmUgyWU_zZEhF2yYc&export=download>

**36 controlli di accettazione superati** sulla copia della release, non su un prototipo precedente. Il solo codice del sistema fornito alle VM veniva dall'ISO: il disco IDE conteneva soltanto dati. QEMU 10.0.13, OVMF 2025.02, PC/i440FX, TCG senza KVM, 1 vCPU, 384 MiB di RAM configurata, VGA standard e PS/2. Le prove usano la vera uscita GOP a 1280×800, input emulato via QMP e messaggi COM1. L'RTC virtuale del test parte dal 18 novembre 2026 alle 10:30 per rendere verificabile il passaggio di mese. Nessuno screenshot fa parte delle texture della UI.

| Cosa è stato verificato | Osservazione |
|---|---|
| Boot e memoria | El Torito UEFI; `BOOTX64.EFI` PE32+ estratto dalla stessa ISO; ingresso dopo `ExitBootServices` e `LMA=1`. GDT/IDT/TSS del kernel, I/O APIC + LAPIC, IRQ PIT e thread RTC. Il probe alloca, tocca e libera due pagine. |
| Separazione dei processi | `MOV CS` in userspace, chiamata SYSCALL e ritorno a CPL 3, selettore salvato sull'interrupt timer `0x23`. CR3 distinti, secondo thread utente e rifiuto di un buffer di uscita supervisor. È un controllo funzionale, **non** un audit di sicurezza. |
| Finestre e composizione | Mouse PS/2: ingrandimento del Dock e lancio di Terminale. Finder da `(150,174,724,421)` a `(194,200,724,421)` dopo il trascinamento, poi a `766×445`. Zoom e ripristino esatto. Apertura, chiusura, contrazione e ripristino registrati. |
| Leggibilità e materiali | Nella stessa regione senza testo del Centro di Controllo: 9 colori con vetro e un solo colore con materiale opaco. Screenshot in entrambi i temi e verifica visiva di scrivania, Cerca, anteprima, app e pannelli. Il confronto controlla il risultato a schermo; la costruzione ottica è in `desktop/glass.c`. |
| Quattro app aggiunte | Calcolatrice lanciata dal manifest di Finder: `12.5*4-8=42`, errore per divisione per zero e recupero. Calendario: cambio novembre → dicembre e nota del giorno 18. Monitor: campionamento tick/processi, pausa/ripresa e ordinamento. Galleria: tre sfondi distinti verificati in pixel **fuori** dalla finestra. |
| Scrivania e sistema | Libreria app, ricerca su Documenti, panoramica e menu di sistema. Tutte e nove le app presenti insieme nell'output `ps`. In movimento ridotto l'apertura di Info è terminata in un frame. Le cinque preferenze ricaricate sono `[1,2,1,1,1]`: scuro, Laguna, Dock a sinistra, ridotto, opaco. |
| File condivisi | Terminale: `mkdir/write/cp/mv/cat/rm`. Finder crea cartella e file, copia, rinomina ed elimina. `Documents/segnale.txt`, creato dal Terminale, compare nella vista rapida e si apre in TextEdit. TextEdit salva `Documents/prisma.txt` con **`Da TextEdit, su disco.`**. |
| Persistenza | La nota **`Verifica di Prisma`** è un normale `Documents/Agenda/2026-12-18.txt`. Terminale rilegge sia la nota sia il documento. Riavvio dal menu, ACPI S5, poi nuovo processo QEMU con lo stesso disco: stessi byte. Un parser host indipendente verifica file, directory e preferenze a VM spenta. |
| Live e aggiornamento | Senza disco IDE la sessione dichiara RAM live e non trova il documento del disco. Una copia del disco-fixture creato dalla release **TinyOS 1.0 reale** conserva tutti i file dell'utente, legge le preferenze `[1,1,1]`, aggiorna a nove manifest e salva il nuovo record dopo una modifica. |
| Riproducibilità e distribuzione | Una build in una directory pulita sul toolchain bloccato dà la stessa ISO (`cmp`). Anche il download pubblico anonimo (303 → 200) ha la stessa lunghezza e lo stesso SHA-256; `cmp` identico. Nessuna eccezione CPU imprevista o kernel panic nelle quattro trascrizioni di accettazione. |

### Misure, non promesse di FPS

- Contrazione di Finder: **14 frame del compositore in 47 tick PIT**; ripristino: **11 in 39 tick** (PIT a 100 Hz). Un campionamento QMP indipendente ha osservato **7 stati distinti** della regione della finestra durante la contrazione.
- Nei campioni del test con movimento completo, le aperture hanno richiesto 5–9 frame in 32–53 tick; i due zoom 6 frame in 42 e 48 tick. Le superfici sovrapposte, le dimensioni e il carico dell'host contano. Non è un desktop a 60 FPS garantiti.
- Dall'invio del tasto `=` tramite QMP all'osservazione del risultato sulla seriale: **16,1 ms in una prova**. Non è latenza input-to-photon, né un percentile. Per numeri grezzi: `tests/evidence/measurements.json`.

### Ripetere le prove

```sh
./build.sh
ISO="$PWD/build/tinymacOS-1.1-x86_64.iso" python3 tests/integration.py --evidence /tmp/prisma-verifica
```

`--evidence` evita di sovrascrivere il materiale fornito. Il test crea dischi temporanei; `tests/fixtures/ORIGIN.txt` documenta la provenienza della fixture 1.0. La lista completa di asserzioni è in `tests/evidence/results.txt`, le trascrizioni in `*-serial.txt`, i fotogrammi in `*.png`, la sequenza autentica di contrazione/ripristino in `04-movimento.webp`. `rebuild.txt` e `download.txt` registrano i confronti byte per byte. La breve demo WebM distribuita separatamente è una registrazione di un'ulteriore sessione della stessa ISO, non un rendering promozionale.

Target validato: solo la configurazione virtuale indicata. Nessun test di hardware fisico, altro firmware, PIC fallback, fault injection o recupero da perdita di alimentazione. Si veda il README per i limiti dei driver e di TinyFS.
