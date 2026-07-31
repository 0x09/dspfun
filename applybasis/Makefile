COEFF_PRECISION ?= D
INTERMEDIATE_PRECISION ?= L

PREFIX ?= /usr/local

fftw := fftw3$(subst L,l,$(subst D,,$(subst F,f,$(COEFF_PRECISION))))

CFLAGS := -D_GNU_SOURCE -DCOEFF_PRECISION=$(COEFF_PRECISION) -DINTERMEDIATE_PRECISION=$(INTERMEDIATE_PRECISION) -std=c11 -O3 -ffast-math -I../include -DMAGICKWAND_VERSION=$(shell pkg-config --modversion MagickWand | cut -d. -f1) $(shell pkg-config --cflags MagickWand) $(CFLAGS)
LDLIBS := $(shell pkg-config --libs MagickWand) -lm $(LDLIBS)

DEPS = ../include/precision.h
TOOLS = applybasis genbasis draw

all: $(TOOLS)

applybasis.o: $(DEPS)
genbasis.o: $(DEPS)
draw: LDLIBS := $(shell pkg-config --cflags --libs $(fftw)) $(LDLIBS)

clean:
	$(RM) $(TOOLS) *.o

install: all
	install $(TOOLS) $(PREFIX)/bin/

uninstall:
	$(RM) $(addprefix $(PREFIX)/bin/, $(TOOLS))

.PHONY: all clean install uninstall
