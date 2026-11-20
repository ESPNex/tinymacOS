# Console di Levante 2.4.0 (solo host)

La console visualizza la vera VM QEMU via WebSocket/RFB. Serve un solo supporto: la ISO. Non richiede `BaseSystem.img`, non espone un download DMG e non avvia una seconda VM USB per il Recupero.

- **Avvia dalla ISO:** parte il CD con un disco SATA virtuale vuoto e temporaneo. Per il Recupero premi **R alla mela** oppure clicca **Invia R** nello stesso momento. Il comando arriva alla tastiera USB della VM; il loader apre il DMG dentro il CD.
- Il piè di pagina offre un unico download dal percorso `/cd-di-avvio`. Non viene servito il workspace. Le VM non hanno rete e il loro disco viene eliminato alla disconnessione: non usare dati personali.
- Per un’installazione permanente usa la ISO nella tua VM locale. Dopo l’installazione rimuovi il CD: R apre la copia di BaseSystem sul disco.

```sh
export TMC_BUILD=/var/tmp/tinymac-riviera
./build.sh
NOVNC=/usr/share/novnc python3 tools/console/server.py
```

Serve Python ≥3.11 con `aiohttp`, noVNC 1.6, QEMU e OVMF. L’URL predefinito è `http://127.0.0.1:8080`; per un proxy HTTPS/WebSocket configurare `BIND` e `PORT`. Il percorso di default dell’immagine corrisponde a `tools/env.sh`. Per servire una ISO già scaricata basta impostare `ISO=/percorso/tinymacOS-2.3.0-x86_64.iso` (non occorrono altri file di build). Le release storiche conservano le loro pagine e i loro supporti distinti.
