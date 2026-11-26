# 10.2.5 / 30B16 — accettazione finale superata

I due requisiti obbligatori sono implementati e verificati: installazione/avvio
UEFI nativi 4Kn e conversione/deattivazione FS2 sullo stesso extent da Recovery.
Non sono stati sostituiti da esclusioni di rilascio.

## Risultati

- **31/31 suite host** effettive, inclusi normali e ASan/UBSan dove previsti,
  crittografia confrontata indipendentemente con libsodium, core FS2 fino a 1 TiB.
- **11 gate guest** sul kernel `8cbc7c1bf850d9e224ef15d55cfa1cf5621ca24ffa6137c9c6daeabe69d959db`: installazione/Recovery/reinstallazione
  512 e 4096; conversione e deattivazione reali con account/file/snapshot;
  regressioni Finder/Terminale/appunti/drag; import esterno; backup/UI/reset;
  installer cifrato e rinnovo irreversibile; BIOS senza NVMe firmware con
  controllo negativo e helper; avvio ottico BIOS e UEFI.
- Kernel normale e Recovery entro il limite loader di 160 MiB.
- 35 file comuni delle ISO, compresi EFI.IMG e payload 512/4Kn, estratti e
  confrontati byte per byte. Il codice del piccolo fix C degli store PMBR non
  allineati produce un oggetto compilato identico a quello già distribuito.

## Distribuzione normalizzata

La release offre ora una sola ISO: `tinymacOS-10.2.5-x86_64.iso`, BIOS+UEFI.
La variante solo UEFI non è più un asset scaricabile. Nessuna immagine è stata
ricompilata; la tabella e le ricevute sottostanti conservano entrambi gli hash
come storico della validazione originale. `SHA256SUMS` della release elenca
soltanto i quattro payload ancora distribuiti (ISO, helper, sorgenti, report).

## Hash dei supporti

| Artefatto | SHA-256 |
|---|---|
| ISO UEFI storica, rimossa dai download; supporto della matrice completa | `ea98bb53bb3abbf82b3592d44bdd9bc1e9b9cc84c78a9af6b0ad4d5df76a7293` |
| **Unica ISO distribuita**, BIOS+UEFI, con proprie prove di avvio | `1ce54378e933ca388f0fa1ba2471612422a826a2ec033722b29daab4bbfcb30a` |

Le due ISO non sono byte-identiche e non si attribuiscono all'una le prove
dell'altra senza la verifica esplicita dei payload comuni.

## Evidenza e limitazioni

`workspace/local-10.2.5/release-final-verification.json` aggrega le ricevute.
I log accettati e le prime esecuzioni fallite dei driver host sono conservati
separatamente; `release-final-evidence.tar.xz` conserva ricevute e log guest.
Le correzioni ai driver QA non sono false dichiarazioni di PASS degli aborti.

La conversione richiede una coda non referenziata sufficiente; conserva snapshot
e ID, non compatta, è riprendibile e non rollback. Azzeramento logico, non secure
erase fisico. 4Kn su disco vuoto; resize/modifica 4Kn esistenti rifiutati. Nessuna
certificazione hardware/Windows. [Note complete](NOTE-10.2.5.md).

La pubblicazione è un passaggio distinto: lo stato e il readback degli asset
sono riportati in `STATUS.json` e nella ricevuta di pubblicazione.
