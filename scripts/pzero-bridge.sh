#!/usr/bin/env sh
set -eu

CONFIG_DIR="${HOME:-/tmp}/.config/pzero"
BRIDGES="$CONFIG_DIR/bridges.conf"
mkdir -p "$CONFIG_DIR"
touch "$BRIDGES"

usage() {
  echo "Usage: pzero-bridge {add|list|remove} [args]"
  echo "  add <host_path> <name> <ro|rw>"
  echo "  list"
  echo "  remove <name>"
}

cmd="${1:-}"
case "$cmd" in
  add)
    [ $# -eq 4 ] || { usage; exit 1; }
    host_path="$2"; name="$3"; mode="$4"
    case "$mode" in ro|rw) ;; *) echo "mode must be ro or rw"; exit 1;; esac
    # validate path
    case "$host_path" in /*) ;; *) echo "host_path must be absolute"; exit 1;; esac
    [ -d "$host_path" ] || { echo "host_path not a directory"; exit 1; }
    # validate name
    case "$name" in *[!A-Za-z0-9._-]*) echo "invalid name"; exit 1;; esac
    # remove existing entry with same name
    tmp="$(mktemp)"
    grep -v "^[^ ]* ${name} " "$BRIDGES" > "$tmp" 2>/dev/null || true
    mv "$tmp" "$BRIDGES"
    printf '%s %s %s\n' "$host_path" "$name" "$mode" >> "$BRIDGES"
    echo "bridge added: $host_path -> /mnt/pzero/$name ($mode)"
    ;;
  list)
    if [ ! -s "$BRIDGES" ]; then echo "(no bridges)"; exit 0; fi
    cat "$BRIDGES"
    ;;
  remove)
    [ $# -eq 2 ] || { usage; exit 1; }
    name="$2"
    tmp="$(mktemp)"
    grep -v "^[^ ]* ${name} " "$BRIDGES" > "$tmp" 2>/dev/null || true
    mv "$tmp" "$BRIDGES"
    echo "bridge removed: $name"
    ;;
  *)
    usage; exit 1;;
esac
