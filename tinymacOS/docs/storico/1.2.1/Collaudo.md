# Collaudo · tinymacOS 1.2.1 «Aurora»

**52 verifiche di accettazione superate sul CD** `tinymacOS-1.2.1-x86_64.iso` (25.018.368 byte). SHA-256 `c09cad34ee788176f8de60039090b7a28ceb4d6385eb6e00f87d7860a92c789b`, uguale nei metadati del file scaricabile su Google Drive. L’immagine avviata è esterna all’albero dei sorgenti. Le immagini di questa prova sono catture del framebuffer del guest, non illustrazioni dell’interfaccia.

## Logo e accesso

- **Avvio.** Nel supporto UEFI il logo bianco usa il tracciato di `assets/apple.svg`; la barra sottostante continua a seguire letture e inizializzazioni. In questa esecuzione sono state campionate 37 larghezze distinte, da 1 a 192 pixel. A tutti i 36 stadi intermedi catturati, l’area del logo (94 × 108) coincide pixel per pixel con la maschera ottenuta dal vettore. Il confronto esclude lo schermo del firmware e il cambio di scena verso il login. [Frame di avvio](../tests/evidence/00-avvio.png).
- **Login.** Dopo Invio il campo password lascia il posto a dodici tacche bianche arrotondate; la tacca più luminosa avanza in senso orario intorno al centro vuoto. La prova osserva i pixel sul cerchio e negli spazi tra le tacche, senza leggere la fase dal programma. Non sono presenti una barra di login o un cursore a girandola. L’indicatore termina all’esito della verifica: la password errata ripristina il campo con l’errore, quella corretta riprende la sessione già aperta. [Fotogramma intero](../tests/evidence/00i-spinner-login.png) · [ritaglio animato alla risoluzione nativa](../tests/evidence/00i-spinner-login.webp).

| Operazione reale | Campioni con spinner | Posizioni luminose distinte |
|---|---:|---:|
| Creazione account | 40 | 12 |
| Accesso rifiutato | 34 | 12 |
| Accesso riuscito | 34 | 12 |
| Accesso con movimento ridotto | 27 | 1 (ferma, con messaggio di stato) |

Il logo del menu è rasterizzato direttamente a misura, e quello della finestra Info dallo stesso SVG. La console facoltativa serve il nuovo CD esterno: il download HTTP della console è stato confrontato tramite SHA-256 con l’immagine avviata.

## Regressione

- El Torito UEFI, loader PE32+ x64, passaggio al kernel in long mode, client Ring 3, allocazione di memoria, interrupt e scheduler. Tutte le sette trascrizioni della suite sono prive di panic, eccezioni CPU inattese e password di prova in chiaro.
- Primo avvio, errori di conferma, blocco e accesso dopo riavvio. Sblocco di una sessione con una riga di Terminale già in corso senza inoltrarle i tasti del login. Verificatore PBKDF2-HMAC-SHA256/120000 confrontato con `hashlib`, recupero col record di backup, rifiuto senza sovrascrittura se entrambi i record persistenti sono invalidi.
- Finder e Terminale leggono gli oggetti originali del CD e ne calcolano l’hash. Verificate le copie nel FAT di avvio, la paginazione dell’intero ELF fino all’ultima pagina, i divieti di modifica e l’assenza di copie parziali oltre il limite di TinyFS.
- Nove app come processi distinti; documenti, calendario, calcolatrice, monitor e galleria; trascinamento, ridimensionamento, Dock e passaggi fra finestre. Vetro/opaco e movimento ridotto provati anche dopo il riavvio. Con lo stesso disco un nuovo guest ritrova i dati; due sessioni solo-CD ripartono in RAM senza dati. Provata la migrazione su una copia di un disco 1.0.

| Oggetto letto nel guest | Byte | SHA-256 |
|---|---:|---|
| `kernel.elf` | 7.496.120 | `ccec0db6da3ad4556832a3aaf3b2ae46319dd3b4bb99df090bd291ec24998383` |
| `Avvio/BOOTX64.EFI` | 196.772 | `a309a0faa795ce968909bda3cb412fd99939a3970ead9e0466363a81ecc6d2c0` |
| `kernel.map` | 32.509 | `2cef9f29d32bd1ca6185cfd4c0d6503b8028332a19cd03c70b093d14c7b636bb` |
| `Sorgenti/kernel/main.c` | 756 | `90bf87d728f292cdf1b9c1d52456f65c7fa7816eaabfe25b474a915f5a8f9450` |

## Ripetere la prova

```sh
./build.sh
python3 tests/crypto.py
python3 tests/integration.py --evidence /tmp/aurora-121-evidence
```

Linux amd64, GCC 14.2, binutils 2.44, QEMU 10.0.13/OVMF 2025.02, TCG, `pc -cpu max -smp 1 -m 384 -vga std -nic none`, tastiera e mouse PS/2. Non è un test su hardware Apple o con Secure Boot. [Elenco completo](../tests/evidence/results.txt) · [Misure](../tests/evidence/measurements.json). Lo ZIP dei sorgenti include i due file testuali; le catture grafiche si rigenerano con la suite. La registrazione `Aurora-avvio-login-Finder.webm` e lo ZIP 1.2 rimangono documenti della versione precedente, non prove della 1.2.1.
