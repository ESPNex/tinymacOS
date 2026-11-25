# FileVault nativo — 10.2.2

La cifratura del volume dati di tinymacOS è un formato nativo: **non è compatibile con FileVault/APFS di Apple**. Protegge i dati a riposo, non un computer già sbloccato o compromesso. Il progetto rimane sperimentale e il formato non ha ricevuto un audit di sicurezza esterno.

## Utilizzo

- Installare tinymacOS su un disco di prova dedicato. Conservare una copia esterna dei dati prima di modificare la cifratura.
- In **Impostazioni → Sicurezza e privacy → FileVault**, aprire la sezione integrata e scegliere **Attiva FileVault**. Nella sessione ordinaria occorre la password dell'account.
- Salvare la chiave di recupero fuori dal disco: viene mostrata una sola volta. Confermare gli ultimi otto caratteri per avviare la conversione.
- La conversione avviene sul volume esistente, senza formattazione automatica. Il progresso è persistente; dopo un riavvio lo sblocco riprende la conversione prima dell'accesso al desktop.
- Al successivo avvio, inserire la password del volume o la chiave di recupero. La chiave di recupero sblocca il volume, ma non sostituisce la password dell'account per l'accesso alla sessione.
- La stessa sezione consente disattivazione, cambio della password del volume e rigenerazione della chiave di recupero. La password del volume e quella dell'account sono credenziali distinte dopo un cambio della sola password del volume.
- In **Recovery → Utility → Terminale**, eseguire `fdesetup manage`: si apre la sezione FileVault di Impostazioni. Selezionare il disco e sbloccarlo prima delle operazioni sui dati. Non è presente una voce FileVault nel menu Recovery. Le copie locali sullo stesso volume passano attraverso lo stesso livello di cifratura.
- Nel Terminale: `fdesetup status` e `fdesetup manage`.

## Cosa è protetto

Il percorso I/O sotto il filesystem cifra l'intero payload nativo: documenti, nomi, metadati, record account, snapshot e copie locali. EFI, tabella delle partizioni, kernel/sistema avviabile e sistema Recovery restano pubblici per consentire l'avvio. Sono inoltre visibili dimensioni e metadati tecnici del contenitore cifrato, non i nomi dei file.

La precedente cifratura per-file non viene convertita silenziosamente: va prima disattivata. Volumi non riconosciuti, spazio riservato insufficiente ed errori di integrità non devono provocare una formattazione. L'annullamento di un'attivazione mai iniziata richiede un filesystem ancora in chiaro, la password verificata di un account e una tabella di autenticazione completamente inutilizzata; non è un ripristino generico degli header.

## Formato e primitive

- Payload: 63.632 settori da 512 byte; header, journal e tabella di autenticazione sono riservati dopo il payload, all'interno della partizione dati.
- Chiave volume casuale da 256 bit; keyslot indipendenti per password e recupero.
- Password: PBKDF2-HMAC-SHA256, 600.000 iterazioni, sale casuale.
- Recupero: credenziale casuale da 256 bit, rappresentata da 64 caratteri esadecimali; derivazione BLAKE2b con chiave.
- Settori e keyslot: XChaCha20-Poly1305 tramite Monocypher 4.0.2. Header autenticati con BLAKE2b con chiave e duplicati.
- Nonce dei settori riservati persistentemente prima dell'uso; journal dei dati e della tabella per riprendere le operazioni interrotte.
- Entropia da RDSEED/RDRAND oppure dal raccoglitore jitterentropy; nessun ripiego sui soli timestamp o sali pubblici.

## Limiti e collaudi

Non protegge RAM, firmware, periferiche malevole o sistema compromesso dopo lo sblocco. Non offre protezione dal ripristino di una vecchia immagine completa del disco. La resistenza alle interruzioni presuppone che il dispositivo rispetti i comandi di flush. Non perdere entrambe le credenziali: non esiste una chiave universale di recupero.

I test sono in `tests/vault_host.c`, `tests/vault_independent.py` e `tests/vault_guest.py`. I primi includono volume completo, interruzioni di scritture/flush, settori parziali, ripresa della migrazione, manomissione e revoca delle vecchie credenziali. Il confronto indipendente usa Python hashlib e libsodium per decifrare un settore e un keyslot effettivamente prodotti dal motore. Il test guest esercita l'interfaccia, il riavvio a freddo, Recovery e la lettura offline del disco. I risultati della specifica ISO sono allegati alla release quando pubblicata; il solo codice di test non equivale a un collaudo superato. Nessuna certificazione su ASRock Q1900M/J1900 o altro hardware fisico.

### Riprodurre i test

Sul sistema host con GCC, Python 3 e PyNaCl installati:

```sh
cd tinymacOS
sh tests/test-vault.sh
```

Per i test QEMU predisporre le dipendenze descritte nel README di sviluppo e impostare `TMC_QA_DIR`, `TMC_STAGE_ISO`, `TMC_INSTALL_ISO`, `TMC_BUILD`, `TOOLROOT` e `TMC_FONT` verso la stessa candidata. Eseguire prima `tests/native_install.py`, che genera il disco virtuale di prova, poi `tests/vault_guest.py` e `tests/vault_settings_bios.py`. Ogni suite copia il disco in una directory temporanea dedicata; non collegare dischi fisici. La seconda verifica anche l'ingresso da Impostazioni e il riavvio cifrato BIOS senza ISO.
