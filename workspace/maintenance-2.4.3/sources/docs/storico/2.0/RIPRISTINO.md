# Sorgenti della release precedente (20A1)

La ISO e il Recupero 2.0 originali restano disponibili ai link in `CONSEGNA.txt`. I vecchi risultati di QA restano in `tests/evidence/riviera` alla radice del progetto.

Prima delle modifiche per il tablet sono stati salvati i sorgenti 20A1 fuori dal workspace. Per conservarne le differenze anche insieme ai nuovi sorgenti, `20A2-a-20A1.patch` riporta gli **input di build** della 20A2 alla 20A1. È testo, non contiene una ISO o un archivio binario. La patch è stata applicata a una copia esterna dei sorgenti: i file ottenuti sono stati confrontati con il salvataggio precedente alle modifiche.

Solo **su una copia della cartella del progetto**, dalla radice di tale copia:

```sh
patch -p1 < docs/storico/2.0/20A2-a-20A1.patch
TMC_BUILD=/var/tmp/riviera-storica-20A1 ./build.sh
```

Non applicare la patch ai sorgenti attivi se vuoi compilare il tablet. I test 20A2 vanno usati con la 20A2. Nessun media deve essere creato nel workspace, nemmeno temporaneamente.
