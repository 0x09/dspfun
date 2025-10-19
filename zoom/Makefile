PREFIX ?= /usr/local

fftw := fftw3$(subst L,l,$(subst D,,$(subst F,f,$(COEFF_PRECISION))))

CC ?= cc
CFLAGS := -D_GNU_SOURCE -DCOEFF_PRECISION=$(COEFF_PRECISION) -DINTERMEDIATE_PRECISION=$(INTERMEDIATE_PRECISION) -std=c11 -Wno-initializer-overrides -O3 -ffast-math -I../include -DMAGICKWAND_VERSION=$(shell pkg-config --modversion MagickWand | cut -d. -f1) $(shell pkg-config --cflags MagickWand $(fftw) libavcodec libavformat libswscale libavutil) $(CFLAGS)
LDLIBS := $(shell pkg-config --libs MagickWand $(fftw) libavcodec libavformat libswscale libavutil) -lm

vpath %.h ../include
DEPS = precision.h magickwand.h ffapi.h
TOOLS = zoom

all: $(TOOLS)

vpath %.c ../include

../include/ffapi.c: ../include/ffapi.h

zoom: zoom.c ffapi.o

clean:
	rm -f -- $(TOOLS) ffapi.o

install: all
	install $(TOOLS) $(PREFIX)/bin/

uninstall:
	rm -f -- $(addprefix $(PREFIX)/bin/, $(TOOLS))

.PHONY: all clean install uninstall
