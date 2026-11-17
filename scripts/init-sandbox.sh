#!/usr/bin/env sh
# Initialize and enter the termux-pzero-sandbox (no rootfs download).
set -eu

PREFIX="${PREFIX:-/data/data/com.termux/files/usr}"
ROOTFS="${PZERO_ROOTFS:-/data/data/com.termux/files}"
HOME_DIR="${HOME:-/data/data/com.termux/files/home}"
CONFIG_DIR="$HOME_DIR/.config/pzero"
BRIDGES="$CONFIG_DIR/bridges.conf"
ALLOWLIST="$CONFIG_DIR/allowlist"

mkdir -p "$CONFIG_DIR"
touch "$BRIDGES" "$ALLOWLIST"

# Build allowlist from bridges + defaults.
: > "$ALLOWLIST"
printf '%s rw\n' "$HOME_DIR" >> "$ALLOWLIST"
printf '%s rw\n' "/tmp" >> "$ALLOWLIST"
for d in /usr /bin /etc /lib /share /var; do
  printf '%s ro\n' "$d" >> "$ALLOWLIST"
done

# Prepare bridge mount points and allowlist entries.
BRIDGE_ARGS=""
while read -r host_path guest_name mode extra || [ -n "$host_path" ]; do
  # skip comments/blank
  case "$host_path" in \#*|"") continue;; esac
  [ -n "$guest_name" ] && [ -n "$mode" ] || continue
  [ -d "$host_path" ] || { echo "warning: bridge source missing: $host_path" >&2; continue; }
  guest_path="/mnt/pzero/$guest_name"
  mkdir -p "$ROOTFS$guest_path"
  BRIDGE_ARGS="$BRIDGE_ARGS -b $host_path:$guest_path"
  printf '%s %s\n' "$guest_path" "$mode" >> "$ALLOWLIST"
done < "$BRIDGES"

# Standard Termux/proot bindings (reuse installed prefix as rootfs).
ARGS="--kill-on-exit"
# Android system dirs
for d in /system /vendor /data /apex; do
  [ -d "$d" ] && ARGS="$ARGS -b $d:$d"
done
[ -e /linkerconfig/ld.config.txt ] && ARGS="$ARGS -b /linkerconfig/ld.config.txt:/linkerconfig/ld.config.txt"
[ -f /property_contexts ] && ARGS="$ARGS -b /property_contexts:/property_contexts"
# dev/proc
ARGS="$ARGS -b /dev:/dev -b /proc:/proc"
# Termux hierarchy
ARGS="$ARGS -b $PREFIX:/usr"
for f in bin etc lib share tmp var; do
  ARGS="$ARGS -b $PREFIX/$f:/$f"
done
# storage (optional, read via bridges preferred)
[ -d /storage ] && ARGS="$ARGS -b /storage:/storage"
# bridges
ARGS="$ARGS $BRIDGE_ARGS"

# Export security env for tools.
export PZERO_ALLOW_RO=""
export PZERO_ALLOW_RW=""
# Build from allowlist file instead; tools read file. But env override simpler:
RO_LIST=""
RW_LIST=""
while read -r path mode extra || [ -n "$path" ]; do
  case "$path" in \#*|"") continue;; esac
  [ -n "$mode" ] || continue
  if [ -z "$RO_LIST" ]; then RO_LIST="$path"; else RO_LIST="$RO_LIST:$path"; fi
  if [ "$mode" = "rw" ]; then
    if [ -z "$RW_LIST" ]; then RW_LIST="$path"; else RW_LIST="$RW_LIST:$path"; fi
  fi
done < "$ALLOWLIST"
export PZERO_ALLOW_RO="$RO_LIST"
export PZERO_ALLOW_RW="$RW_LIST"
export PZERO_ROOTFS="$ROOTFS"

# Ensure sandbox home exists in rootfs.
if [ ! -d "$ROOTFS/home" ]; then mkdir -p "$ROOTFS/home"; fi
USER_NAME="$(basename "$HOME_DIR")"
mkdir -p "$ROOTFS/home/$USER_NAME"
# bind host home if not already under rootfs
case "$HOME_DIR" in
  "$ROOTFS"*) ;;
  *) ARGS="$ARGS -b $HOME_DIR:$HOME_DIR" ;;
esac

cd "$HOME_DIR"
# shell
SHELL_BIN="/bin/sh"
[ -x "$PREFIX/bin/bash" ] && SHELL_BIN="/bin/bash"

if [ $# -gt 0 ]; then
  exec "$PREFIX/bin/proot" $ARGS -0 -r "$ROOTFS" -w "$HOME_DIR" /bin/sh -c "export PZERO_ALLOW_RO='$PZERO_ALLOW_RO'; export PZERO_ALLOW_RW='$PZERO_ALLOW_RW'; export PZERO_ROOTFS='$PZERO_ROOTFS'; export HOME='$HOME_DIR'; cd '$HOME_DIR'; exec \"\$@\"" sh "$@"
else
  exec "$PREFIX/bin/proot" $ARGS -0 -r "$ROOTFS" -w "$HOME_DIR" /bin/sh -c "export PZERO_ALLOW_RO='$PZERO_ALLOW_RO'; export PZERO_ALLOW_RW='$PZERO_ALLOW_RW'; export PZERO_ROOTFS='$PZERO_ROOTFS'; export HOME='$HOME_DIR'; cd '$HOME_DIR'; exec $SHELL_BIN -l"
fi
