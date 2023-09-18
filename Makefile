CC ?= cc
INSTALL ?= install
PREFIX ?= /usr/local

project = l2flood

LDFLAGS = -lbluetooth

parallel:
	$(CC) $(project).c -fopenmp $(LDFLAGS) -o $(project)

serial:
	$(CC) $(project).c $(LDFLAGS) -o $(project)

clean:
	rm -f ./$(project)

install:
	mkdir -p "$(DESTDIR)$(PREFIX)/bin"
	$(INSTALL) ./$(project) "$(DESTDIR)$(PREFIX)/bin/$(project)"

.PHONY: parallel serial clean install
