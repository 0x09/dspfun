CC ?= cc
CFLAGS := -std=c11 -Ofast -I../include/ -D_GNU_SOURCE $(CFLAGS)
LIBS := $(shell pkg-config --cflags --libs MagickWand fftw3) -lm

DEPS = spec.h keyed_enum.h precision.h

all: spec ispec

spec: spec.c $(DEPS)
	$(CC) $(CFLAGS) $(LIBS) -o $@ $<

ispec: ispec.c $(DEPS)
	$(CC) $(CFLAGS) $(LIBS) -o $@ $<

clean:
	rm -f spec ispec

.PHONY: all clean
