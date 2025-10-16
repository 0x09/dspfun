/*
 * genbasis - Generate basis functions for a variety of 2D transforms.
 * Copyright 2012-2016 0x09.net.
 */

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <complex.h>
#include <float.h>
#include <getopt.h>
#include <strings.h>
#include <unistd.h>

#include "magickwand.h"
#include "longmath.h"
#include "precision.h"

#ifndef I
#define I _Complex_I
#endif

void real(complex_intermediate component, coeff real_component[3]) {
	real_component[0] = (mi(creal)(component)+1)/2;

	for(int i = 1; i < 3; i++)
		real_component[i] = real_component[0];
}
void imag(complex_intermediate component, coeff real_component[3]) {
	real_component[0] = (mi(cimag)(component)+1)/2;

	for(int i = 1; i < 3; i++)
		real_component[i] = real_component[0];
}
void mag(complex_intermediate component, coeff real_component[3]) {
	real_component[0] = (mi(cabs)(component)+1)/2;

	for(int i = 1; i < 3; i++)
		real_component[i] = real_component[0];
}
void phase(complex_intermediate component, coeff real_component[3]) {
	real_component[0] = (mi(carg)(component+I*INTERMEDIATE_CONST(EPSILON))+mi(M_PI))/mi(M_PI)/2;

	for(int i = 1; i < 3; i++)
		real_component[i] = real_component[0];
}
void cplx(complex_intermediate component, coeff real_component[3]) {
	real_component[0] = (mi(creal)(component)+1)/2;
	real_component[1] = 0;
	real_component[2] = (mi(cimag)(component)+1)/2;
}


complex_intermediate dft(long long k, long long n, unsigned long long N) {
	return mi(cexp)((-2*I*mi(M_PI)*k*n)/N);
}
complex_intermediate idft(long long k, long long n, unsigned long long N) {
	return mi(cexp)((2*I*mi(M_PI)*k*n)/N);
}
complex_intermediate dct1(long long k, long long n, unsigned long long N) {
	return (n && N-1-n) ? mi(cos)((mi(M_PI)*(k*n))/(N-1)) : (n ? mi(pow)(-1,k) : mi(1.))/2;
}
complex_intermediate dct2(long long k, long long n, unsigned long long N) {
	return mi(cos)((mi(M_PI)*(k*(2*n+1)))/(2*N));
}
complex_intermediate dct3(long long k, long long n, unsigned long long N) {
	return n ? mi(cos)((mi(M_PI)*(n*(2*k+1)))/(2*N)) : mi(0.5);
}
complex_intermediate dct4(long long k, long long n, unsigned long long N) {
	return mi(cos)((mi(M_PI)*((2*k+1)*(2*n+1)))/(4*N));
}
complex_intermediate dst1(long long k, long long n, unsigned long long N) {
	return mi(sin)((mi(M_PI)*((k+1)*(n+1)))/(N+1));
}
complex_intermediate dst2(long long k, long long n, unsigned long long N) {
	return mi(sin)((mi(M_PI)*((k+1)*(2*n+1)))/(2*N));
}
complex_intermediate dst3(long long k, long long n, unsigned long long N) {
	return (N-1-n) ? mi(sin)((mi(M_PI)*((2*k+1)*(n+1)))/(2*N)) : mi(pow)(-1,k)/2;
}
complex_intermediate dst4(long long k, long long n, unsigned long long N) {
	return mi(sin)((mi(M_PI)*((2*k+1)*(2*n+1)))/(4*N));
}
complex_intermediate wht(long long k, long long n, unsigned long long N) {
	unsigned long long sig = 0;
	for(int i = 0; i < log2(N); i++)
		sig += ((n>>i)&1LL)*((
			     //k>>i //natural order
			       i?(k>>(unsigned long long)(log2(N)-i-1))+(k>>(unsigned long long)(log2(N)-i)):k>>(unsigned long long)(log2(N)-1)
		       )&1LL);
	return mi(pow)(-1,sig);
}
complex_intermediate dht(long long k, long long n, unsigned long long N) {
	return mi(M_SQRT2) * mi(cos)(2*mi(M_PI)*n*k/N - mi(M_PI)/4);
}

typedef union { unsigned long long a[2]; struct { unsigned long long w, h; }; } coords;
typedef union { long long a[2]; struct { long long w, h; }; } offsets;

static void usage() {
	fprintf(stderr,"Usage: genbasis --size <WxH> [options] <outfile>\n");
	exit(1);
}
static void help() {
	puts(
	"Usage: genbasis --size <WxH> [options] <outfile>\n"
	"\n"
	"Options:\n"
	"  -h, --help             This help text.\n"
	"  -f, --function <type>  Type of basis to generate. [default: DFT]\n"
	"                         Types: DFT, iDFT, DCT[1-4], DST[1-4], WHT, DHT.\n"
	"  -I, --inverse          Transpose the output.\n"
	"  -n, --natural          Center the output around the DC. Commonly in DFT visualizations.\n"
	"  -P, --plane <type>     How to represent complex values in the output image. [default: real]\n"
	"                         Types: real, imaginary, magnitude, phase\n"
	"                         Note: types other than \"real\" are only meaningful for the DFT.\n"
	"  -s, --size <WxH>       Size of the basis functions.\n"
	"  -t, --terms <WxH>      Number of basis functions to generate in each dimension. [default: equal to --size]\n"
	"  -O, --offset <XxY>     Offset the terms by this amount [default: 0x0]\n"
	"  -p, --padding <p>      Amount of padding to add in between terms. [default: 1]\n"
	"  -S, --scale <int>      Integer point upscaling factor for basis functions. [default: 1]\n"
	"  -g, --linear           Generate the basis functions in linear light and scale to sRGB for output.\n"
	);
	exit(0);
}

