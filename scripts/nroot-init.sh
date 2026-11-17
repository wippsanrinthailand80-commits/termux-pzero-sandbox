#!/usr/bin/env sh
set -eu

PREFIX="${PREFIX:-/data/data/com.termux/files/usr}"
ROOTFS="${NROOT_ROOTFS:-$HOME/.nroot/rootfs}"

mkdir -p "$ROOTFS"
for d in bin dev etc home lib mnt opt proc sys tmp usr var; do
    mkdir -p "$ROOTFS/$d"
done
mkdir -p "$ROOTFS/usr/local/bin" "$ROOTFS/usr/local/lib"

# minimal shell from Termux (available via /opt/termux)
if [ ! -e "$ROOTFS/bin/sh" ]; then
    ln -sf /opt/termux/bin/sh "$ROOTFS/bin/sh"
fi
if [ ! -e "$ROOTFS/bin/env" ] && [ -x "$PREFIX/bin/env" ]; then
    ln -sf /opt/termux/bin/env "$ROOTFS/bin/env"
fi

# expose a small set of essential tools (clean but usable)
for t in ls cat cp mv rm mkdir echo grep sed awk; do
    if [ -x "$PREFIX/bin/$t" ] && [ ! -e "$ROOTFS/bin/$t" ]; then
        ln -sf "/opt/termux/bin/$t" "$ROOTFS/bin/$t"
    fi
done

ARGS="--kill-on-exit"
for d in /system /vendor /data /apex; do
    [ -d "$d" ] && ARGS="$ARGS -b $d:$d"
done
[ -e /linkerconfig/ld.config.txt ] && ARGS="$ARGS -b /linkerconfig/ld.config.txt:/linkerconfig/ld.config.txt"
ARGS="$ARGS -b /dev:/dev -b /proc:/proc -b /sys:/sys"
# expose Termux as /opt/termux for optional tools
ARGS="$ARGS -b $PREFIX:/opt/termux"
# storage
[ -d /storage ] && ARGS="$ARGS -b /storage:/storage"

# host home (optional) -> /host-home, not /home, to keep rootfs clean
ARGS="$ARGS -b ${HOME}:/host-home"

export NROOT_ROOTFS="$ROOTFS"
export HOME="$ROOTFS/home"
mkdir -p "$ROOTFS/home"
export PATH="/usr/local/bin:/opt/termux/bin:/bin:/usr/bin"

SHELL_BIN="/bin/sh"
if [ -x "$PREFIX/bin/bash" ]; then SHELL_BIN="/bin/bash"; fi

if [ $# -gt 0 ]; then
    exec "$PREFIX/bin/proot" $ARGS -r "$ROOTFS" -w /home /bin/sh -c "export HOME=/home; export PATH=/usr/local/bin:/opt/termux/bin:/bin:/usr/bin; cd /home; exec \"\$@\"" sh "$@"
else
    exec "$PREFIX/bin/proot" $ARGS -r "$ROOTFS" -w /home /bin/sh -c "export HOME=/home; export PATH=/usr/local/bin:/opt/termux/bin:/bin:/usr/bin; cd /home; exec /bin/sh -l"
fi
