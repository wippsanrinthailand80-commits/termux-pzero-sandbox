PREFIX ?= $(if $(TERMUX_PREFIX),$(TERMUX_PREFIX),/usr/local)
CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Wno-format-truncation -std=c99 -Iinclude
LDFLAGS ?=

BINS = pzero-ls pzero-cat pzero-write pzero-exec

all: $(BINS)

pzero-ls: src/pzero-ls.c src/pzero_common.c
	$(CC) $(CFLAGS) -o $@ src/pzero-ls.c src/pzero_common.c $(LDFLAGS)

pzero-cat: src/pzero-cat.c src/pzero_common.c
	$(CC) $(CFLAGS) -o $@ src/pzero-cat.c src/pzero_common.c $(LDFLAGS)

pzero-write: src/pzero-write.c src/pzero_common.c
	$(CC) $(CFLAGS) -o $@ src/pzero-write.c src/pzero_common.c $(LDFLAGS)

pzero-exec: src/pzero-exec.c src/pzero_common.c
	$(CC) $(CFLAGS) -o $@ src/pzero-exec.c src/pzero_common.c $(LDFLAGS)

install: all
	mkdir -p $(PREFIX)/bin
	install -m 0755 $(BINS) $(PREFIX)/bin/
	install -m 0755 tools/pzero.sh $(PREFIX)/bin/pzero
	install -m 0755 scripts/init-sandbox.sh $(PREFIX)/bin/pzero-init
	install -m 0755 scripts/pzero-bridge.sh $(PREFIX)/bin/pzero-bridge
	install -m 0755 scripts/nroot-init.sh $(PREFIX)/bin/nroot-init
	install -m 0755 scripts/nroot-install.sh $(PREFIX)/bin/nroot-install

clean:
	rm -f $(BINS)
.PHONY: all install clean
