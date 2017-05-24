/*
 * draw - Draw DCT coefficients directly on a canvas.
 * Copyright 2011-2016 0x09.net.
 */

#include <stdio.h>
#include <math.h>
#include <png.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

#include <fftw3.h>

#include "magickwand.h"

#define a(n,N) (n?sqrt(2.f/N):sqrt(1.f/N))
struct size { int w,h; };
struct coef { int x,y; float w; };

void usage() {
	puts("usage: draw -b WxH -f XxY:strength");
	exit(0);
}
int main(int argc, char* argv[]) {
	struct size bs = { 512, 512 };
	struct coef* ba = NULL;
	int fns = 0, nc = 0;
	float energy = 0.f;
	int opt;
	while((opt = getopt(argc,argv,"b:f:")) != -1) {
		switch(opt) {
			case 'b': sscanf(optarg,"%dx%d",&bs.w,&bs.h); break;
			case 'f':
				ba = realloc(ba,sizeof(struct coef)*(fns+1));
				memset(ba+fns,0,sizeof(struct coef));
				ba[fns].w = -1;
				if(sscanf(optarg,"%dx%d:%f",&ba[fns].x,&ba[fns].y,&ba[fns].w) == 2) nc++;
				else energy += ba[fns].w;
				fns++;
				break;
		}
	}
	if(argc-optind < 1)
		usage();

	for(int i = 0; i < fns; i++)
		if(ba[i].w == -1) ba[i].w = (1-energy)/nc;
	float* coefs = fftwf_malloc(sizeof(float)*bs.w*bs.h);
	memset(coefs,0,sizeof(float)*bs.w*bs.h);

	for(int i = 0; i < fns; i++)
		coefs[ba[i].y*bs.w+ba[i].x] = ba[i].w/4;
	coefs[0] += 0.5;
	free(ba);

	fftwf_plan p = fftwf_plan_r2r_2d(bs.h,bs.w,coefs,coefs,FFTW_REDFT01,FFTW_REDFT01,FFTW_ESTIMATE);
	fftwf_execute(p);
	fftwf_destroy_plan(p);

	MagickWandGenesis();
	MagickWand* wand = NewMagickWand();
	MagickConstituteImage(wand,bs.w,bs.h,"I",FloatPixel,coefs);
	MagickWriteImage(wand,argv[argc-1]);
	DestroyMagickWand(wand);
	MagickWandTerminus();

	fftwf_free(coefs);
	fftwf_cleanup();

	return 0;
}