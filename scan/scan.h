/*
 * scan - progressively reconstruct images using various frequency space scans.
 */

#ifndef SCAN_H
#define SCAN_H

#include "precision.h"
#include "keyed_enum.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#define scan_serialization(X,type)\
	X(type,index)\
	X(type,coordinate)
enum_public_gen(scan_serialization)

struct scan_method;
struct scan_method* scan_methods();
struct scan_method* scan_method_find(const char* name);
struct scan_method* scan_method_find_prefix(const char* prefix);
struct scan_method* scan_method_next(struct scan_method*);
const char* scan_method_name(struct scan_method*);
const char* scan_method_options(struct scan_method*);

struct scan_context;
struct scan_context* scan_init(struct scan_method*, size_t width, size_t height, size_t channels, coeff* coeffs, const char* options);
void scan_destroy(struct scan_context*);

void scan(struct scan_context*, size_t i, size_t (*coords)[2]);
size_t scan_interval(struct scan_context*, size_t i);
size_t scan_limit(struct scan_context*);
size_t scan_max_interval(struct scan_context*);
size_t scan_method(struct scan_context*);
void scan_serialize(struct scan_context*, FILE*, enum scan_serialization);

#endif
