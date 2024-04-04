COEFF_PRECISION ?= F

fftw := fftw3$(subst L,l,$(subst D,,$(subst F,f,$(COEFF_PRECISION))))

PREFIX ?= /usr/local

pcdeps = $(fftw) MagickWand libavcodec libavformat libswscale libavutil

CC ?= cc
CFLAGS := -DCOEFF_PRECISION=$(COEFF_PRECISION) -DINTERMEDIATE_PRECISION=$(INTERMEDIATE_PRECISION) -D_GNU_SOURCE -Wno-initializer-overrides -std=c11 -Ofast -flto -I../include $(shell pkg-config --cflags $(pcdeps)) $(CFLAGS) -DMAGICKWAND_VERSION=$(shell pkg-config --modversion MagickWand | cut -d. -f1)
LIBS := $(shell pkg-config --libs $(pcdeps)) -lm -l$(fftw)_threads -lpthread

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
