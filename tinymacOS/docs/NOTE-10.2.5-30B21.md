# tinymacOS 10.2.5 — 30B21

## Regressione mouse: ripristino del profilo della ISO 10.2.4

Le precedenti 30B18, 30B19 e 30B20 non hanno risolto il blocco del mouse USB
sul PC dell'utente. Le prove QEMU non erano una certificazione fisica.
Il rollback di desktop/main.c e drivers/input.c non ripristinava la
configurazione di compilazione della versione funzionante.

Il confronto è stato rifatto sulla ISO ufficialmente distribuita della 10.2.4,
non solo sul commit dei sorgenti. ISO SHA-256:
`7b6059a4c82fee9add7a642690ac679d554a287a9c7ed277cff2279017750b21`.
Il kernel estratto dalla sua KernelCache avvia lo stack Linux USB core/HID
canonical: input_init salta al relativo startup e lo scheduler crea il worker
canonical. Anche Recovery contiene lo stack canonical. Nel kernel distribuito
30B20 il percorso è invece la facade nativa: canonical non era compilato.
È una differenza binaria verificata, non ancora la dimostrazione della causa
fisica. I 25 sorgenti confrontati del percorso canonical/HCD/scheduler/input
sono identici al commit dichiarato dalla validazione 10.2.4.

### Cambiamento

- build.sh impone `TMC_USB_CANONICAL=1` per sistema live/installato e Recovery,
  come già impone FS2 per evitare selezioni ambientali silenziose.
- Nuovo controllo sui binari effettivi: input_init deve saltare allo startup
  canonical, lo scheduler deve referenziare il worker e deve essere collegato
  il percorso HID completo. La 30B20 fallisce questo controllo.
- L'audit della ISO finale confronta kernel live/installato/Recovery con i
  binari che hanno superato il controllo; non bastano stringhe presenti.
- Prove guest osservano registrazione usbhid, contatori del worker in crescita,
  facade storica inattiva, mouse e tastiera USB senza PS/2, pixel del cursore,
  scollegamento e riconnessione. Resta necessaria la conferma sul PC interessato.

### Conservato

Stessa release 10.2.5; una sola ISO BIOS+UEFI. Nessun nuovo sistema 10.3.
Desktop/input della 10.2.4, DnD file disabilitato, menu Finder Taglia/Copia/Incolla
e appunti Terminale. Font UI e Mono forniti, senza fallback DejaVu. FS2,
installazione/avvio 4Kn e conversione/disattivazione cifratura in-place.
Nessuna modifica alla visibilità EFI/Recovery.

Recovery «Reinstalla» senza tinymacOS preesistente resta distinta dalla nuova
installazione dal desktop live; il messaggio generico non prova corruzione GPT.
RAW/MSR/NTFS restano occupati: solo spazio realmente non allocato, nessun bypass
delle protezioni GPT. La riparazione 55AA resta specifica e soggetta a consenso.

**Non dichiarato:** soluzione fisica già confermata, certificazione Windows
reale, avvio USB-DD ibrido o inclusione di Safari/WebKit completo.
Helper NVMe mantenuto byte-identico dalla 30B18, senza rivendicare un nuovo test.
