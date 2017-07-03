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

#include "magickwand.h"
#include "longmath.h"

#ifndef I
#define I _Complex_I
#endif

#define CLAMP(x) ((x)<0?0:(x)>255?255:(x))

long double real(complex long double ccoeff) {
	return creall(ccoeff);
}
long double imag(complex long double ccoeff) {
	return cimagl(ccoeff);
}
long double mag(complex long double ccoeff) {
	return cabsl(ccoeff);
}
long double phase(complex long double ccoeff) {
	return 255.l*cargl(ccoeff+FLT_EPSILON*I)/M_PIl;
}

void linear(long double coeff[3], long double scale) {
	for(int i = 0; i < 3; i++) coeff[i] /= scale;
}
void logscale(long double coeff[3], long double scale) {
	for(int i = 0; i < 3; i++) coeff[i] = copysignl(255.l*log1pl(fabsl(coeff[i]))/log1pl(255.l*scale),coeff[i]);
}
void gain(long double coeff[3], long double scale) {
	linear(coeff,sqrtl(scale));
	logscale(coeff,sqrtl(scale));
}
void loglevel(long double coeff[3], long double scale) {
	linear(coeff,scale);
	logscale(coeff,1);
}

void absolute(long double coeff[3]) {
	for(int i = 0; i < 3; i++)
		coeff[i] = fabsl(coeff[i]);
}
void invert(long double coeff[3]) {
	for(int i = 0; i < 3; i++)
		coeff[i] += (coeff[i]<0)*255.l;
}
void shift(long double coeff[3]) {
	for(int i = 0; i < 3; i++)
		coeff[i] = (coeff[i]+255.l)/2.l;
}
void (*shift2)(long double[3]) = shift; //dummy

void hue(long double coeff[3]) {
	if(coeff[0] >= 0 && coeff[1] >= 0 && coeff[2] >= 0)
		return;
	absolute(coeff);
	memcpy(coeff,
		((long double[3]){
			(- coeff[0] + 2*coeff[1] + 2*coeff[2])/3,
			(2*coeff[0] -   coeff[1] + 2*coeff[2])/3,
			(2*coeff[0] + 2*coeff[1] -   coeff[2])/3
		}),
	sizeof(long double[3]));
}

complex long double dft(long long k, long long n, unsigned long long N, bool ortho) {
	return cexpl((-2*I*M_PIl*k*n)/N);
}
complex long double idft(long long k, long long n, unsigned long long N, bool ortho) {
	return cexpl((2*I*M_PIl*k*n)/N);
}
complex long double dct1(long long k, long long n, unsigned long long N, bool ortho) {
	if(ortho)
		return 0; //unimplemented
	return (n && N-1-n) ? cosl((M_PIl*(k*n))/(N-1)) : 0.5l*(1+powl(-1,k));
}
complex long double dct2(long long k, long long n, unsigned long long N, bool ortho) {
	long double coeff = cosl((M_PIl*(k*(2*n+1)))/(2*N));
	if(ortho)
		coeff *= (k ? M_SQRT2l : 1);
	return coeff;
}
complex long double dct3(long long k, long long n, unsigned long long N, bool ortho) {
	long double coeff = n ? cosl((M_PIl*(n*(2*k+1)))/(2*N)) : 0.5l;
	if(ortho)
		coeff *= n ? M_SQRT2l : 2;
	return coeff;
}
complex long double dct4(long long k, long long n, unsigned long long N, bool ortho) {
	long double coeff = cosl((M_PIl*((2*k+1)*(2*n+1)))/(4*N));
	if(ortho)
		coeff *= M_SQRT2l;
	return coeff;
}
complex long double dst1(long long k, long long n, unsigned long long N, bool ortho) {
	long double coeff = sinl((M_PIl*((k+1)*(n+1)))/(N+1));
	if(ortho)
		coeff *= M_SQRT2l*sqrt(N/(long double)(N+1));
	return coeff;
}
complex long double dst2(long long k, long long n, unsigned long long N, bool ortho) {
	if(ortho)
		return 0; //unimplemented
	return sinl((M_PIl*((k+1)*(2*n+1)))/(2*N));
}
complex long double dst3(long long k, long long n, unsigned long long N, bool ortho) {
	if(ortho)
		return 0; //unimplemented
	return (N-1-n) ? sinl((M_PIl*((2*k+1)*(n+1)))/(2*N)) : powl(-1,k)/2;
}
complex long double dst4(long long k, long long n, unsigned long long N, bool ortho) {
	long double coeff = sinl((M_PIl*((2*k+1)*(2*n+1)))/(4*N));
	if(ortho)
		coeff *= M_SQRT2l;
	return coeff;
}
complex long double wht(long long k, long long n, unsigned long long N, bool ortho) {
	N = log2(N);
	unsigned long long sig = (n & (k >> (N-1))) & 1LL;
	for(N--, n>>=1; N; N--, n>>=1)
		sig += (n & ((k>>(N-1))+(k>>N))) & 1LL;
	return powl(-1,sig);
}

typedef union { unsigned long long a[2]; struct { unsigned long long w, h; }; } coords;
typedef union { long long a[2]; struct { long long w, h; }; } offsets;

