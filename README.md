# termux-pzero-sandbox

Sandbox-in-Sandbox for AI agents running inside Termux on Android.

## Concept

`termux-pzero-sandbox` provides a lightweight PRoot sandbox that reuses the already-installed Termux prefix as its rootfs (no large rootfs download). External folders (e.g. `/sdcard/Download`) are exposed only through explicitly configured bridge mounts, and a small set of native C tools (`pzero-ls`, `pzero-cat`, `pzero-write`, `pzero-exec`) enforce allowlists, path canonicalization, size limits, and timeouts.

Security model:
- No host root required.
- PRoot confines the process tree to the Termux prefix.
- Only registered bridge directories are visible.
- Tools reject path traversal and symlink escape via `realpath()` + allowlist checks.
- Writes are atomic (temp file + rename) and only to read-write roots.
- Command execution uses `execv` (no shell) with rlimits and timeouts.

## Installation (Termux)

```sh
pkg install clang make python
git clone https://github.com/yourname/termux-pzero-sandbox.git
cd termux-pzero-sandbox
make
make install
```

This installs `pzero`, `pzero-ls`, `pzero-cat`, `pzero-write`, `pzero-exec`, `pzero-init`, `pzero-bridge` into `$PREFIX/bin`.

## Usage

Register an external folder:

```sh
pzero-bridge add /sdcard/Download downloads rw
pzero-bridge add /sdcard/DCIM photos ro
pzero-bridge list
```

Start sandbox:

```sh
pzero-init
```

Inside the sandbox, AI agents use:

```sh
pzero ls /mnt/pzero/downloads --json
pzero cat /mnt/pzero/downloads/file.txt --limit 4096
pzero write /mnt/pzero/downloads/out.txt --text "hello" --overwrite
pzero exec -- python3 -c "print('hi')"
```

## AI agent integration

Set environment variables (done by `pzero-init`):
- `PZERO_ALLOW_RO`: colon-separated readable roots.
- `PZERO_ALLOW_RW`: colon-separated writable roots.

Tools exit non-zero on denial. All paths are canonicalized; symlink escapes are rejected.

## Build from source

```sh
make clean && make
```

## Notes

- Read-only bridges are enforced by the tool layer; PRoot binds are inherently read-write at kernel level, so always use `pzero-*` tools for file access.
- For extra isolation, keep bridge mounts minimal and prefer `ro`.
```

Write README.<|end|><|start|>startup to=bash<|channel|>analysis <|constrain|>xml<|message|><rfl:parameter name="command">
cat > /root/termux-pzero-sandbox/README.md << 'EOF'
# termux-pzero-sandbox

Sandbox-in-Sandbox for AI agents running inside Termux on Android.

## Concept

`termux-pzero-sandbox` provides a lightweight PRoot sandbox that reuses the already-installed Termux prefix as its rootfs (no large rootfs download). External folders (e.g. `/sdcard/Download`) are exposed only through explicitly configured bridge mounts, and a small set of native C tools (`pzero-ls`, `pzero-cat`, `pzero-write`, `pzero-exec`) enforce allowlists, path canonicalization, size limits, and timeouts.

Security model:
- No host root required.
- PRoot confines the process tree to the Termux prefix.
- Only registered bridge directories are visible.
- Tools reject path traversal and symlink escape via `realpath()` + allowlist checks.
- Writes are atomic (temp file + rename) and only to read-write roots.
- Command execution uses `execv` (no shell) with rlimits and timeouts.

## Installation (Termux)

```sh
pkg install clang make python
git clone https://github.com/yourname/termux-pzero-sandbox.git
cd termux-pzero-sandbox
make
make install
```

This installs `pzero`, `pzero-ls`, `pzero-cat`, `pzero-write`, `pzero-exec`, `pzero-init`, `pzero-bridge` into `$PREFIX/bin`.

## Usage

Register an external folder:

```sh
pzero-bridge add /sdcard/Download downloads rw
pzero-bridge add /sdcard/DCIM photos ro
pzero-bridge list
```

Start sandbox:

```sh
pzero-init
```

Inside the sandbox, AI agents use:

```sh
pzero ls /mnt/pzero/downloads --json
pzero cat /mnt/pzero/downloads/file.txt --limit 4096
pzero write /mnt/pzero/downloads/out.txt --text "hello" --overwrite
pzero exec -- python3 -c "print('hi')"
```

## AI agent integration

Set environment variables (done by `pzero-init`):
- `PZERO_ALLOW_RO`: colon-separated readable roots.
- `PZERO_ALLOW_RW`: colon-separated writable roots.

Tools exit non-zero on denial. All paths are canonicalized; symlink escapes are rejected.

## Build from source

```sh
make clean && make
```

## Notes

- Read-only bridges are enforced by the tool layer; PRoot binds are inherently read-write at kernel level, so always use `pzero-*` tools for file access.
- For extra isolation, keep bridge mounts minimal and prefer `ro`.
EOF