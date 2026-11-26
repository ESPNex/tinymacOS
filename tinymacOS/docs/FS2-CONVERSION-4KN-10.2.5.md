# FS2: conversione sullo stesso extent e geometria nativa 4Kn

## Conversione offline

Da Recovery, aprire Terminale → `fdesetup manage`, selezionare il disco e usare
FileVault. L'attivazione richiede una nuova password e la conferma degli ultimi
otto caratteri della chiave di recupero. La disattivazione accetta password del
volume o chiave di recupero. Non è il vecchio reset: conserva account privati,
file, catalogo snapshot e blocchi condivisi, senza formattare il volume.

Preparare prima una copia esterna verificata. La conversione richiede spazio
libero **nella coda non referenziata dello stesso extent**, sufficiente per una
copia temporanea con tutti gli ID di blocco ancora referenziati, incluse entrambe
le radici di fallback e gli snapshot. Non elimina cronologia, non compatta gli
ID e non promette di convertire un volume quasi pieno o molto frammentato.
Capacità/spazio insufficienti e credenziali errate vengono rifiutati prima di
qualsiasi scrittura. FV003 usa record autenticati da 20 KiB per blocchi FS2 da
16 KiB: la capacità disponibile diminuisce all'attivazione e viene ripristinata
alla disattivazione.

La preparazione scrive solo la coda verificata libera e verifica la copia.
Due journal autenticati conservano credenziali e avanzamento. La successiva
rilocazione è **riprendibile, non rollback**: dopo un'interruzione riaprire
FileVault in Recovery e autenticarsi con password o chiave. Durante il processo
il kernel impedisce mount e accessi che aggirino il convertitore. Dopo la verifica
della destinazione viene azzerata, con avanzamento persistente, tutta la coda
inutilizzata, comprese copia temporanea e precedenti tracce nei blocchi liberi.
Solo dopo vengono ritirati i journal. Errori o metadati non autenticabili bloccano
il volume; non inizializzarlo per tentare di sbloccarlo.

L'azzeramento è logico: non è secure erase di celle rimappate, cache esterne o
copie precedenti, né protezione contro il replay di un'intera immagine vecchia.

## Installazione 4Kn

I namespace NVMe con settori logici nativi da 4096 byte entrano nell'inventario.
L'ABI interna rimane a unità da 512 byte; il trasporto traduce e conserva i byte
adiacenti nelle operazioni parziali. GPT, CRC, PMBR e offset vengono emessi nelle
unità native. ESP e Recovery usano veri BPB FAT da 4096 byte, ricostruiti e
riletti indipendentemente; non sono immagini 512e rinominate.

L'installazione 4Kn è ammessa su disco vuoto, verificato integralmente prima
delle scritture. La modifica/resize di partizioni 4Kn esistenti rimane rifiutata
senza scritture. L'avvio diretto è UEFI; un BIOS incapace di leggere NVMe richiede
un helper USB/SATA leggibile dal firmware, non codice MBR sul disco 4Kn.

## Verifiche riproducibili

- `tests/test-fs2-convert.sh`: core FS2 reale, account/file/snapshot conservati,
  credenziali errate senza scritture, journal corrotti fail-closed, interruzioni
  di scrittura/flush e settori parziali durante rilocazione e pulizia.
- `tests/test-native4k-install.sh`: installer e adattatore Recovery reali con
  trasporto RAM, GPT nativo, conversione/deattivazione e invarianti esterni.
- `tests/native4k_install_guest.py`: QEMU NVMe con LBA logico/fisico 4096,
  installazione GUI, cold boot senza ISO, Recovery interna e reinstallazione.
- `tests/fs2_convert_guest.py`: vera UI Recovery, conversione, credenziali,
  avvio cifrato, deattivazione, avvio plaintext e verifica di file/snapshot.

Le ricevute finali indicano gli hash esatti dell'artefatto e gli esiti effettivi;
la sola presenza di questi test non costituisce un PASS. Nessuna certificazione
su hardware fisico è implicita.
