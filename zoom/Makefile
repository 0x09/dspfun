PREFIX ?= /usr/local

CC ?= cc
CFLAGS := -std=c11 -Ofast -DMAGICKWAND_VERSION=$(shell pkg-config --modversion MagickWand | cut -d. -f1) $(CFLAGS)
LIBS := -I../include $(shell pkg-config --cflags --libs MagickWand fftw3) -lm

TOOLS = zoom

all: $(TOOLS)

zoom: zoom.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f -- $(TOOLS)

install: all
	install $(TOOLS) $(PREFIX)/bin/

uninstall:
	rm -f -- $(addprefix $(PREFIX)/bin/, $(TOOLS))

.PHONY: all clean install uninstall
