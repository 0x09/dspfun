COEFF_PRECISION ?= F
INTERMEDIATE_PRECISION ?= L

fftw := fftw3$(subst L,l,$(subst D,,$(subst F,f,$(COEFF_PRECISION))))

PREFIX ?= /usr/local

CFLAGS := -D_GNU_SOURCE -std=c11 -O3 -ffast-math -Wno-initializer-overrides -I../include -DCOEFF_PRECISION=$(COEFF_PRECISION) -DINTERMEDIATE_PRECISION=$(INTERMEDIATE_PRECISION) $(shell pkg-config --cflags libavcodec libavformat libswscale libavutil) $(CFLAGS)
LDLIBS := $(shell pkg-config --libs libavcodec libavformat libswscale libavutil) -lm $(LDLIBS)

TOOLS = motion rotate transcode

all: $(TOOLS)

vpath %.c ../include

../include/ffapi.c: ../include/ffapi.h

motion: LDLIBS := $(shell pkg-config --cflags --libs $(fftw)) -l$(fftw)_threads -lpthread $(LDLIBS)
motion: motion.c ffapi.o

rotate: rotate.c ffapi.o

transcode: transcode.c ffapi.o

clean:
	$(RM) $(TOOLS) ffapi.o

install: all
	install $(TOOLS) $(PREFIX)/bin/

uninstall:
	$(RM) -- $(addprefix $(PREFIX)/bin/, $(TOOLS))

.PHONY: all clean install uninstall
