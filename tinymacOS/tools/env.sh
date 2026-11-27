# Source this file from the project root. TOOLROOT points to a Debian extraction prefix (optional).
if [ -n "${TOOLROOT:-}" ]; then
 export PATH="$TOOLROOT/usr/bin:$TOOLROOT/usr/sbin:$PATH" LD_LIBRARY_PATH="$TOOLROOT/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
export EFIINC="${EFIINC:-${TOOLROOT:-}/usr/include/efi}" EFILIB="${EFILIB:-${TOOLROOT:-}/usr/lib}"
# Keep generated media outside the source workspace. TMC_BUILD is an absolute path.
export TMC_BUILD="${TMC_BUILD:-/var/tmp/tinymacos-$(id -u)/10.2.6}"
export TMC_ISO=tinymacOS-10.2.6-x86_64.iso
