# Collaudo · tinymacOS 1.2.2 «Aurora»

**ISO UEFI x86-64:** `tinymacOS-1.2.2-x86_64.iso` · 25.147.392 byte · SHA-256 `e763fe876314cac5b69caa5675bdec35d221c127a1ec74a5b1f5b39b5ea5a2e1`. La dimensione e il digest coincidono con quelli del file su Drive. L’immagine di collaudo non viene conservata nel workspace.

## Il difetto riprodotto e la correzione

Con il CD 1.2.1, QEMU/OVMF, CPU `max,rdrand=off,rdseed=off`, nessun dispositivo RNG e disco dati vuoto, il modulo con dati validi mostrava «Sorgente casuale non disponibile su questa macchina». Nessuna finestra aperta; entrambi i settori account ancora vuoti. Non era una password sbagliata. La nuova ISO è stata avviata con le stesse impostazioni. Il kernel ha selezionato il raccoglitore di jitter dopo i suoi controlli, ha creato l’account e ha aperto Finder. Il test non ha aggiunto seed alla ISO o al disco.

## Cinque verifiche specifiche (CD di produzione)

1. Due nuove installazioni, medesimi orologio virtuale iniziale, nome e password, nessun generatore EFI/CPU/virtio disponibile: due salt distinti. Entrambi i record per disco sono coerenti, e i verificatori corrispondono a PBKDF2-HMAC-SHA256/120000 calcolato da Python.
2. Disattivati anche il bit TSC della CPU e il dispositivo virtuale: compare la pagina *Codice di configurazione*, prima di qualsiasi password. Codici brevi o degeneri rifiutati. Un codice appena generato con il generatore di sistema dell’host viene accettato; da solo non apre la sessione e non crea l’account. La successiva normale creazione salva gli esatti 16 byte di salt e il verificatore corretto.
3. Con il dispositivo virtio-rng del launcher, CPU priva di RDSEED/RDRAND e nessun disco dati, il guest raggiunge Finder. Sorgente registrata in questa configurazione OVMF: **EFI RNG**.
4. Un account e un documento sono stati creati con la vera ISO 1.2.1. Sostituendo il solo CD con la 1.2.2 e nascondendo anche il TSC, l’accesso non ha richiesto alcuna sorgente casuale né alcun codice. Password errata rifiutata, originale accettata. Documento riletto; **l’intero disco di 8 MiB era identico byte per byte** dopo la prova.
5. Le trascrizioni dei test mirati non contengono il codice casuale fornito dall’host, la password della prova, eccezioni inattese o kernel panic. Non è un audit di sicurezza né una certificazione del generatore.

## Regressione

**52 verifiche di accettazione superate** sulla VM senza RNG hardware/firmware utilizzabile: boot El Torito e Ring 3; blocco e accesso, avvio a freddo, recupero da un record valido e rifiuto dei record illeggibili; rendering nativo, nove app, dati e preferenze persistenti, sessioni live e migrazione da TinyOS 1.0. Per la mela fornita sono stati confrontati tutti i pixel a 36 stadi intermedi. La barra resta all’avvio. Le prove di creazione, password errata e password corretta hanno osservato tutte le dodici posizioni luminose dello spinner; con movimento ridotto una sola. Finder e Terminale continuano a leggere e verificare i veri file del volume avviato. Il download della console VM ha lo stesso digest del CD su Drive.

[Elenco delle 52 verifiche](../tests/evidence/results.txt) · [misure](../tests/evidence/measurements.json) · [verifiche sulle sorgenti e sul cambio CD](../tests/evidence/salt/salt-checks.json). L’immagine della pagina opzionale è `tests/evidence/salt/configurazione-solo-se-necessaria.png`. I risultati testuali sono inclusi anche nel kit dei sorgenti.

```sh
./build.sh
python3 tests/crypto.py
TMC_RNG=none TMC_CPU=max,rdrand=off,rdseed=off python3 tests/integration.py --evidence /tmp/aurora-122
python3 tests/salt_compat.py --evidence /tmp/aurora-122-salt --previous /percorso/tinymacOS-1.2.1-x86_64.iso
```

Senza il vecchio CD, omettere `--previous`: verrà saltata solo la prova di aggiornamento da un suo account. Ambiente di riferimento: QEMU x86-64 10.0.13 (TCG), OVMF, 384 MiB, una vCPU, PS/2 e VGA standard. Nessun test qui implica compatibilità universale con macchine fisiche o altri hypervisor.
