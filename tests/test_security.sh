#!/bin/sh
set -e
export PZERO_ALLOW_RO="/tmp/scratchpad"
export PZERO_ALLOW_RW="/tmp/scratchpad"
mkdir -p /tmp/scratchpad/t
echo x > /tmp/scratchpad/t/a.txt
# traversal denied
if pzero-cat /tmp/scratchpad/t/../../etc/passwd >/dev/null 2>&1; then echo "FAIL: traversal allowed"; exit 1; fi
# symlink escape denied
ln -sf /etc/passwd /tmp/scratchpad/t/link 2>/dev/null || true
if pzero-cat /tmp/scratchpad/t/link >/dev/null 2>&1; then echo "FAIL: symlink escape allowed"; exit 1; fi
echo "security tests passed"
