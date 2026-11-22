# Contribuire

## Modifiche al sistema

L'albero principale è `tinymacOS/`. Verificare la direzione della sincronizzazione prima di aggiornare i mirror in `workspace/integrated-next/` e `workspace/webkit-tmc/`: alcuni strumenti copiano file, non risolvono conflitti. Conservare licenze, lock e provenienza dei componenti esterni.

Le cartelle di release e manutenzione contengono ricevute e snapshot storici. Non riscrivere manifest, hash o risultati per farli apparire validi sul codice nuovo. I finalizzatori di una vecchia release non devono essere eseguiti su un albero successivo. Toolchain e immagini generate vanno fuori dal checkout.

Per codice eseguibile occorrono i contratti pertinenti, le regressioni kernel e le prove live/installato prima della pubblicazione. Una modifica soltanto documentale non richiede una nuova ISO o una nuova versione.

## Documentazione senza duplicazioni

- Il README principale presenta il progetto e riporta soltanto lo stato corrente.
- Il README del sistema documenta build e struttura; il quickstart spiega l'uso.
- Ogni nota di release descrive solo cambiamenti, verifiche pertinenti e limiti di quella build. Il titolo GitHub non va ripetuto come intestazione nel corpo.
- Le note canoniche sono i file `RELEASE.md` nelle rispettive directory di release; il testo GitHub deve corrispondere. Il vecchio testo resta nella cronologia Git.
- Evitare sezioni aggiunte in coda per rettificare le precedenti: aggiornare la sezione esistente. Non copiare il changelog nel README.
- Non ripetere link alla stessa release, download già negli asset o checksum già in `SHA256SUMS.txt`. Conservare i riferimenti tecnici e legali necessari.
- Non definire “Latest” le versioni storiche; non confondere test virtuali con riscontri fisici.

Il controllo documentale si esegue dalla radice:

```sh
python3 workspace/documentation/check.py
```

## Pubblicazione e accesso

Sincronizzare i sorgenti prima della pubblicazione e verificare il commit remoto. Non modificare tag o asset esistenti per una correzione editoriale. Nelle manutenzioni conservare le vecchie release come pre-release.

Non commettere credenziali, chiavi, diagnostica del PC o configurazioni locali. Nell'ambiente Arena `workspace/github_sync.py` controlla i file prima del push e richiede `--allow-public`; il database Git e l'autenticazione restano esterni al checkout. Un normale clone non richiede questa disposizione locale.
