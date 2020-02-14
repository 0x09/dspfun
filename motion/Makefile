CC ?= cc
CFLAGS := -D_GNU_SOURCE -std=c11 -Ofast -Wno-initializer-overrides -I../include $(CFLAGS)
LIBS := -lavcodec -lavformat -lswscale -lavutil -lm

all: motion rotate transcode

ffapi.o: ../include/ffapi.c
	$(CC) $(CFLAGS) -c -o $@ $+

motion: motion.c ffapi.o
	$(CC) $(CFLAGS) -o $@ $+ $(shell pkg-config --libs fftw3f) $(LIBS)

rotate: rotate.c ffapi.o
	$(CC) $(CFLAGS) -o $@ $+ $(LIBS)

transcode: transcode.c ffapi.o
	$(CC) $(CFLAGS) -o $@ $+ $(LIBS)

clean:
	rm -f motion rotate transcode ffapi.o

.PHONY: all clean
