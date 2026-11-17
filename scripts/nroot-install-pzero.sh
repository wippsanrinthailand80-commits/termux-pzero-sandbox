#!/usr/bin/env sh
set -eu
ROOTFS="${NROOT_ROOTFS:-$HOME/.nroot/rootfs}"
PREFIX="${PREFIX:-/data/data/com.termux/files/usr}"
mkdir -p "$ROOTFS/usr/local/bin"
for b in pzero pzero-ls pzero-cat pzero-write pzero-exec pzero-init pzero-bridge; do
    if [ -x "$PREFIX/bin/$b" ]; then
        cp "$PREFIX/bin/$b" "$ROOTFS/usr/local/bin/$b"
        chmod +x "$ROOTFS/usr/local/bin/$b"
    fi
done
echo "Installed pzero tools into Nroot rootfs"
