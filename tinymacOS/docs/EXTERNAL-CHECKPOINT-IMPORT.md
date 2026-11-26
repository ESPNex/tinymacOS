# Importazione di un checkpoint da un disco esterno

## Ambito implementato

Il backend FS2 può importare **un checkpoint nominato** da un altro disco FS2,
anche fra volumi con diversa capacità e diversa cifratura (plain/FV003).
L'operazione è disponibile dal Terminale di Recupero:

```
diskutil list
diskutil importBackup disk1 disk0 nome-checkpoint
diskutil importBackup disk1 disk0 nome-checkpoint --confirm
```

Il primo disco è la **sorgente**, il secondo il **destinatario**. I dischi devono
essere distinti, non montati dal runtime, con una disposizione d'installazione
riconosciuta e volumi già inizializzati. Se cifrati, vanno prima sbloccati
separatamente. Un checkpoint con spazi nel nome deve essere fra virgolette.

**Attenzione:** vengono sostituiti insieme documenti, account e preferenze del
destinatario con quelli del checkpoint. Per l'accesso all'account servirà la
password registrata nel checkpoint. La cifratura e le credenziali di sblocco del
volume destinatario NON vengono sostituite da quelle della sorgente. Una
sorgente cifrata importata su un destinatario plain produce dati plain.

## Garanzie del percorso

- La sorgente usa callback che rifiutano scritture e flush; non viene modificata.
- L'anteprima verifica checksum, checkpoint e spazio necessario senza scritture.
- Gli alberi e i blocchi del checkpoint vengono ricollocati sul destinatario;
  non si copiano settori fisici alla cieca né offset validi soltanto alla sorgente.
- Le allocazioni sono limitate a pagine e profondità degli alberi: nessuna
  immagine dell'intero volume, tabella di tutti i file o limite fisso di 384 file.
- Sono copiati file, directory e radice privata dell'account del checkpoint,
  non il diverso stato live successivo della sorgente.
- Una singola pubblicazione del superblocco attiva le nuove radici insieme.
  Gli alberi precedenti e gli snapshot del destinatario restano raggiungibili
  dai riferimenti duraturi pertinenti; gli snapshot esistenti non vengono eliminati.
- Non si copiano GPT/MBR, EFI, sistema sigillato, Recovery o partizione sorgente.
- Non si importa in massa il catalogo degli snapshot della sorgente: il disco
  sorgente conserva la cronologia, mentre questo comando importa un checkpoint.
- Occorre spazio libero per lo staging accanto ai riferimenti del destinatario;
  in caso di spazio insufficiente si rifiuta prima di iniziare a scrivere.
- Dopo un errore di I/O il mount temporaneo viene scartato. Una ripetizione apre
  nuovi mount e ricontrolla sorgente e destinatario.

## Test

`tests/test-fs2-import.sh`: normale e ASan/UBSan; supporto sparse da 1 TiB,
oltre 1.100 file, alberi divisi, file materializzato da 3 MiB e offset sparse
oltre 16 GiB, account, nomi con punto, rimontaggio, snapshot del destinatario,
16 interruzioni complete/parziali di scritture e flush, corruzione e spazio
insufficiente. Nessuna scrittura della sorgente.

`tests/test-fs2-import-adapter.sh`: adattatori kernel effettivi, tutte e quattro
le combinazioni plain/FV003, normale e ASan/UBSan, file da 128 KiB, account,
sblocco indipendente dopo reprobe, rifiuto fuori Recupero/sullo stesso disco/
su disco montato e canary fuori dal solo extent DATA.

`tests/fs2_external_import_guest.py`: gate reale Terminale/Recupero su due NVMe
virtuali dopo installazione GUI sullo stesso artefatto; non certificare questo
gate senza la sua ricevuta con `passed=true`. Il destinatario di prova è una
copia del disco installato dalla GUI, non un filesystem costruito dall'host.

Questa funzionalità **non è** conversione in-place della cifratura, supporto di
backup arbitrari di altri sistemi operativi, montaggio Finder di volumi esterni,
supporto 4Kn o accettazione finale di release.
