COEFF_PRECISION ?= F
INTERMEDIATE_PRECISION ?= L

fftw := fftw3$(subst L,l,$(subst D,,$(subst F,f,$(COEFF_PRECISION))))

PREFIX ?= /usr/local

CC ?= cc
CFLAGS := -D_GNU_SOURCE -std=c11 -Ofast -Wno-initializer-overrides -I../include -DCOEFF_PRECISION=$(COEFF_PRECISION) -DINTERMEDIATE_PRECISION=$(INTERMEDIATE_PRECISION) $(shell pkg-config --cflags libavcodec libavformat libswscale libavutil) $(CFLAGS)
LIBS := $(shell pkg-config --cflags --libs libavcodec libavformat libswscale libavutil) -lm

TOOLS = motion rotate transcode

all: $(TOOLS)

ffapi.o: ../include/ffapi.c
	$(CC) $(CFLAGS) -c -o $@ $+

motion: motion.c ffapi.o
	$(CC) $(CFLAGS) -o $@ $+ $(shell pkg-config --cflags --libs $(fftw)) $(LIBS)

rotate: rotate.c ffapi.o
	$(CC) $(CFLAGS) -o $@ $+ $(LIBS)

transcode: transcode.c ffapi.o
	$(CC) $(CFLAGS) -o $@ $+ $(LIBS)

clean:
	rm -f $(TOOLS) ffapi.o

install: all
	install $(TOOLS) $(PREFIX)/bin/

uninstall:
	rm -f -- $(addprefix $(PREFIX)/bin/, $(TOOLS))

.PHONY: all clean install uninstall
