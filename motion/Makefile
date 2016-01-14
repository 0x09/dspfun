CC ?= cc
CFLAGS := -std=c11 -Ofast $(CFLAGS)
LIBS := -I../include $(shell pkg-config --libs fftw3f) -lavcodec -lavformat -lswscale -lavutil -lm

all: motion

motion: motion.c ../include/ffapi.c
	$(CC) $(CFLAGS) $(LIBS) -o $@ $+

clean:
	rm -f motion

.PHONY: all clean
