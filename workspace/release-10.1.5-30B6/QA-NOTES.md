# QA 10.1.5 — limiti e correzione del collaudo stesso

## Riscontro fisico

La 10.1.4 non muove il mouse né al login né in Recovery sul PC dell’utente. Questo prevale sulle ricevute PASS delle vecchie VM. Nessuna nuova prova fisica ASRock è stata effettuata; la 10.1.5 non viene dichiarata risolutiva da remoto.

## Prove nuove, non riuso dei vecchi PASS

`regression-negative-controls.py` compila i trasporti e il decoder originali 10.1.4 e quelli nuovi, con i medesimi input. Dieci casi riproducono mancata consegna di eventi validi e disconnessione del proprietario firmware. `linux-hcd-contract.py` aggiunge un undicesimo caso sullo stop EHCI a vuoto e conserva quattro contratti precedenti. Il codice precedente deve compilare e fallire un’asserzione di valore, non un errore del compilatore o del sanitizer.

## Errore scoperto nella pipeline kernel

Dopo aver eliminato le vecchie copie generate per liberare spazio, la prima compilazione delle sonde si è fermata: il packer creava la ISO con il nome corrente, ma manifest e launcher cercavano ancora **`tinymacOS-10.1.2-x86_64.iso`**. La presenza di un vecchio file poteva quindi consentire il riuso di media QA obsoleti. Questo mette in dubbio l’associazione alla versione corrente di quei precedenti test kernel; non viene nascosto né usato come prova della causa del mouse.

Correzione della pipeline, non del risultato:

- Le sonde appena create ricevono un nome QA stabile tramite rename della ISO realmente prodotta in quel comando.
- Manifest con hash della ISO, ELF e KernelCache; confronto byte per byte tra kernel della sonda e build di produzione corrente.
- `probe_media.py` verifica gli hash **prima di ogni avvio**, estrae KernelCache dalla ISO effettiva con un lettore indipendente e lo confronta con la produzione.
- Controllo dei sorgenti registrati prima della compilazione; fail-closed se cambiano o manca il nuovo manifest.
- Cinque ricevute `kernel-probe-{vm,network,sync,tls,threads}.json` vincolate al kernel della release, oltre alla suite kernel completa rieseguita.

Le sonde rimangono QA-only e non vengono pubblicate come ISO di sistema o browser. I test input retail, installazione e Recovery usano invece esplicitamente l’hash della nuova ISO di release, non il media delle sonde.

## Criterio finale

Solo `final-qa.json` completo certifica i gate software della 10.1.5; solo `github-release-verification.json` certifica pubblicazione e download anonimo completo. Un processo ancora in corso, un tentativo fallito o un log parziale non costituiscono PASS. Nessuna di queste ricevute certifica l’hardware fisico.

## Esito prima della pubblicazione

Entrambe le matrici complete terminate con exit 0. Gate di 80 ricevute superato, 589 file di sorgente verificati. Tutti i 13 profili guest kernel sono legati ai 5 media appena generati e al kernel corrente, inclusa estrazione indipendente di KernelCache. Il finalizzatore distingue esplicitamente ISO retail e ISO QA (che sostituisce il comando): verifica gli hash retail contro la release e quelli QA contro i manifest delle sonde, con identità del kernel obbligatoria; non richiede che i due volumi interi siano identici. Questa distinzione ha corretto un primo errore del finalizzatore, non un test fallito del kernel. La verifica fisica resta assente.
