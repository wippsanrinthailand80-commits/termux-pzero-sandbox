#!/bin/sh
# thin dispatcher: exec the matching native tool without forking a subprocess
set -eu

SELF="$0"
case "$SELF" in
  */*) DIR="${SELF%/*}" ;;
  *) SELF="$(command -v -- "$SELF" 2>/dev/null || true)"; DIR="${SELF%/*}" ;;
esac
if [ -z "${DIR:-}" ] || [ ! -d "$DIR" ]; then
    # fall back to Termux prefix / common locations
    for d in "/data/data/com.termux/files/usr/bin" "$PREFIX/bin" /usr/local/bin /usr/bin; do
        if [ -x "$d/pzero-ls" ]; then DIR="$d"; break; fi
    done
fi
if [ -z "${DIR:-}" ]; then
    echo "pzero: cannot locate native tools" >&2
    exit 1
fi

CMD="${1:-}"
[ $# -gt 0 ] && shift
case "$CMD" in
    ls|cat|write|exec)
        exec "$DIR/pzero-$CMD" "$@"
        ;;
    bridges|bridge)
        exec "$DIR/pzero-bridge" "$@"
        ;;
    init)
        exec "$DIR/pzero-init" "$@"
        ;;
    -h|--help|help)
        echo "Usage: pzero {ls|cat|write|exec|bridge} [args]"
        ;;
    *)
        echo "pzero: unknown command: $CMD" >&2
        echo "Usage: pzero {ls|cat|write|exec|bridge} [args]" >&2
        exit 1
        ;;
esac
