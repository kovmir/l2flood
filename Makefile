CC ?= cc
PREFIX ?= /usr/local

BLUEZ_LDLIBS := $(shell pkg-config --libs bluez)
BLUEZ_CFLAGS := $(shell pkg-config --cflags bluez)

CFLAGS += -fopenmp $(BLUEZ_CFLAGS)
LDLIBS += $(BLUEZ_LDLIBS)

BUILD_BIN := l2flood

all: build

build: $(BUILD_BIN)

$(BUILD_BIN): main.c
	$(CC) $(LDFLAGS) $(CFLAGS) $^ -o $@ $(LDLIBS)

clean:
	rm -f ./$(BUILD_BIN)

install:
	install -Dm755 "$(DESTDIR)$(PREFIX)/bin/$(BUILD_BIN)"

uninstall:
	rm -f "$(DESTDIR)$(PREFIX)/bin/$(BUILD_BIN)"

.PHONY: build clean install uninstall
