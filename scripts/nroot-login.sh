#!/usr/bin/env sh
set -eu

PREFIX="${PREFIX:-/data/data/com.termux/files/usr}"
BASE="${NROOT_ROOTFS:-$HOME/.nroot}"
mode="${1:-default}"

if [ "$mode" = "default" ]; then
    exec nroot-init
fi

ROOTFS="$BASE/distros/$mode"
if [ ! -d "$ROOTFS" ]; then
    echo "nroot-login: distro rootfs not found: $ROOTFS" >&2
    echo "Create it first, e.g.:" >&2
    echo "  proot-distro install $mode && mv \$PREFIX/var/lib/proot-distro/installed-rootfs/$mode $ROOTFS" >&2
    exit 1
fi

mkdir -p "$ROOTFS/home" "$ROOTFS/root" 2>/dev/null || true

ARGS="--kill-on-exit"
for d in /system /vendor /data /apex; do
    [ -d "$d" ] && ARGS="$ARGS -b $d:$d"
done
[ -e /linkerconfig/ld.config.txt ] && ARGS="$ARGS -b /linkerconfig/ld.config.txt:/linkerconfig/ld.config.txt"
ARGS="$ARGS -b /dev:/dev -b /proc:/proc -b /sys:/sys"
ARGS="$ARGS -b $PREFIX:/opt/termux"
[ -d /storage ] && ARGS="$ARGS -b /storage:/storage"
ARGS="$ARGS -b ${HOME}:/host-home"

export NROOT_ROOTFS="$BASE"
export HOME="/root"

SHELL_BIN="/bin/sh"
[ -x "$ROOTFS/bin/bash" ] && SHELL_BIN="/bin/bash"

if [ $# -gt 1 ]; then
    shift
    exec "$PREFIX/bin/proot" $ARGS -r "$ROOTFS" -w /root /bin/sh -c "export HOME=/root; cd /root; exec \"\$@\"" sh "$@"
else
    exec "$PREFIX/bin/proot" $ARGS -r "$ROOTFS" -w /root /bin/sh -c "export HOME=/root; cd /root; exec $SHELL_BIN -l"
fi
