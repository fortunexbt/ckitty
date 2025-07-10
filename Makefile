CC = cc
CFLAGS = -Wall -pedantic
LDFLAGS = -lncurses
LDFLAGS_V3 = -lncurses -lm
SRCDIR = src
TARGETS = ckitty ckitty_v2 ckitty_v3
PREFIX = /usr/local

all: $(TARGETS)

ckitty: $(SRCDIR)/ckitty.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@

ckitty_v2: $(SRCDIR)/ckitty_v2.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@

ckitty_v3: $(SRCDIR)/ckitty_v3.c
	$(CC) $(CFLAGS) $< $(LDFLAGS_V3) -o $@

install: ckitty_v3
	install -Dm755 ckitty_v3 $(PREFIX)/bin/ckitty

uninstall:
	rm -f $(PREFIX)/bin/ckitty

clean:
	rm -f $(TARGETS)

.PHONY: all install uninstall clean