#include "speclib.h"
#include "assoc.h"

#include <math.h>
#include <stdlib.h>

enum_gen_keys(signtype)
enum_gen_table(signtype)
enum_gen_keys(scaletype)
enum_gen_table(scaletype)

#define spec_presets(X)\
	X("abs",    (&(struct spec_params){scaletype_log,   signtype_abs     }))\
	X("shift",  (&(struct spec_params){scaletype_log,   signtype_shift   }))\
	X("flat",   (&(struct spec_params){scaletype_linear,signtype_shift   }))\
	X("signmap",(&(struct spec_params){scaletype_linear,signtype_saturate}))

#define spec_options(X)\
	X("preset",assoc_keys(spec_presets))\
	X("scale", enum_table(scaletype)+1)\
	X("sign",  enum_table(signtype)+1)

assoc_gen(spec_presets)
assoc_gen(spec_options)

#undef spec_options

const char** spec_options() {
	return assoc_keys(spec_options);
}

const char** spec_option_values(const char* option) {
	return (const char**)assoc_val(spec_options,option);
}

const char* spec_param_parse(struct spec_params* p, const char* key, const char* val) {
	if(!strcmp(key,"scale")) {
		if(!(p->scaletype = enum_val(scaletype,val)))
			return val;
	}
	else if(!strcmp(key,"sign")) {
		if(!(p->signtype = enum_val(signtype,val)))
			return val;
	}
	else {
		// let preset be specified either with preset= or as a key
		key = strcmp(key,"preset") ? key : val;
		const struct spec_params* preset = assoc_val(spec_presets,key);
		if(!preset)
			return key;
		*p = *preset;
	}
	return NULL;
}

const char* spec_params_parse(struct spec_params* p, const char* options, const char* key_val_sep, const char* pairs_sep) {
	const char* errindex = NULL;
	char* key,* buf = strdup(options),* string = buf;
	while((key = strsep(&string,pairs_sep))) {
		if(!*key)
			continue;
		char* val = key + strcspn(key,key_val_sep);
		if(*val)
			*val++ = '\0';
		if(spec_param_parse(p,key,val))
			break;
	}
	if(key)
		errindex = options + (key-buf);
	free(buf);
	return errindex;
}

intermediate spec_normalization(size_t n) {
	size_t hn = n>>1;
	if(__builtin_expect(!(SIZE_MAX >> hn),0))
		return mi(pow)(mi(M_SQRT2),n);
	intermediate ert2n = (size_t)1 << hn;
	return n % 2 ? ert2n * mi(M_SQRT2) : ert2n;
}

intermediate spec_normalization_nd(size_t ndims, size_t dims[ndims]) {
	size_t n = 0;
	for(size_t i = 0; i < ndims; i++)
		n += dims[i] > 0;
	return spec_normalization(n);
}

intermediate spec_copysign(intermediate c, intermediate sign) {
	return mi(copysign)(c,!!sign*2-1);
}

typedef intermediate (*specfn)(intermediate);
struct spec_scaler {
	intermediate gain, max;
	specfn scale, sign, unscale, unsign;
};

// pre-permuting these into one specfn seems to have no benefit
static intermediate specfn_scale_linear(intermediate c) {
	return c;
}
static intermediate specfn_scale_log(intermediate c) {
	return mi(copysign)(mi(log1p)(mi(fabs)(c)),c);
}
static intermediate specfn_scale_exp(intermediate c) {
	return mi(copysign)(mi(expm1)(mi(fabs)(c)),c);
}
static intermediate specfn_sign_shift(intermediate c) {
	return (c/mi(2.)+mi(0.5))*(mi(254.)/255);
}
static intermediate specfn_sign_unshift(intermediate c) {
	return (c/(mi(254.)/255)-mi(0.5))*mi(2.);
}
static intermediate specfn_sign_abs(intermediate c) {
	return mi(fabs)(c);
}
static intermediate specfn_sign_map(intermediate c) {
	return c;
}
static intermediate specfn_sign_saturate(intermediate c) {
	return !signbit(c);
}
static intermediate specfn_sign_center(intermediate c) {
	return c*2-1;
}

struct spec_scaler* spec_create(struct spec_params* params, coeff max, coeff gain) {
	struct spec_scaler* p = malloc(sizeof(*p));
	if(!p)
		return NULL;

	switch(params->scaletype) {
		case scaletype_none:
		case scaletype_log:
			p->scale = specfn_scale_log;
			p->unscale = specfn_scale_exp;
			break;
		case scaletype_linear: p->scale = p->unscale = specfn_scale_linear; break;
	}
	switch(params->signtype) {
		case signtype_none:
		case signtype_abs:
			p->sign = specfn_sign_abs;
			p->unsign = specfn_sign_map;
			break;
		case signtype_shift:
			p->sign = specfn_sign_shift;
			p->unsign = specfn_sign_unshift;
			break;
		case signtype_saturate:
			p->sign = specfn_sign_saturate;
			p->unsign = specfn_sign_center;
			break;
	}

	p->gain = gain;
	p->max = p->scale(p->gain*max);

	return p;
}

void spec_destroy(struct spec_scaler* p) {
	free(p);
}

intermediate spec_scale(struct spec_scaler* p, intermediate c) {
	return p->sign(p->scale(c * p->gain) / p->max);
}

intermediate spec_unscale(struct spec_scaler* p, intermediate c) {
	return p->unscale(p->unsign(c) * p->max) / p->gain;
}
