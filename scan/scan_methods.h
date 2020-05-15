/*
 * scan - progressively reconstruct images using various frequency space scans.
 * Copyright 2018 0x09.net.
 */

#ifndef SCAN_METHODS_H
#define SCAN_METHODS_H

#include "precision.h"

#include <stdlib.h>

struct scan_method {
	const char* name;
	void (*scan)(void*, size_t, size_t, size_t, size_t (*)[2]);

	size_t (*limit)(void*, size_t, size_t);
	size_t (*interval)(void*, size_t, size_t, size_t);
	size_t (*max_interval)(void*, size_t, size_t);

	const char* init_args;
	void* (*init)(size_t, size_t, size_t, coeff*, const char*);
	void (*destroy)(void*);
};

#endif
