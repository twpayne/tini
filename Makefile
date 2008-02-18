PREFIX=/usr/local
DEVICE=/dev/ttyS0

CC=gcc
CFLAGS=-O2 -Wall -Wno-unused -DDEVICE=\"$(DEVICE)\"

SRCS=tini.c flytec.c regexp.c
HEADERS=tini.h
OBJS=$(SRCS:%.c=%.o)
BINS=tini
DOCS=README COPYING

.PHONY: all clean setgidinstall install tarball

all: $(BINS)

tarball:
	mkdir tini-$(VERSION)
	cp Makefile $(SRCS) $(HEADERS) $(DOCS) tini-$(VERSION)
	tar -czf tini-$(VERSION).tar.gz tini-$(VERSION)
	rm -Rf tini-$(VERSION)

setgidinstall: install
	@echo "  CHGRP   tini"
	@chgrp --reference=$(DEVICE) $(PREFIX)/bin/tini
	@echo "  CHMOD   tini"
	@chmod g+s $(PREFIX)/bin/tini

install: $(BINS)
	@echo "  INSTALL tini"
	@mkdir -p $(PREFIX)/bin
	@cp tini $(PREFIX)/bin/tini

tini: $(OBJS)

clean:
	@echo "  CLEAN   $(BINS) $(OBJS)"
	@rm -f $(BINS) $(OBJS)

%.o: %.c $(HEADERS)
	@echo "  CC      $<"
	@$(CC) -c -o $@ $(CFLAGS) $<

%: %.o
	@echo "  LD      $<"
	@$(CC) -o $@ $(CFLAGS) $^
