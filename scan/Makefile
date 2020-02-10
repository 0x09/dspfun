CC ?= cc
CFLAGS := -DPRECISION=F -Wno-initializer-overrides -std=c11 -Ofast -flto -I../include $(CFLAGS) -DMAGICKWAND_VERSION=$(shell pkg-config --modversion MagickWand | cut -d. -f1)
LIBS := -lavcodec -lavformat -lswscale -lavutil -lm $(shell pkg-config --cflags --libs fftw3f MagickWand)

all: scan

ffapi.o: ../include/ffapi.c
	$(CC) $(CFLAGS) -c -o $@ $+

speclib.o: ../include/speclib.c
	$(CC) $(CFLAGS) -c -o $@ $+

scan: scan.c ffapi.o speclib.o scan_context.o scan_methods.o scan_precomputed.o
	$(CC) $(CFLAGS) -o $@ $+ $(LIBS)

clean:
	rm -f scan scan_context.o scan_methods.o precomputed_scan.o speclib.o ffapi.o

.PHONY: all clean
