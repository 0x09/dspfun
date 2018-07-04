/*
 * scan - progressively reconstruct images using various frequency space scans.
 * Copyright 2018 0x09.net.
 */

#include "scan_precomputed.h"
#include "scan_methods.h"
#include "scan.h"

#include <stdio.h>

enum_private_gen(scan_serialization)

struct scan_context {
	struct scan_method* method;
	size_t width, height, limit, max_interval;
	void* internal;
};

struct scan_context* scan_init(struct scan_method* method, size_t width, size_t height, size_t channels, coeff* coeffs, const char* scan_options) {
	struct scan_context* ctx = calloc(1,sizeof(*ctx));
	ctx->method = method;
	ctx->width = width;
	ctx->height = height;

	if(method->init && !(ctx->internal = method->init(width,height,channels,coeffs,scan_options))) {
		free(ctx);
		return NULL;
	}

	ctx->limit = method->limit ? method->limit(ctx->internal,ctx->width,ctx->height) : width*height;
	ctx->max_interval = method->max_interval ? method->max_interval(ctx->internal,ctx->width,ctx->height) : width*height/ctx->limit;

	return ctx;
}

void scan_destroy(struct scan_context* ctx) {
	if(ctx->method->destroy)
		ctx->method->destroy(ctx->internal);
	else
		free(ctx->internal);
	free(ctx);
}

void scan(struct scan_context* ctx, size_t i, size_t (*coords)[2]) {
	ctx->method->scan(ctx->internal,ctx->width,ctx->height, i, coords);
}

size_t scan_interval(struct scan_context* ctx, size_t i) {
	return ctx->method->interval ? ctx->method->interval(ctx->internal,ctx->width,ctx->height, i) : ctx->max_interval;
}

size_t scan_limit(struct scan_context* ctx) {
	return ctx->limit;
}

size_t scan_max_interval(struct scan_context* ctx) {
	return ctx->max_interval;
}

struct scan_precomputed* scan_precompute(struct scan_context* ctx) {
	struct scan_precomputed* p = calloc(1,sizeof(*p));
	p->limit = scan_limit(ctx);
	p->scans = malloc(sizeof(*p->scans)*p->limit);
	p->intervals = malloc(sizeof(*p->intervals)*p->limit);
	for(size_t i = 0; i < p->limit; i++) {
		p->intervals[i] = scan_interval(ctx,i);
		p->scans[i] = malloc(sizeof(*p->scans[i])*p->intervals[i]);
		scan(ctx,i,p->scans[i]);
	}
	return p;
}

void scan_serialize(struct scan_context* ctx, FILE* f, enum scan_serialization fmt) {
	struct scan_precomputed* p = scan_precompute(ctx);
	switch(fmt) {
		case scan_serialization_none:
		case scan_serialization_coordinate:
			scan_precomputed_serialize_coordinate(p, f);
			break;
		case scan_serialization_index:
			scan_precomputed_serialize_index(p, f);
			break;
	}
	scan_precomputed_destroy(p);
}
