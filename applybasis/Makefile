PREFIX ?= /usr/local

fftw := fftw3$(subst L,l,$(subst D,,$(subst F,f,$(COEFF_PRECISION))))

CC ?= cc
CFLAGS := -DCOEFF_PRECISION=$(COEFF_PRECISION) -DINTERMEDIATE_PRECISION=$(INTERMEDIATE_PRECISION) -std=c11 -Ofast -DMAGICKWAND_VERSION=$(shell pkg-config --modversion MagickWand | cut -d. -f1) $(CFLAGS)
LIBS := -I../include $(shell pkg-config --cflags --libs MagickWand) -lm

DEPS = ../include/precision.h
TOOLS = applybasis genbasis draw

all: $(TOOLS)

applybasis: applybasis.c $(DEPS)
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

genbasis: genbasis.c $(DEPS)
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

draw: draw.c $(DEPS)
	$(CC) $(CFLAGS) -o $@ $< $(shell pkg-config --cflags --libs $(fftw)) $(LIBS)

clean:
	rm -f -- $(TOOLS)

install: all
	install $(TOOLS) $(PREFIX)/bin/

uninstall:
	rm -f -- $(addprefix $(PREFIX)/bin/, $(TOOLS))

.PHONY: all clean install uninstall
