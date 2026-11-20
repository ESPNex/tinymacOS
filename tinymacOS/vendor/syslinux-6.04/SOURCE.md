# Complete corresponding source

The exact extracted and Debian-patched corresponding source (1,592 upstream/package files plus a source-access note) is provided in this same repository, branch `vendor/syslinux-6.04`, immutable commit `53791ac78b90525b81bd5b7ae692a681c4c3e12a`.

https://github.com/ESPNex/tinymacOS/tree/53791ac78b90525b81bd5b7ae692a681c4c3e12a

Clone with:

```sh
git clone --single-branch --branch vendor/syslinux-6.04 https://github.com/ESPNex/tinymacOS.git syslinux-source
```

All recipients of the ISO must retain equivalent access to this corresponding source branch, the license/copyright notices and Debian package build instructions. The repository is private; source access requires the same authorized GitHub access as the binary release. No ZIP/TAR deliverable is needed. Complete vendor sources are kept in that branch so the Arena snapshot does not have to hold the additional 13 MiB. Main preserves the selected redistributed binaries and their SHA-256/package locks.