static void usage() {
	puts("Usage: applybasis -i infile -o outfile [-d out.coeff]\n"
	     "            -f|--function=(DFT),iDFT,DCT[1-4],DST[1-4],WHT  [-I|--inverse]\n"
	     "            [-P|--plane=(real),imag,mag,phase]  [-R|--rescale=(linear),log,gain,level[-...]]  [-N|--range=shift,(shift2),abs,invert,hue]\n"
	     "            [-t|--terms WxH]  [-s|--sum NxM]  [-O|--offset XxY]  [-p|--padding p]  [-S|--scale scale]");
	exit(0);
}
int main(int argc, char* argv[]) {
	int opt;
	int optind = 0;
	char* infile = NULL,* outfile = NULL,* outcoeffs = NULL;
	coords terms = {}, partsum = {};
	offsets offset = {};
	int inverse = false, orthogonal = false;
	unsigned int scale = 1, padding = 1;
	long double (*realize)(complex long double) = real;
	void (*rescale[2])(long double[3],long double) = {linear};
	void (*range)(long double[3]) = shift2;
	complex long double (*function)(long long, long long, unsigned long long,bool) = dft;
	unsigned char padcolor[3] = {0,0,0};
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
		{}
	};
	while((opt = getopt_long(argc,argv,"i:o:d:f:IP:R:N:t:s:O:p:S:",gopts,NULL)) != -1)
		switch(opt) {
			case 0: break;
			case 'i': infile = optarg; break;
			case 'o': outfile = optarg; break;
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
				if     (!strcmp(optarg,"imag"))  realize = imag;
				else if(!strcmp(optarg,"mag"))   realize = mag;
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
			default : usage();
		}
	if(!infile || !outfile)
		usage();

	size_t inrange = 1;
	complex long double* pixels;
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
		unsigned char* magickpixels = malloc(insize.w*insize.h*3);
		MagickExportImagePixels(wand,0,0,insize.w,insize.h,"RGB",CharPixel,magickpixels);
		for(int i = 0; i < insize.w*insize.h*3; i++)
			pixels[i] = magickpixels[i];
		if(range == shift2)
			for(int i = 0; i < insize.w*insize.h*3; i++)
				pixels[i] = pixels[i]*2 - 255;
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
	coords dumpsize = {N->w*K->w,N->h*K->h};
	if(outcoeffs) {
		df = fopen(outcoeffs,"w");
		fwrite(&dumpsize,sizeof(dumpsize),1,df);
	}

	coords framesize;
	for(int i = 0; i < 2; i++)
		framesize.a[i] = size.a[i]*terms.a[i]*scale+padding*terms.a[i]+padding;

	unsigned char* frame = malloc(framesize.w*framesize.h*3);
	for(int i = 0; i < framesize.w*framesize.h*3; i++)
		frame[i] = padcolor[i%3]; //fill

	#define INDEX(d) ((size.d*bi.d+i.d)*scale+padding*bi.d+padding)
	for(k->h = 0; k->h < K->h; k->h++)
		for(k->w = 0; k->w < K->w; k->w++)
			for(n->h = 0; n->h < N->h; n->h++)
				for(n->w = 0; n->w < N->w; n->w++) {
					complex long double partsums[3] = {};
					for(s.h = 0; s.h < partsum.h; s.h++)
						for(s.w = 0; s.w < partsum.w; s.w++) {
							complex long double ccoeff = 1;
							for(int j = 0; j < 2; j++) {
								bi.a[j]+=offset.a[j];
								ccoeff *= function(k->a[j],n->a[j]*partsum.a[j]+s.a[j],insize.a[j],orthogonal);
								bi.a[j]-=offset.a[j];
							}
							for(int j = 0; j < 3; j++)
								partsums[j] += ccoeff * pixels[((n->h*partsum.h+s.h)*insize.w+n->w*partsum.w+s.w)*3+j];
						}
					long double coeff[3], coeff2[3];
					for(int j = 0; j < 3; j++)
						coeff[j] = coeff2[j] = realize(partsums[j]);
					rescale[0](coeff,partsum.w*partsum.h*inrange);
					if(rescale[1]) {
						rescale[1](coeff2,partsum.w*partsum.h*inrange);
						long double NN = sqrtl(insize.w*insize.h)-1, nn = sqrtl(partsum.w*partsum.h*inrange)-1;
						for(int j = 0; j < 3; j++)
							coeff[j] = ((NN-nn)*coeff[j]+nn*coeff2[j])/NN;
					}
					range(coeff);
					for(size_t ys = 0; ys < scale; ys++)
						for(size_t xs = 0; xs < scale; xs++)
							for(int d = 0; d < 3; d++)
								frame[(((INDEX(h)+ys)*framesize.w+INDEX(w))+xs)*3+d] = CLAMP(coeff[d]);
					if(df)
						fwrite(partsums,sizeof(partsums),1,df);
				}
	wand = NewMagickWand();
	MagickConstituteImage(wand,framesize.w,framesize.h,"RGB",CharPixel,frame);
	MagickWriteImage(wand,outfile);
	DestroyMagickWand(wand);
	MagickWandTerminus();

	if(df)
		fclose(df);
	free(pixels);

	return 0;
}
