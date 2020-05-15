/*
 * scan - progressively reconstruct images using various frequency space scans.
 * Copyright 2018 0x09.net.
 */

#ifndef SCAN_PRECOMPUTED_H
#define SCAN_PRECOMPUTED_H

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

struct scan_precomputed {
	size_t limit;
	size_t* intervals;
	size_t (**scans)[2];
};

struct scan_precomputed* scan_precomputed_unserialize(FILE* f);
void scan_precomputed_serialize_coordinate(struct scan_precomputed* p, FILE* f);
void scan_precomputed_serialize_index(struct scan_precomputed* p, FILE* f);
void scan_precomputed_destroy(struct scan_precomputed*);

void scan_precomputed_dimensions(struct scan_precomputed*, size_t* restrict width, size_t* restrict height);
bool scan_precomputed_add_coord(struct scan_precomputed*, size_t index, size_t x, size_t y);

#endif
