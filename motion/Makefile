CC ?= cc
CFLAGS := -std=c11 -Ofast -I../include $(CFLAGS)
LIBS := -lavcodec -lavformat -lswscale -lavutil -lm

all: motion rotate transcode

ffapi.o: ../include/ffapi.c
	$(CC) $(CFLAGS) -c -o $@ $+

motion: motion.c ffapi.o
	$(CC) $(CFLAGS) $(shell pkg-config --libs fftw3f) $(LIBS) -o $@ $+

rotate: rotate.c ffapi.o
	$(CC) $(CFLAGS) $(LIBS) -o $@ $+

transcode: transcode.c ffapi.o
	$(CC) $(CFLAGS) $(LIBS) -o $@ $+

clean:
	rm -f motion rotate transcode ffapi.o

.PHONY: all clean
