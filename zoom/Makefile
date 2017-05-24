CC ?= cc
CFLAGS := -std=c11 -Ofast -DMAGICKWAND_VERSION=$(shell pkg-config --modversion MagickWand | cut -d. -f1) $(CFLAGS)
LIBS := -I../include $(shell pkg-config --cflags --libs MagickWand fftw3) -lm

all: zoom

zoom: zoom.c
	$(CC) $(CFLAGS) $(LIBS) -o $@ $<

clean:
	rm -f zoom

.PHONY: all clean
