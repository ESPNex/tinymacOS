# Corresponding source for the ISOLINUX/MEMDISK transport

This orphan vendor branch contains the complete extracted Debian syslinux source package 3:6.04~git20190206.bf6db5b4+dfsg1-3, with the Debian quilt patches applied by dpkg-source. The three redistributed binaries in main/tinymacOS/vendor/syslinux-6.04 come from the matching SHA-256-pinned Debian isolinux and syslinux-common packages. No source ZIP/TAR is required to browse or clone this branch.

Source and binary package URLs/hashes are pinned in main's vendor-lock.json and debian-source-index.txt. Copyright and GPL notices are retained. Clone this branch to obtain the complete corresponding source; install Debian build dependencies listed in debian/control and use Debian's standard dpkg-buildpackage build workflow. This is only an optical RAM-disk transport: native BIOS firmware remains OpenDuet/OpenCore and the operating system remains tinymacOS.
