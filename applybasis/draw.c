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
#include "precision.h"

struct size { int w,h; };
struct coef { int x,y; coeff w; };

void usage() {
	fprintf(stderr,"Usage: draw -b <WxH> [-f <XxY:strength> ...] <outfile>\n");
	exit(1);
}
void help() {
	puts(
	"Usage: draw -b <WxH> [-f <XxY:strength> ...] <outfile>\n"
	"\n"
	"    Options:\n"
	"  -b <WxH>           Size of the output image.\n"
	"  -f <XxY:strength>  Frequency component position and value. My repeat.\n"
	);
	exit(0);
}

int main(int argc, char* argv[]) {
	struct size bs = { 512, 512 };
	struct coef* ba = NULL;
	int fns = 0, nc = 0;
	coeff energy = 0;
	int opt;
	while((opt = getopt(argc,argv,"b:f:h")) != -1) {
		switch(opt) {
			case 'h': help();
			case 'b': sscanf(optarg,"%dx%d",&bs.w,&bs.h); break;
			case 'f':
				ba = realloc(ba,sizeof(struct coef)*(fns+1));
				memset(ba+fns,0,sizeof(struct coef));
				ba[fns].w = -1;
				if(sscanf(optarg,"%dx%d:%" COEFF_SPECIFIER,&ba[fns].x,&ba[fns].y,&ba[fns].w) == 2) nc++;
				else energy += ba[fns].w;
				fns++;
				break;
			default: usage();
		}
	}

	const char* outfile = NULL;
	if(argc - optind)
		outfile = argv[optind];
	else if(isatty(STDOUT_FILENO))
		outfile = "sixel:-";
	else usage();

	for(int i = 0; i < fns; i++)
		if(ba[i].w == -1) ba[i].w = (1-energy)/nc;
	coeff* coefs = fftw(alloc_real)(bs.w*bs.h);
	memset(coefs,0,sizeof(*coefs)*bs.w*bs.h);

	for(int i = 0; i < fns; i++)
		coefs[ba[i].y*bs.w+ba[i].x] = ba[i].w/4;
	coefs[0] += mc(0.5);
	free(ba);

	fftw(plan) p = fftw(plan_r2r_2d)(bs.h,bs.w,coefs,coefs,FFTW_REDFT01,FFTW_REDFT01,FFTW_ESTIMATE);
	fftw(execute)(p);
	fftw(destroy_plan)(p);

	MagickWandGenesis();
	MagickWand* wand = NewMagickWand();
	MagickConstituteImage(wand,bs.w,bs.h,"I",TypePixel,coefs);
	MagickWriteImage(wand,outfile);
	DestroyMagickWand(wand);
	MagickWandTerminus();

	fftw(free)(coefs);
	fftw(cleanup)();

	return 0;
}
