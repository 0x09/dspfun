COEFF_PRECISION ?= D
INTERMEDIATE_PRECISION ?= L

PREFIX ?= /usr/local

fftw := fftw3$(subst L,l,$(subst D,,$(subst F,f,$(COEFF_PRECISION))))

CFLAGS := -DCOEFF_PRECISION=$(COEFF_PRECISION) -DINTERMEDIATE_PRECISION=$(INTERMEDIATE_PRECISION) -std=c11 -O3 -ffast-math -I../include/ -D_GNU_SOURCE -DMAGICKWAND_VERSION=$(shell pkg-config --modversion MagickWand | cut -d. -f1) $(shell pkg-config --cflags MagickWand $(fftw)) $(CFLAGS)
LDLIBS := $(shell pkg-config --libs MagickWand $(fftw)) -lm $(LDLIBS)

DEPS = spec.h ../include/precision.h ../include/keyed_enum.h ../include/assoc.h
TOOLS = spec ispec

all: $(TOOLS)

spec.o: $(DEPS)
ispec.o: $(DEPS)

clean:
	$(RM) $(TOOLS) spec.o ispec.o

install: all
	install $(TOOLS) $(PREFIX)/bin/

uninstall:
	$(RM) $(addprefix $(PREFIX)/bin/, $(TOOLS))

.PHONY: all clean install uninstall
