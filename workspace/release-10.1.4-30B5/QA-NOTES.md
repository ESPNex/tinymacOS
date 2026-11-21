# Annotazioni di esecuzione QA 10.1.4

I risultati contano solo con processo terminato e asserzioni superate; non basta un log parziale.

## Errori di integrazione trovati e corretti prima del gate

- Primo link desktop fallito: il renderer `alpha_symbol()` era stato scritto soltanto nella copia OS e sovrascritto dall’overlay autorevole durante `--apply`. Corretto nell’overlay e nella copia OS; controllo 21 integrazioni + 7 ABI allineati. Le due build indipendenti definitive includono il renderer corretto e producono la medesima ISO. Il log del primo errore è conservato esternamente, non è un PASS.
- Prima compilazione delle sonde kernel QA fallita perché il packer ora richiede `control-svg.json`, ma il packer della sonda generava soltanto i vecchi asset. Aggiunta la compilazione SVG anche in `build-guest-network-probe.sh`. Le sonde sono QA-only, non asset di release. La suite kernel è stata ripetuta dopo questa correzione.
- Primo tentativo ottico BIOS/EHCI/tablet arrivato a Recovery e alla shell, ma fallito sull’asserzione completa `diskutil info /`: nel framebuffer mancava `/`, che il driver di test invia come Shift+7. Il log e i frame del tentativo fallito restano in `/var/tmp/tmc-10.1.4/optical-ehci-r-attempt-1-failed/`. Nel test ottico BIOS, gli eventi modificatore/tasto della stessa combinazione ora hanno transizioni separate (lead 200 ms, tasto 300 ms, rilascio tasto prima del modificatore). Nessuna modifica al driver di produzione o indebolimento della verifica della risposta shell per far passare questa prova. Questa osservazione non dimostra da sola una causa fisica o un difetto del dispositivo reale.

## Confini delle prove

- I tre controlli negativi X/Y usano identici pacchetti e il sorgente originale 10.1.3, non un sostituto volutamente rotto.
- I quattro contratti HCD controllano codice reale con registri/eventi simulati; non costituiscono accesso a un controller fisico.
- QEMU esercita avvio, input via dispositivi emulati e framebuffer realmente renderizzato; non è prova della compatibilità ASRock.
- Le icone derivano da SVG originali compilati in maschere native. Non è presente un interprete SVG generale nel sistema.
- `final-qa.json` è il gate dei risultati completati. Solo `github-release-verification.json` conferma la pubblicazione e il download anonimo completo.

## Esito finale prima della pubblicazione

Entrambe le matrici complete sono terminate con exit 0. Superato il gate di 62 ricevute, 589 file di sorgente verificati, 27 profili live/HDD e 29 regressioni kernel. Tutti e quattro gli avvii ottici BIOS (EHCI/xHCI × R/F12), ciascuno con mouse e tablet, hanno superato anche la risposta shell completa dopo le transizioni modificatore separate. Superate le quattro prove di tastiera italiana Recovery, inclusi punteggiatura, AltGr e backspace UTF-8. Il risultato fisico resta non verificato.
