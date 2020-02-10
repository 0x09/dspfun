CC ?= cc
CFLAGS := -std=c11 -Ofast -I../include/ -D_GNU_SOURCE -DMAGICKWAND_VERSION=$(shell pkg-config --modversion MagickWand | cut -d. -f1) $(CFLAGS)
LIBS := $(shell pkg-config --cflags --libs MagickWand fftw3) -lm

DEPS = spec.h ../include/precision.h ../include/keyed_enum.h ../include/assoc.h

all: spec ispec

spec: spec.c $(DEPS)
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

ispec: ispec.c $(DEPS)
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f spec ispec

.PHONY: all clean
