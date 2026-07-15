CC ?= cc
CPPFLAGS ?=
CFLAGS ?= -Wall -Wextra -Wpedantic -Wconversion -std=c11
LDFLAGS ?=
LDLIBS ?= -lncurses -lm
PREFIX ?= /usr/local

APP = ckitty
TEST = ckitty_test
SRCDIR = src
TESTDIR = tests

APP_OBJS = $(SRCDIR)/ckitty.o $(SRCDIR)/ckitty_core.o
TEST_OBJS = $(TESTDIR)/test_ckitty_core.o $(SRCDIR)/ckitty_core.o
SANITIZER_FLAGS = -fsanitize=address,undefined -fno-omit-frame-pointer

# Homebrew keeps ncurses keg-only on macOS. Auto-detect it when available,
# while still allowing packagers to override CPPFLAGS/LDFLAGS explicitly.
ifeq ($(shell uname -s),Darwin)
NCURSES_PREFIX ?= $(shell brew --prefix ncurses 2>/dev/null)
ifneq ($(strip $(NCURSES_PREFIX)),)
CPPFLAGS += -I$(NCURSES_PREFIX)/include
LDFLAGS += -L$(NCURSES_PREFIX)/lib
endif
endif

CFLAGS += $(SANITIZE)
LDFLAGS += $(SANITIZE)

.PHONY: all check test test-cli sanitize install uninstall clean

all: $(APP)

$(APP): $(APP_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(TEST): $(TEST_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ -lm

$(SRCDIR)/ckitty.o: $(SRCDIR)/ckitty.c $(SRCDIR)/ckitty_core.h
$(SRCDIR)/ckitty_core.o: $(SRCDIR)/ckitty_core.c $(SRCDIR)/ckitty_core.h
$(TESTDIR)/test_ckitty_core.o: $(TESTDIR)/test_ckitty_core.c $(SRCDIR)/ckitty_core.h

%.o:
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

test: $(TEST)
	./$(TEST)

test-cli: $(APP)
	$(TESTDIR)/test_cli.sh ./$(APP)

check: all test test-cli

sanitize:
	$(MAKE) clean
	$(MAKE) SANITIZE="$(SANITIZER_FLAGS)" check

install: $(APP)
	install -d "$(PREFIX)/bin"
	install -m 755 $(APP) "$(PREFIX)/bin/ckitty"

uninstall:
	rm -f "$(PREFIX)/bin/ckitty"

clean:
	rm -f $(APP) $(TEST) $(APP_OBJS) $(TEST_OBJS)
