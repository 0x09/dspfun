/*
 * spec - Generate invertible frequency spectrums for viewing and editing.
 * Copyright 2014-2016 0x09.net.
 */

#ifndef SPEC_H
#define SPEC_H

#include <fftw3.h>
#include <math.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <getopt.h>

#include "precision.h"
#include "longmath.h"
#include "magickwand.h"

#define absf(f,x) mi(copysign)(mi(f)(mc(fabs)(x)),x)

#include "keyed_enum.h"
#include "assoc.h"

#define spectype(X,T)\
	X(T,abs)\
	X(T,shift)\
	X(T,flat)\
	X(T,sign)\
	X(T,copy)

#define signtype(X,T)\
	X(T,abs)\
	X(T,shift)\
	X(T,saturate)\
	X(T,retain)

#define rangetype(X,T)\
	X(T,one)\
	X(T,dc)\
	X(T,dcs)

#define scaletype(X,T)\
	X(T,linear)\
	X(T,log)

#define gaintype(X,T)\
	X(T,native)\
	X(T,reference)\
	X(T,custom)

enum_gen(spectype)
enum_gen(signtype)
enum_gen(rangetype)
enum_gen(scaletype)
enum_gen(gaintype)

struct specparams {
	enum scaletype scaletype;
	enum signtype signtype;
	enum gaintype gaintype;
	enum rangetype rangetype;
};
struct specopts {
	char* help;
	bool gamma;
	const char* csp;
	const char* input,* output;
	struct specparams params;
	intermediate gain, clip, quant;
};

#define specparams(X)\
	X("abs",  (&(struct specparams){scaletype_log,   signtype_abs,     gaintype_native,rangetype_dc }))\
	X("shift",(&(struct specparams){scaletype_log,   signtype_shift,   gaintype_native,rangetype_one}))\
	X("flat", (&(struct specparams){scaletype_linear,signtype_shift,   gaintype_custom,rangetype_one}))\
	X("sign", (&(struct specparams){scaletype_linear,signtype_saturate,gaintype_custom,rangetype_one}))\
	X("copy", (&(struct specparams){scaletype_linear,signtype_retain,  gaintype_custom,rangetype_one}))\
	X(NULL,   (&(struct specparams){0}))

assoc_gen(specparams)

#define SPEC_OPT_FLAGS "gc:t:s:T:S:G:R:"

const static struct specopts spec_opt_defaults = { .csp="RGB", .gain = 1 };

static void spec_usage(FILE* out) {
	fprintf(out,"-g -c csp -t (%s) -R (%s) -T (%s) -S (%s) -G (%s(float)) ",
	        enum_keys(spectype),enum_keys(rangetype),enum_keys(scaletype),enum_keys(signtype),enum_keys(gaintype));
}

static void spec_help(void) {
	printf(
	"  -g             Generate in linear light\n"
	"  -c <channels>  Color channels to use. [default: RGB]\n"
	"  -t <template>  Spectrogram template. [default: abs]\n"
	"                 values: %s\n"
	"  -R <range>     Range to scale coffieicnts to. [default: one]\n"
	"                 values: %s\n"
	"  -T <scale>     How to scale coefficients. [default: log]\n"
	"                 values: %s\n"
	"  -S <sign>      How to represent signed values. [default: abs]\n"
	"                 values: %s\n"
	"  -G <gain>      Multiplier for scaling. [default: native]\n"
	"                 values: %s\n",
		enum_keys(spectype),
		enum_keys(rangetype),
		enum_keys(scaletype),
		enum_keys(signtype),
		enum_keys(gaintype)
	);
}

static int spec_opt_proc(struct specopts* opts, int c, const char* arg) {
	switch(c) {
		case 'g': opts->gamma = true; break;
		case 'c': opts->csp = arg; break;
		case 't': opts->params = *(struct specparams*)assoc_val(specparams,arg); break;
		case 'R': opts->params.rangetype = enum_val(rangetype,arg); break;
		case 'T': opts->params.scaletype = enum_val(scaletype,arg); break;
		case 'S': opts->params.signtype = enum_val(signtype,arg); break;
		case 'G':
			if(!(opts->params.gaintype = enum_val(gaintype,arg))) {
				opts->params.gaintype = gaintype_custom;
				opts->gain = strtold(arg,NULL);
			}
			break;
		default: return 1;
	}
	return 0;
}

#include <limits.h>

static inline void base16enc(const void* restrict in_, void* restrict out_, size_t size) {
	unsigned char* out = out_;
	for(const unsigned char* in = in_; in != (const unsigned char*)in_+size; in++) {
		*out++ = (*in&15)+65;
		*out++ = (*in>>4)+65;
	}
}
static inline void base16dec(const void* restrict in_, void* restrict out_, size_t size) {
	const unsigned char* in = in_;
	for(unsigned char* out = out_; out != (unsigned char*)out_ + size; out++, in+=2)
		*out = (in[0]-65)|((in[1]-65)<<4);
}
#endif
