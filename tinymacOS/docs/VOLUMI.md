# Montare e smontare i volumi

Nel Finder, fai clic destro su un volume dati nella barra laterale e scegli **Smonta** o **Monta**, secondo il suo stato. In Utility Disco, seleziona il volume nella barra laterale e usa **Smonta** o **Monta** nella barra degli strumenti, accanto a **S.O.S.** Il pulsante cambia secondo lo stato effettivo del volume; per dischi interi, spazio non allocato, snapshot e volumi protetti è disabilitato. Un volume smontato rimane nell’elenco, ma i suoi file non sono accessibili finché non viene rimontato.

Nel Terminale:

```sh
mount
umount disk0s1
mount disk0s1
diskutil unmount /Volumes/disk0s1
diskutil mount /Volumes/disk0s1
```

`mount` senza argomenti elenca i volumi e il loro stato. Usa il nome effettivamente riportato sul tuo sistema: `disk0s1` è soltanto un esempio.

- Chiudi i documenti aperti sul volume prima di smontarlo. Se ci sono file aperti, l’operazione viene rifiutata.
- Una scrittura non completata impedisce lo smontaggio: il volume rimane montato.
- Il volume di avvio e i ruoli di sistema protetti non possono essere smontati da questi comandi.
- Non è disponibile uno smontaggio forzato. Il rimontaggio verifica il filesystem prima di renderlo nuovamente accessibile.
- I volumi dati supportati sono TMCFS2, FAT32, exFAT e NTFS. La disponibilità di un controller o di un formato immagine è un requisito separato: questi comandi non aggiungono da soli il supporto storage USB o `hdiutil`.

Le vecchie unità con filesystem nativo non più supportato non vengono montate, convertite o modificate.
