CC ?= cc
INSTALL ?= install
PREFIX ?= /usr/local

project = l2flood

LDFLAGS = -lbluetooth

build:
	$(CC) $(CFLAGS) -fopenmp $(project).c $(LDFLAGS) -o $(project)

clean:
	rm -f ./$(project)

install:
	mkdir -p "$(DESTDIR)$(PREFIX)/bin"
	$(INSTALL) ./$(project) "$(DESTDIR)$(PREFIX)/bin/$(project)"

.PHONY: build clean install
