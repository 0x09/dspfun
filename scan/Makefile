COEFF_PRECISION ?= F
INTERMEDIATE_PRECISION ?= D

fftw := fftw3$(subst L,l,$(subst D,,$(subst F,f,$(COEFF_PRECISION))))

PREFIX ?= /usr/local

pcdeps = $(fftw) MagickWand libavcodec libavformat libswscale libavutil

CFLAGS := -DCOEFF_PRECISION=$(COEFF_PRECISION) -DINTERMEDIATE_PRECISION=$(INTERMEDIATE_PRECISION) -D_GNU_SOURCE -Wno-initializer-overrides -std=c11 -O3 -ffast-math -flto -I../include $(shell pkg-config --cflags $(pcdeps)) $(CFLAGS) -DMAGICKWAND_VERSION=$(shell pkg-config --modversion MagickWand | cut -d. -f1)
LDLIBS := $(shell pkg-config --libs $(pcdeps)) -lm -l$(fftw)_threads -lpthread $(LDLIBS)

TOOLS = scan

all: $(TOOLS)

vpath %.c ../include

../include/ffapi.c: ../include/ffapi.h
../include/speclib.c: ../include/speclib.h

scan: scan.c ffapi.o speclib.o scan_context.o scan_methods.o scan_precomputed.o

clean:
	$(RM) scan scan_context.o scan_methods.o scan_precomputed.o speclib.o ffapi.o

install: all
	install $(TOOLS) $(PREFIX)/bin/

uninstall:
	$(RM) -- $(addprefix $(PREFIX)/bin/, $(TOOLS))

.PHONY: all clean install uninstall
