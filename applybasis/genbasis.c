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

#include "magickwand.h"
#include "longmath.h"

#ifndef I
#define I _Complex_I
#endif

void real(complex long double ccoeff, long double coeff[3]) {
	coeff[0] = 127.5l*(creall(ccoeff)+1.l);
	for(int i = 1; i < 3; i++) coeff[i] = coeff[0];
}
void imag(complex long double ccoeff, long double coeff[3]) {
	coeff[0] = 127.5l*(cimagl(ccoeff)+1.l);
	for(int i = 1; i < 3; i++) coeff[i] = coeff[0];
}
void mag(complex long double ccoeff, long double coeff[3]) {
	coeff[0] = 127.5l*(cabsl(ccoeff)+1.l);
	for(int i = 1; i < 3; i++) coeff[i] = coeff[0];
}
void phase(complex long double ccoeff, long double coeff[3]) {
	coeff[0] = 127.5l*(cargl(ccoeff+DBL_EPSILON*I)+M_PIl)/M_PIl;
	for(int i = 1; i < 3; i++) coeff[i] = coeff[0];
}
void cplx(complex long double ccoeff, long double coeff[3]) {
	coeff[0] = 127.5l*(creall(ccoeff)+1.l);
	coeff[1] = 0;
	coeff[2] = 127.5l*(cimagl(ccoeff)+1.l);
}


complex long double dft(long long k, long long n, unsigned long long N) {
	return cexpl((-2*I*M_PIl*k*n)/N);
}
complex long double idft(long long k, long long n, unsigned long long N) {
	return cexpl((2*I*M_PIl*k*n)/N);
}
complex long double dct1(long long k, long long n, unsigned long long N) {
	return (n && N-1-n) ? cosl((M_PIl*(k*n))/(N-1)) : 0.5l*(1+powl(-1,k));
}
complex long double dct2(long long k, long long n, unsigned long long N) {
	return cosl((M_PIl*(k*(2*n+1)))/(2*N));
}
complex long double dct3(long long k, long long n, unsigned long long N) {
	return n ? cosl((M_PIl*(n*(2*k+1)))/(2*N)) : 0.5l;
}
complex long double dct4(long long k, long long n, unsigned long long N) {
	return cosl((M_PIl*((2*k+1)*(2*n+1)))/(4*N));
}
complex long double dst1(long long k, long long n, unsigned long long N) {
	return sinl((M_PIl*((k+1)*(n+1)))/(N+1));
}
complex long double dst2(long long k, long long n, unsigned long long N) {
	return sinl((M_PIl*((k+1)*(2*n+1)))/(2*N));
}
complex long double dst3(long long k, long long n, unsigned long long N) {
	return (N-1-n) ? sinl((M_PIl*((2*k+1)*(n+1)))/(2*N)) : powl(-1,k)/2;
}
complex long double dst4(long long k, long long n, unsigned long long N) {
	return sinl((M_PIl*((2*k+1)*(2*n+1)))/(4*N));
}
complex long double wht(long long k, long long n, unsigned long long N) {
	unsigned long long sig = 0;
	for(int i = 0; i < log2(N); i++)
		sig += ((n>>i)&1LL)*((
			     //k>>i //natural order
			       i?(k>>(unsigned long long)(log2(N)-i-1))+(k>>(unsigned long long)(log2(N)-i)):k>>(unsigned long long)(log2(N)-1)
		       )&1LL);
	return powl(-1,sig);
}

typedef union { unsigned long long a[2]; struct { unsigned long long w, h; }; } coords;
typedef union { long long a[2]; struct { long long w, h; }; } offsets;

static void usage() {
	puts("Usage: genbasis -o outfile -f|--function=(DFT),iDFT,DCT[1-4],DST[1-4],WHT [-I|--inverse] [-n|--natural] [-P|--plane=(real),imag,mag,phase,cplx]\n"
	     "             -s|--size WxH [-t|--terms WxH] [-O|--offset XxY] [-p|--padding p] [-S|--scale s]\n");
	exit(0);
}
int main(int argc, char* argv[]) {
	int opt;
	int optind = 0;
	char* outfile = NULL;
	coords terms = {0}, size = {0};
	offsets offset = {0};
	int natural = false, inverse = false;
	unsigned int scale = 1, padding = 1;
	unsigned char padcolor[3] = {255,0,0};
	complex long double (*function)(long long, long long, unsigned long long) = dft;
	void (*realize)(complex long double, long double[3]) = real;
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
		{}
	};
	while((opt = getopt_long(argc,argv,"o:f:InP:t:O:p:S:s:",gopts,NULL)) != -1)
		switch(opt) {
			case 'o': outfile = optarg; break;
			case 'f': {
				if(!strcasecmp(optarg,"idft")) function = idft;
				else if(!strcasecmp(optarg,"wht")) function = wht;
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
				if     (!strcmp(optarg,"imag"))  realize = imag;
				else if(!strcmp(optarg,"mag"))   realize = mag;
				else if(!strcmp(optarg,"phase")) realize = phase;
				else if(!strcmp(optarg,"cplx")){ realize = cplx; memcpy(padcolor,((unsigned char[3]){16,48,16}),3); }
			} break;
			case 's': sscanf(optarg,"%llux%llu",&size.w,&size.h); break;
			case 't': sscanf(optarg,"%llux%llu",&terms.w,&terms.h); break;
			case 'O': sscanf(optarg,"%lldx%lld",&offset.w,&offset.h); break;
			case 'p': padding = strtoul(optarg,NULL,10); break;
			case 'S': scale = strtoul(optarg,NULL,10); break;
			default : usage();
		}
	if(!outfile)
		usage();

	for(int i = 0; i < 2; i++)
		if(!terms.a[i])
			terms.a[i] = size.a[i];

	coords bi, i, s;
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

	unsigned char* frame = malloc(framesize.w*framesize.h*3);
	for(int i = 0; i < framesize.w*framesize.h*3; i++)
		frame[i] = padcolor[i%3]; //fill

	#define INDEX(d) ((size.d*bi.d+i.d)*scale+padding*bi.d+padding)
	for(k->h = 0; k->h < K->h; k->h++)
		for(k->w = 0; k->w < K->w; k->w++)
			for(n->h = 0; n->h < N->h; n->h++)
				for(n->w = 0; n->w < N->w; n->w++) {
					complex long double ccoeff = 1;
					for(int j = 0; j < 2; j++) {
						bi.a[j]+=offset.a[j];
						ccoeff *= function(k->a[j],n->a[j],size.a[j]);
						bi.a[j]-=offset.a[j];
					}
					long double coeff[3];
					realize(ccoeff,coeff);
					for(size_t ys = 0; ys < scale; ys++)
						for(size_t xs = 0; xs < scale; xs++)
							for(int d = 0; d < 3; d++)
								frame[(((INDEX(h)+ys)*framesize.w+INDEX(w))+xs)*3+d] = coeff[d];
				}

	MagickWandGenesis();
	MagickWand* wand;
	wand = NewMagickWand();
	MagickConstituteImage(wand,framesize.w,framesize.h,"RGB",CharPixel,frame);
	MagickWriteImage(wand,outfile);
	DestroyMagickWand(wand);
	MagickWandTerminus();

	return 0;
}
