#!/usr/bin/env sh
set -eu
# Install a single static binary into the Nroot rootfs.
# Usage: nroot-install <url> <name>
[ $# -eq 2 ] || { echo "Usage: $0 <url> <name>"; exit 1; }
url="$1"; name="$2"
ROOTFS="${NROOT_ROOTFS:-$HOME/.nroot/rootfs}"
dest="$ROOTFS/usr/local/bin/$name"
mkdir -p "$ROOTFS/usr/local/bin"
echo "Downloading $url -> $dest"
curl -fsSL -o "$dest" "$url"
chmod +x "$dest"
echo "Installed $name into Nroot rootfs"
