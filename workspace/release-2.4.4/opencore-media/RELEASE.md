**Build 21B5 · revisione ottica OC1.**

## Novità
- Aggiunto l'avvio BIOS dallo stesso CD: ISOLINUX/MEMDISK → OpenDuet BlockIO 1.0.8 → OpenCore → loader nativo.
- Percorso UEFI diretto conservato, con payload identici alla 2.4.4 originale.

## Verifiche e limiti
Avvio dalla sola ISO fino al desktop con SeaBIOS e OVMF, account, input e due CPU online. **Il disco installato resta solo UEFI**: il supporto BIOS autonomo arriva con OC2. Non è un'immagine USB-DD ibrida.

OpenCore e Syslinux sono inclusi con licenze e provenienza. I sorgenti Syslinux corrispondenti, con patch Debian, sono conservati nel branch `vendor/syslinux-6.04`, commit `53791ac78b90525b81bd5b7ae692a681c4c3e12a`, della stessa repository.

Asset: `tinymacOS-2.4.4-x86_64.iso` e `SHA256SUMS.txt`.
