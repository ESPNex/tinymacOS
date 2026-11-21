# Note di esecuzione 30A2

- Primo ciclo precedente alla correzione utente interrotto: non usato come gate finale.
- Ricostruite da zero due ISO dopo le modifiche Recovery: final e repro coincidono.
- Un avvio parallelo della tastiera BIOS Recovery è stato anticipato rispetto alla creazione della fixture installer: FileNotFoundError, nessuna prova guest eseguita. Ripreso dopo la creazione del disco; le ricevute finali richiedono SHA-256 corrente.
- Un lancio delle app aggiuntive era privo di TMC_TEST_ISO (KeyError prima della VM): rilanciato con ambiente esplicito. Il collector richiede il risultato effettivo del rilancio.
- Prove supplementari del selettore R in Recovery ottico BIOS non superate; tentati campionamento logo, polling rapido e tastiera PS/2. Stesso esito negativo anche sul controllo costituito dalla ISO 30A1 pubblicata. Non trasformato in PASS; risultato negativo ed esclusione dal conteggio dei PASS documentati in supplementary-selector-result.json e RELEASE.md.
- Il mouse fisico ASRock NON è stato verificato da remoto. Nessun PASS host o QEMU dimostra la soluzione su quella macchina.
