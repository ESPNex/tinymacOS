# Sorgenti della release precedente (20A2 · 2.0.1)

La ISO 2.0.1 e il suo Recupero USB separato restano su Drive ai link in `../../consegna/Riviera-2.0.1.md`; i risultati di QA della 20A2 sono in `tests/evidence/riviera-2.0.1`. Le pagine di consegna 2.0.1 sono archiviate in questa cartella.

Prima delle modifiche per il supporto unico i sorgenti 20A2 sono stati salvati fuori dal workspace. La patch `20A3-a-20A2.patch` riporta gli **input di build della 20A3 alla 20A2** (testo, senza media binari). Applicata a una copia esterna della cartella corrente, con `patch -p1`, ha ripristinato il salvataggio pre-modifica: 36 file, nessun rifiuto, nessuna differenza residua (tranne `__pycache__` rigenerato).

Solo **su una copia della cartella del progetto**, dalla radice di tale copia:

```sh
patch -p1 < docs/storico/2.0.1/20A3-a-20A2.patch
TMC_BUILD=/var/tmp/riviera-storica-20A2 ./build.sh
```

Non applicare la patch ai sorgenti attivi. I test della 2.0.2 si usano con la 2.0.2; quelli della 2.0.1 sono nell’archivio. Nessun media va creato nel workspace, nemmeno temporaneamente.
