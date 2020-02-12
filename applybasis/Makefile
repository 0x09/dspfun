CC ?= cc
CFLAGS := -std=c11 -Ofast -DMAGICKWAND_VERSION=$(shell pkg-config --modversion MagickWand | cut -d. -f1) $(CFLAGS)
LIBS := -I../include $(shell pkg-config --cflags --libs MagickWand) -lm

all: applybasis genbasis draw

applybasis: applybasis.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

genbasis: genbasis.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

draw: draw.c
	$(CC) $(CFLAGS) -o $@ $< $(shell pkg-config --libs fftw3f) $(LIBS)

clean:
	rm -f draw genbasis applybasis

.PHONY: all clean
