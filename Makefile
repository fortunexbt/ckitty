CC ?= cc
CPPFLAGS ?=
CFLAGS ?= -Wall -Wextra -Wpedantic -Wconversion -std=c11
LDFLAGS ?=
# macOS exposes wide-character curses through ncurses; Linux ships it as
# ncursesw in the same development package.
ifeq ($(shell uname -s),Darwin)
LDLIBS ?= -lncurses -lm
else
LDLIBS ?= -lncursesw -lm
endif
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

# Sanitizer instrumentation must survive caller-supplied build flags.
override CFLAGS += $(SANITIZE)
override LDFLAGS += $(SANITIZE)

# WebAssembly build: the same ckitty.c, with web_curses.c standing in for
# ncurses and emitting ANSI to a terminal in the page.
EMCC ?= emcc
WEB_SRCS = $(SRCDIR)/ckitty.c $(SRCDIR)/ckitty_core.c $(SRCDIR)/web_curses.c
WEB_OUT = web/public/ckitty.js
EMFLAGS = -O3 -std=c11 -Wall -Wextra -DCKITTY_WEB \
	-sASYNCIFY -sMODULARIZE=1 -sEXPORT_NAME=createCkitty -sINVOKE_RUN=0 \
	-sEXPORTED_RUNTIME_METHODS=callMain -sALLOW_MEMORY_GROWTH=1 \
	-sENVIRONMENT=web -sFILESYSTEM=0 --closure 0

.PHONY: all check test test-cli test-resize sanitize demo install uninstall clean web

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

check: all test test-cli test-resize

test-resize: $(APP)
	python3 tests/test_resize.py ./$(APP)

demo: all
	./tools/make-demo.sh

web: $(WEB_SRCS) $(SRCDIR)/ckitty_core.h $(SRCDIR)/web_curses.h
	$(EMCC) $(EMFLAGS) $(WEB_SRCS) -o $(WEB_OUT)

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
