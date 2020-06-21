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
	X(T,sign)

#define signtype(X,T)\
	X(T,abs)\
	X(T,shift)\
	X(T,saturate)

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
	X(NULL,   (&(struct specparams){0}))

assoc_gen(specparams)

static inline struct specopts spec_args(int argc, char* argv[], const char* ignore) {
	struct specopts opts={.csp="RGB", .gain = 1 };
	static const char spec_optflags[] = "gc:t:s:hT:S:G:R:";
	char optflags[strlen(spec_optflags)+strlen(ignore)+1];
	strcat(strcpy(optflags,spec_optflags),ignore);
	int c;
	while((c = getopt(argc,argv,optflags)) > 0)
		switch(c) {
			case 'g': opts.gamma = true; break;
			case 'c': opts.csp = optarg; break;
			case 't': opts.params = *(struct specparams*)assoc_val(specparams,optarg); break;
			case 'R': opts.params.rangetype = enum_val(rangetype,optarg); break;
			case 'T': opts.params.scaletype = enum_val(scaletype,optarg); break;
			case 'S': opts.params.signtype = enum_val(signtype,optarg); break;
			case 'G':
				if(!(opts.params.gaintype = enum_val(gaintype,optarg))) {
					opts.params.gaintype = gaintype_custom;
					opts.gain = strtold(optarg,NULL);
				}
				break;
			case 'h':
				asprintf(&opts.help,"-g -c csp -t (%s) -R (%s) -T (%s) -S (%s) -G (%s(float)) ",
				         enum_keys(spectype),enum_keys(rangetype),enum_keys(scaletype),enum_keys(signtype),enum_keys(gaintype));
			break; //let the OS get it
		}
	argc -= optind;
	argv += optind;
	opts.input = opts.output = "-";
	if(argc > 0) opts.input  = argv[0];
	if(argc > 1) opts.output = argv[1];
	opterr = 0;
	//optreset = optind = 1;
	optind = 1;
	return opts;
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
