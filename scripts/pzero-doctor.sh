#!/usr/bin/env sh
set -u
PREFIX="${PREFIX:-/data/data/com.termux/files/usr}"
fail=0
check() { if [ "$2" = "ok" ]; then echo "OK: $1"; else echo "FAIL: $1"; fail=1; fi; }

[ -x "$PREFIX/bin/proot" ] && check "proot installed" ok || check "proot installed" fail
for b in pzero pzero-ls pzero-cat pzero-write pzero-exec pzero-init pzero-bridge; do
    [ -x "$PREFIX/bin/$b" ] && check "$b installed" ok || check "$b installed" fail
done
[ -f "$HOME/.config/pzero/allowlist" ] && check "allowlist present" ok || check "allowlist present" fail
if [ -f "$HOME/.config/pzero/bridges.conf" ]; then
    while read -r hp name mode; do
        [ -z "$hp" ] && continue
        case "$hp" in \#*) continue;; esac
        [ -d "$hp" ] && check "bridge $name ($hp)" ok || check "bridge $name ($hp)" fail
    done < "$HOME/.config/pzero/bridges.conf"
fi
[ -d "$HOME/.nroot/rootfs" ] && check "nroot rootfs present" ok || echo "INFO: nroot rootfs not created (optional)"
exit $fail