int main(int argc, char* argv[]) {
	int opt;
	const char* outfile = isatty(STDOUT_FILENO) ? "sixel:-" : NULL;
	coords terms = {0}, size = {0};
	offsets offset = {0};
	int natural = false, inverse = false, linear = false;
	unsigned int scale = 1, padding = 1;
	coeff padcolor[3] = {1,0,0};
	complex_intermediate (*function)(long long, long long, unsigned long long) = dft;
	void (*realize)(complex_intermediate, coeff[3]) = real;
	const struct option gopts[] = {
		{"function",required_argument,NULL,'f'},
		{"inverse",no_argument,NULL,'I'},
		{"plane",required_argument,NULL,'P'},
		{"terms",required_argument,NULL,'t'},
		{"offset",required_argument,NULL,'O'},
		{"padding",required_argument,NULL,'p'},
		{"scale",required_argument,NULL,'S'},
		{"size",required_argument,NULL,'s'},
		{"natural",no_argument,NULL,'n'},
		{"linear",no_argument,NULL,'g'},
		{}
	};
	while((opt = getopt_long(argc,argv,"hf:InP:t:O:p:S:s:g",gopts,NULL)) != -1)
		switch(opt) {
			case 'h': help();
			case 'f': {
				if(!strcasecmp(optarg,"idft")) function = idft;
				else if(!strcasecmp(optarg,"wht")) function = wht;
				else if(!strcasecmp(optarg,"dht")) function = dht;
				else if(!strncasecmp(optarg,"dct",3))
					switch(optarg[3]) {
						case '1': function = dct1; break;
						case '3': function = dct3; break;
						case '4': function = dct4; break;
						default:  function = dct2;
					}
				else if(!strncasecmp(optarg,"dst",3))
					switch(optarg[3]) {
						case '1': function = dst1; break;
						case '3': function = dst3; break;
						case '4': function = dst4; break;
						default:  function = dst2;
					}
			} break;
			case 'I': inverse = true; break;
			case 'n': natural = true; break;
			case 'P': {
				if     (!strcmp(optarg,"imaginary")) realize = imag;
				else if(!strcmp(optarg,"magnitude")) realize = mag;
				else if(!strcmp(optarg,"phase"))     realize = phase;
				else if(!strcmp(optarg,"complex")) {
					realize = cplx;
					memcpy(padcolor,((coeff[3]){0.0625,0.1875,0.0625}),sizeof(*padcolor)*3);
				}
			} break;
			case 's': sscanf(optarg,"%llux%llu",&size.w,&size.h); break;
			case 't': sscanf(optarg,"%llux%llu",&terms.w,&terms.h); break;
			case 'O': sscanf(optarg,"%lldx%lld",&offset.w,&offset.h); break;
			case 'p': padding = strtoul(optarg,NULL,10); break;
			case 'S': scale = strtoul(optarg,NULL,10); break;
			case 'g': linear = true; break;
			default : usage();
		}

	argc -= optind;
	argv += optind;

	if(argc > 1)
		usage();
	if(argc)
		outfile = argv[0];
	if(!outfile)
		usage();

	for(int i = 0; i < 2; i++)
		if(!terms.a[i])
			terms.a[i] = size.a[i];

	coords bi, i;
	coords* k = &bi,* n = &i;
	coords* K = &terms,* N = &size;
	if(inverse) {
		K = &size;
		N = &terms;
		k = &i;
		n = &bi;
	}

	coords framesize;
	for(int i = 0; i < 2; i++)
		framesize.a[i] = size.a[i]*terms.a[i]*scale+padding*terms.a[i]+padding;

	for(int i = 0; natural && i < 2; i++)
		offset.a[i] -= terms.a[i]/2;

	coeff* frame = malloc(framesize.w*framesize.h*3*sizeof(*frame));
	for(int i = 0; i < framesize.w*framesize.h*3; i++)
		frame[i] = padcolor[i%3]; //fill

	#define INDEX(d) ((size.d*bi.d+i.d)*scale+padding*bi.d+padding)
	for(k->h = 0; k->h < K->h; k->h++)
		for(k->w = 0; k->w < K->w; k->w++)
			for(n->h = 0; n->h < N->h; n->h++)
				for(n->w = 0; n->w < N->w; n->w++) {
					complex_intermediate component = 1;
					for(int j = 0; j < 2; j++) {
						bi.a[j]+=offset.a[j];
						component *= function(k->a[j],n->a[j],size.a[j]);
						bi.a[j]-=offset.a[j];
					}
					coeff real_component[3];
					realize(component,real_component);
					for(size_t ys = 0; ys < scale; ys++)
						for(size_t xs = 0; xs < scale; xs++)
							for(int d = 0; d < 3; d++)
								frame[(((INDEX(h)+ys)*framesize.w+INDEX(w))+xs)*3+d] = real_component[d];
				}

	MagickWandGenesis();
	MagickWand* wand;
	wand = NewMagickWand();
	MagickConstituteImage(wand,framesize.w,framesize.h,"RGB",TypePixel,frame);
	if(linear) { // linear here refers to the processing colorspace, so this actually means genbasis should convert to nonlinear for output
		MagickSetImageColorspace(wand,RGBColorspace);
		MagickTransformImageColorspace(wand,sRGBColorspace);
	}
	MagickWriteImage(wand,outfile);
	DestroyMagickWand(wand);
	MagickWandTerminus();

	return 0;
}
