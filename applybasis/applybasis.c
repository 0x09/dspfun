/*
 * applybasis - Apply basis functions for a variety of 2D transforms to images.
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

intermediate real(complex_intermediate coeff) {
	return mi(creal)(coeff);
}
intermediate imag(complex_intermediate coeff) {
	return mi(cimag)(coeff);
}
intermediate mag(complex_intermediate coeff) {
	return mi(cabs)(coeff);
}
intermediate phase(complex_intermediate coeff) {
	return mi(carg)(coeff+I*INTERMEDIATE_CONST(EPSILON))/mi(M_PI);
}

void linear(intermediate coeff[3], intermediate scale) {
	for(int i = 0; i < 3; i++) coeff[i] /= scale;
}
void logscale(intermediate coeff[3], intermediate scale) {
	for(int i = 0; i < 3; i++) coeff[i] = mi(copysign)(mi(log1p)(mi(fabs)(coeff[i]))/mi(log1p)(scale),coeff[i]);
}
void gain(intermediate coeff[3], intermediate scale) {
	scale = mi(sqrt)(scale);
	linear(coeff,scale);
	logscale(coeff,scale);
}
void loglevel(intermediate coeff[3], intermediate scale) {
	linear(coeff,scale);
	logscale(coeff,1);
}

void absolute(intermediate coeff[3]) {
	for(int i = 0; i < 3; i++)
		coeff[i] = mi(fabs)(coeff[i]);
}
void invert(intermediate coeff[3]) {
	for(int i = 0; i < 3; i++)
		coeff[i] += coeff[i]<0;
}
void shift(intermediate coeff[3]) {
	for(int i = 0; i < 3; i++)
		coeff[i] = (coeff[i]+1)/2;
}
void (*shift2)(intermediate[3]) = shift; //dummy

void hue(intermediate coeff[3]) {
	if(coeff[0] >= 0 && coeff[1] >= 0 && coeff[2] >= 0)
		return;
	absolute(coeff);
	memcpy(coeff,
		((intermediate[3]){
			(- coeff[0] + 2*coeff[1] + 2*coeff[2])/3,
			(2*coeff[0] -   coeff[1] + 2*coeff[2])/3,
			(2*coeff[0] + 2*coeff[1] -   coeff[2])/3
		}),
	sizeof(intermediate[3]));
}

complex_intermediate dft(long long k, long long n, unsigned long long N, bool ortho) {
	return mi(cexp)((-2*I*mi(M_PI)*k*n)/N);
}
complex_intermediate idft(long long k, long long n, unsigned long long N, bool ortho) {
	return mi(cexp)((2*I*mi(M_PI)*k*n)/N);
}
complex_intermediate dct1(long long k, long long n, unsigned long long N, bool ortho) {
	if(ortho)
		return 0; //unimplemented
	return (n && N-1-n) ? mi(cos)((mi(M_PI)*(k*n))/(N-1)) : (1+mi(pow)(-1,k))/2;
}
complex_intermediate dct2(long long k, long long n, unsigned long long N, bool ortho) {
	intermediate coeff = mi(cos)((mi(M_PI)*(k*(2*n+1)))/(2*N));
	if(ortho)
		coeff *= (k ? mi(M_SQRT2) : 1);
	return coeff;
}
complex_intermediate dct3(long long k, long long n, unsigned long long N, bool ortho) {
	intermediate coeff = n ? mi(cos)((mi(M_PI)*(n*(2*k+1)))/(2*N)) : mi(0.5);
	if(ortho)
		coeff *= n ? mi(M_SQRT2) : 2;
	return coeff;
}
complex_intermediate dct4(long long k, long long n, unsigned long long N, bool ortho) {
	intermediate coeff = mi(cos)((mi(M_PI)*((2*k+1)*(2*n+1)))/(4*N));
	if(ortho)
		coeff *= mi(M_SQRT2);
	return coeff;
}
complex_intermediate dst1(long long k, long long n, unsigned long long N, bool ortho) {
	intermediate coeff = mi(sin)((mi(M_PI)*((k+1)*(n+1)))/(N+1));
	if(ortho)
		coeff *= mi(M_SQRT2)*mi(sqrt)(N/(intermediate)(N+1));
	return coeff;
}
complex_intermediate dst2(long long k, long long n, unsigned long long N, bool ortho) {
	if(ortho)
		return 0; //unimplemented
	return mi(sin)((mi(M_PI)*((k+1)*(2*n+1)))/(2*N));
}
complex_intermediate dst3(long long k, long long n, unsigned long long N, bool ortho) {
	if(ortho)
		return 0; //unimplemented
	return (N-1-n) ? mi(sin)((mi(M_PI)*((2*k+1)*(n+1)))/(2*N)) : mi(pow)(-1,k)/2;
}
complex_intermediate dst4(long long k, long long n, unsigned long long N, bool ortho) {
	intermediate coeff = mi(sin)((mi(M_PI)*((2*k+1)*(2*n+1)))/(4*N));
	if(ortho)
		coeff *= mi(M_SQRT2);
	return coeff;
}
complex_intermediate wht(long long k, long long n, unsigned long long N, bool ortho) {
	N = mi(log2)(N);
	unsigned long long sig = (n & (k >> (N-1))) & 1LL;
	for(N--, n>>=1; N; N--, n>>=1)
		sig += (n & ((k>>(N-1))+(k>>N))) & 1LL;
	return mi(pow)(-1,sig);
}

typedef union { unsigned long long a[2]; struct { unsigned long long w, h; }; } coords;
typedef union { long long a[2]; struct { long long w, h; }; } offsets;

static void usage() {
	fprintf(stderr,
	        "Usage: applybasis -i infile -o outfile [-d out.coeff]\n"
	        "            -f|--function=(DFT),iDFT,DCT[1-4],DST[1-4],WHT  [-I|--inverse]\n"
	        "            [-P|--plane=(real),imag,mag,phase]  [-R|--rescale=(linear),log,gain,level[-...]]  [-N|--range=shift,(shift2),abs,invert,hue]\n"
	        "            [-t|--terms WxH]  [-s|--sum NxM]  [-O|--offset XxY]  [-p|--padding p]  [-S|--scale scale]  [-g|--linear]\n");
	exit(0);
}
int main(int argc, char* argv[]) {
	int opt;
	char* infile = NULL,* outfile = NULL,* outcoeffs = NULL;
	if(isatty(STDOUT_FILENO))
		outfile = "sixel:-";
	coords terms = {}, partsum = {1,1};
	offsets offset = {};
	int inverse = false, orthogonal = false, linearlight = false;
	unsigned int scale = 1, padding = 1;
	intermediate (*realize)(complex_intermediate) = real;
	void (*rescale[2])(intermediate[3],intermediate) = {linear};
	void (*range)(intermediate[3]) = shift2;
	complex_intermediate (*function)(long long, long long, unsigned long long,bool) = dft;
	coeff padcolor[3] = {0,0,0};
	const struct option gopts[] = {
		{"function",required_argument,NULL,'f'},
		{"inverse",no_argument,NULL,'I'},
		{"plane",required_argument,NULL,'P'},
		{"rescale",required_argument,NULL,'R'},
		{"range",required_argument,NULL,'N'},
		{"terms",required_argument,NULL,'t'},
		{"sum",required_argument,NULL,'s'},
		{"offset",required_argument,NULL,'O'},
		{"padding",required_argument,NULL,'p'},
		{"scale",required_argument,NULL,'S'},
		{"linear",no_argument,NULL,'g'},
		{}
	};
	while((opt = getopt_long(argc,argv,"d:f:IP:R:N:t:s:O:p:S:",gopts,NULL)) != -1)
		switch(opt) {
			case 0: break;
			case 'd':
				outcoeffs = optarg;
				orthogonal = true;
				break;
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
			case 'P': {
				if     (!strcmp(optarg,"imaginary")) realize = imag;
				else if(!strcmp(optarg,"magnitude")) realize = mag;
				else if(!strcmp(optarg,"phase")) realize = phase;
			} break;
			case 'R': {
				for(int i = 0; i < 2; i++, optarg += *optarg == '-')
					if     (!strncmp(optarg,"linear",6)){ rescale[i] = linear;   optarg+=6; }
					else if(!strncmp(optarg,"log",3))   { rescale[i] = logscale; optarg+=3; }
					else if(!strncmp(optarg,"gain",4))  { rescale[i] = gain;     optarg+=4; }
					else if(!strncmp(optarg,"level",5)) { rescale[i] = loglevel; optarg+=5; }
			} break;
			case 'N': {
				if     (!strcmp(optarg,"abs"))    range = absolute;
				else if(!strcmp(optarg,"shift"))  range = shift;
				else if(!strcmp(optarg,"invert")) range = invert;
				else if(!strcmp(optarg,"hue"))    range = hue;
			} break;
			case 't': sscanf(optarg,"%llux%llu",&terms.w,&terms.h); break;
			case 's': sscanf(optarg,"%llux%llu",&partsum.w,&partsum.h); break;
			case 'O': sscanf(optarg,"%lldx%lld",&offset.w,&offset.h); break;
			case 'p': padding = strtoul(optarg,NULL,10); break;
			case 'S': scale = strtoul(optarg,NULL,10); break;
			case 'g': linearlight = true; break;
			default : usage();
		}

	argc -= optind;
	argv += optind;

	if(argc < 1 || argc > 2)
		usage();

	infile = argv[0];
	if(argc > 1)
		outfile = argv[1];
	if(!outfile)
		usage();

	size_t inrange = 1;
	complex_intermediate* pixels;
	coords insize, size;

	MagickWandGenesis();
	MagickWand* wand;

	if(!strcmp(strrchr(infile,'.'),".coeff")) {
		orthogonal = true;
		FILE* f = fopen(infile,"r");
		fread(&insize,sizeof(insize),1,f);
		pixels = malloc(sizeof(*pixels)*insize.w*insize.h*3);
		fread(pixels,sizeof(*pixels),insize.w*insize.h*3,f);
		fclose(f);
		inrange = (insize.w/partsum.w)*(insize.h/partsum.h);
	}
	else {
		wand = NewMagickWand();
		if(!MagickReadImage(wand,infile)) {
			DestroyMagickWand(wand);
			MagickWandTerminus();
			return 1;
		}
		insize.w = MagickGetImageWidth(wand);
		insize.h = MagickGetImageHeight(wand);
		pixels = malloc(sizeof(*pixels)*insize.w*insize.h*3);
		if(linearlight)
			MagickTransformImageColorspace(wand,RGBColorspace);
		coeff* magickpixels = malloc(insize.w*insize.h*3*sizeof(*magickpixels));
		MagickExportImagePixels(wand,0,0,insize.w,insize.h,"RGB",TypePixel,magickpixels);
		for(int i = 0; i < insize.w*insize.h*3; i++)
			pixels[i] = magickpixels[i];
		if(range == shift2)
			for(int i = 0; i < insize.w*insize.h*3; i++)
				pixels[i] = pixels[i]*2 - 1;
		free(magickpixels);
		DestroyMagickWand(wand);
	}

	size = insize;
	for(int i = 0; i < 2; i++)
		if(!terms.a[i])
			terms.a[i] = insize.a[i];

	coords bi, i, s;
	coords* k = &bi,* n = &i;
	coords* K = &terms,* N = &size;
	if(inverse) {
		K = &size;
		N = &terms;
		k = &i;
		n = &bi;
	}
	N->w /= partsum.w;
	N->h /= partsum.h;


	FILE* df = NULL;
	coords dumpsize = {{N->w*K->w,N->h*K->h}};
	if(outcoeffs) {
		df = fopen(outcoeffs,"w");
		fwrite(&dumpsize,sizeof(dumpsize),1,df);
	}

	coords framesize;
	for(int i = 0; i < 2; i++)
		framesize.a[i] = size.a[i]*terms.a[i]*scale+padding*terms.a[i]+padding;

	coeff* frame = malloc(framesize.w*framesize.h*3*sizeof(*frame));
	for(int i = 0; i < framesize.w*framesize.h*3; i++)
		frame[i] = padcolor[i%3]; //fill

	unsigned long long coeff_scale = partsum.w*partsum.h*inrange;

	#define INDEX(d) ((size.d*bi.d+i.d)*scale+padding*bi.d+padding)
	for(k->h = 0; k->h < K->h; k->h++)
		for(k->w = 0; k->w < K->w; k->w++)
			for(n->h = 0; n->h < N->h; n->h++)
				for(n->w = 0; n->w < N->w; n->w++) {
					complex_intermediate partsums[3] = {0};
					for(s.h = 0; s.h < partsum.h; s.h++)
						for(s.w = 0; s.w < partsum.w; s.w++) {
							complex_intermediate component = 1;
							for(int j = 0; j < 2; j++) {
								bi.a[j]+=offset.a[j];
								component *= function(k->a[j],n->a[j]*partsum.a[j]+s.a[j],insize.a[j],orthogonal);
								bi.a[j]-=offset.a[j];
							}
							for(int j = 0; j < 3; j++)
								partsums[j] += component * pixels[((n->h*partsum.h+s.h)*insize.w+n->w*partsum.w+s.w)*3+j];
						}
					intermediate real_coeff[3], tmp[3];
					for(int j = 0; j < 3; j++)
						real_coeff[j] = tmp[j] = realize(partsums[j]);
					rescale[0](real_coeff,coeff_scale);
					if(rescale[1]) {
						rescale[1](tmp,coeff_scale);
						intermediate NN = mi(sqrt)(insize.w*insize.h)-1, nn = mi(sqrt)(coeff_scale)-1;
						for(int j = 0; j < 3; j++)
							real_coeff[j] = ((NN-nn)*real_coeff[j]+nn*tmp[j])/NN;
					}
					range(real_coeff);
					for(size_t ys = 0; ys < scale; ys++)
						for(size_t xs = 0; xs < scale; xs++)
							for(int d = 0; d < 3; d++)
								frame[(((INDEX(h)+ys)*framesize.w+INDEX(w))+xs)*3+d] = real_coeff[d];
					if(df)
						fwrite(partsums,sizeof(partsums),1,df);
				}
	wand = NewMagickWand();
	MagickConstituteImage(wand,framesize.w,framesize.h,"RGB",TypePixel,frame);
	if(linearlight) {
		MagickSetImageColorspace(wand,RGBColorspace);
		MagickTransformImageColorspace(wand,sRGBColorspace);
	}
	MagickWriteImage(wand,outfile);
	DestroyMagickWand(wand);
	MagickWandTerminus();

	if(df)
		fclose(df);
	free(pixels);

	return 0;
}
