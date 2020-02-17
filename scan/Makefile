PREFIX ?= /usr/local

CC ?= cc
CFLAGS := -DPRECISION=F -D_GNU_SOURCE -Wno-initializer-overrides -std=c11 -Ofast -flto -I../include $(CFLAGS) -DMAGICKWAND_VERSION=$(shell pkg-config --modversion MagickWand | cut -d. -f1)
LIBS := -lavcodec -lavformat -lswscale -lavutil -lm $(shell pkg-config --cflags --libs fftw3f MagickWand)

TOOLS = scan

all: $(TOOLS)

ffapi.o: ../include/ffapi.c
	$(CC) $(CFLAGS) -c -o $@ $+

speclib.o: ../include/speclib.c
	$(CC) $(CFLAGS) -c -o $@ $+

scan: scan.c ffapi.o speclib.o scan_context.o scan_methods.o scan_precomputed.o
	$(CC) $(CFLAGS) -o $@ $+ $(LIBS)

clean:
	rm -f scan scan_context.o scan_methods.o scan_precomputed.o speclib.o ffapi.o

install: all
	install $(TOOLS) $(PREFIX)/bin/

uninstall:
	rm -f -- $(addprefix $(PREFIX)/bin/, $(TOOLS))

.PHONY: all clean install uninstall
