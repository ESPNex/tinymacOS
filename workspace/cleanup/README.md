# Pulizia autorizzata prima del gigablocco 10.3

1945 file (88.438.898 byte) rimossi dalla copia locale SOLO dopo backup GitHub immutabile, riscaricato e verificato membro per membro. Ricevuta e link in `github-archive-verification.json`; manifesto completo nel medesimo commit remoto. Il grande archivio non rimane nel workspace né nella punta main: si recupera dal commit immutabile.

Rimosse vecchie copie di lavoro/release, screenshot/archivi locali storici e implementazione Recovery EFI obsoleta con i suoi test specifici. Conservati sorgenti attuali, workspace WebKit e ricevute 10.2.5. Lo sfondo condiviso della Recovery nativa è spostato in `desktop/recovery_background.h` senza modificarne il contenuto; aggiornato include e runner dei vecchi test EFI. La Recovery nativa e i relativi guest test restano presenti.

**Non cancellato `fs/btrfs.c`: contiene il dispatcher ancora attivo e il fallback RAM usato da live/Recovery quando fs2_mount non monta un volume installato.** FS2, bootfs, installer, conversione/cifratura, NTFS, driver e loader EFI attuali non sono implementazioni unused. Eliminarli per nome renderebbe il sistema non avviabile o danneggerebbe le funzioni consegnate. Le vecchie copie filesystem nei workspace storici sono invece archiviate/rimosse.

Nessuna ISO o release creata/modificata dalla pulizia. I binari distribuiti restano invariati. Questo intervento autorizza soltanto i cambi di sorgente esplicitamente documentati, non un ritorno al filesystem legacy.
