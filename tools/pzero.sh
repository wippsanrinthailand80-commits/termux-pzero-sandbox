#!/bin/sh
# thin dispatcher: exec the matching native tool without forking a subprocess
set -u

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

LOGDIR="${HOME}/.config/pzero"
mkdir -p "$LOGDIR" 2>/dev/null || true
LOG="$LOGDIR/audit.log"
_ts() { date '+%Y-%m-%dT%H:%M:%S' 2>/dev/null || echo unknown; }
case "$CMD" in
    ls|cat|write|exec)
        "$DIR/pzero-$CMD" "$@"
        _rc=$?
        printf '%s pzero %s %s rc=%s\n' "$(_ts)" "$CMD" "$*" "$_rc" >> "$LOG" 2>/dev/null || true
        exit $_rc
        ;;
    bridges|bridge)
        "$DIR/pzero-bridge" "$@"
        _rc=$?
        printf '%s pzero-bridge %s rc=%s\n' "$(_ts)" "$*" "$_rc" >> "$LOG" 2>/dev/null || true
        exit $_rc
        ;;
    init)
        "$DIR/pzero-init" "$@"
        _rc=$?
        printf '%s pzero-init %s rc=%s\n' "$(_ts)" "$*" "$_rc" >> "$LOG" 2>/dev/null || true
        exit $_rc
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
