COEFF_PRECISION ?= D
INTERMEDIATE_PRECISION ?= L

PREFIX ?= /usr/local

fftw := fftw3$(subst L,l,$(subst D,,$(subst F,f,$(COEFF_PRECISION))))

CC ?= cc
CFLAGS := -DCOEFF_PRECISION=$(COEFF_PRECISION) -DINTERMEDIATE_PRECISION=$(INTERMEDIATE_PRECISION) -std=c11 -O3 -ffast-math -I../include/ -D_GNU_SOURCE -DMAGICKWAND_VERSION=$(shell pkg-config --modversion MagickWand | cut -d. -f1) $(CFLAGS)
LIBS := $(shell pkg-config --cflags --libs MagickWand $(fftw)) -lm

DEPS = spec.h ../include/precision.h ../include/keyed_enum.h ../include/assoc.h
TOOLS = spec ispec

all: $(TOOLS)

spec: spec.c $(DEPS)
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

ispec: ispec.c $(DEPS)
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f -- $(TOOLS)

install: all
	install $(TOOLS) $(PREFIX)/bin/

uninstall:
	rm -f -- $(addprefix $(PREFIX)/bin/, $(TOOLS))

.PHONY: all clean install uninstall
