/*
 * zoom - Interpolate images with a cosine basis at arbitrary scales/offsets.
 * Copyright 2013-2016 0x09.net.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <getopt.h>
#include <unistd.h>

#include <fftw3.h>

#include "magickwand.h"
#include "longmath.h"
#include "precision.h"

enum scaling_type {
	INTERPOLATED = 0,
	CENTERED,
	NATIVE
};

enum sample_display {
	NONE = 0,
	POINT,
	GRID
};

#define min(x,y) ((x) < (y) ? (x) : (y))

static coeff* generate_scaled_basis(enum scaling_type scaling_type, intermediate scale_num, intermediate scale_den, intermediate offset, size_t nvectors, size_t ncomponents, size_t sampling_len) {
	coeff* basis = malloc(nvectors*(ncomponents-1)*sizeof(*basis));
	if(!basis)
		return NULL;

	for(size_t b = 0; b < nvectors; b++)
		for(size_t n = 1; n < ncomponents; n++) {
			intermediate k, N;
			switch(scaling_type) {
				case NATIVE:
					k = b+offset;
					N = sampling_len * scale_num/scale_den;
					break;
				case INTERPOLATED:
					k = (b+offset) * scale_den/scale_num;
					N = sampling_len;
					break;
				case CENTERED:
					k = (b+offset) * (sampling_len-1) * scale_den / (sampling_len * scale_num - scale_den);
					N = sampling_len;
					break;
			}
			basis[b*(ncomponents-1)+n-1] = mi(cos)(mi(M_PI) * (k+mi(0.5)) * n / N);
		}
	return basis;
}

static void usage(const char* self) {
	fprintf(stderr,"Usage: %s [(-s <scale> | -r <res>) -p <pos> -v <size> --basis <type> --showsamples[=<type>] -c -g -P -%%] <input> <output>\n", self);
	exit(1);
}
static void help(const char* self) {
	fprintf(stderr,
		"Usage: %s [options] <input> <output>\n"
		"\n"
		"  -h, --help  This help text.\n"
		"  -s <scale>  Rational or decimal scale factor. May be a single value or XxY to specify horizontal/veritcal scaling factors.\n"
		"  -r <res>    Logical resolution in the form WxH. May be fractional. Takes precedence over -s.\n"
		"  -p <pos>    Floating point offset in image, in the form XxY (e.g. 100.0x100.0). Coordinates are in terms of the scaled output unless -P is set\n"
		"  -v <size>   Output view size in WxH.\n"
		"  -c          Anchor view to center of image\n"
		"  -P          Position coordinates with -p are relative to the input rather than the scaled output\n"
		"  -%%          Position coordinates with -p are a percent value rather than a number of samples\n"
		"  -g          Scale in linear RGB\n"
		"\n"
		"  --showsamples[=<type>]  Show where integer coordinates in the input are located in the scaled image when upscaling.\n"
		"                          type: point (default), grid.\n"
		"\n"
		"  --basis <type>  Set the boundaries of the interpolated basis functions. [default: interpolated]\n"
		"                  type:\n"
		"                    interpolated: even around half of a sample of the scaled output\n"
		"                    native: even around half of a sample of the input before scaling\n"
		"                    centered: the first and last samples of the input correspond to the first and last samples of the output\n"
		"\n", self
	);
	exit(0);
}

int main(int argc, char* argv[]) {
	intermediate vx = 0, vy = 0;
	size_t vw = 0, vh = 0;
	bool centered = false, input_coords = false, pct_coords = false, gamma = false;
	int showsamples = 0;
	intermediate xscale_num = 1, yscale_num = 1;
	intermediate logical_width = 0, logical_height = 0;
	unsigned long long xscale_den = 1, yscale_den = 1;
	int scaling_type = INTERPOLATED;
	int c;
	const struct option opts[] = {
		{"help",no_argument,NULL,'h'},
		{"showsamples",optional_argument,NULL,1},
		{"basis",required_argument,NULL,2},
		{0}
	};

	while((c = getopt_long(argc,argv,"hs:v:p:cgaPr:%",opts,NULL)) != -1) {
		switch(c) {
			case  0 : break;
			case 'h': help(argv[0]);
			case 's': {
				int n;
				if(sscanf(optarg,"%" INTERMEDIATE_SPECIFIER "%n/%llu%n",&xscale_num,&n,&xscale_den,&n) <= 0)
					usage(argv[0]);
				optarg += n;
				if(!*optarg) {
					yscale_num = xscale_num;
					yscale_den = xscale_den;
					break;
				}
				if(sscanf(optarg,"x%" INTERMEDIATE_SPECIFIER "/%llu",&yscale_num,&yscale_den) <= 0)
					usage(argv[0]);
				break;
			}
			case 'r': sscanf(optarg,"%" INTERMEDIATE_SPECIFIER "x%" INTERMEDIATE_SPECIFIER,&logical_width,&logical_height); break;
			case 'v': sscanf(optarg,"%zux%zu",&vw,&vh); break;
			case 'p': sscanf(optarg,"%" INTERMEDIATE_SPECIFIER "x%" INTERMEDIATE_SPECIFIER,&vx,&vy); break;
			case 'c': centered = true; break;
			case 'P': input_coords = true; break;
			case '%': pct_coords = true; break;
			case 'g': gamma = true; break;
			case 1: {
				showsamples = POINT;
				if(optarg) {
					if(!strcmp(optarg,"grid"))
						showsamples = GRID;
					else if(strcmp(optarg,"point"))
						usage(argv[0]);
				}
			} break;
			case 2: {
				if(!strcmp(optarg,"centered"))
					scaling_type = CENTERED;
				else if(!strcmp(optarg,"native"))
					scaling_type = NATIVE;
				else if(strcmp(optarg,"interpolated"))
					usage(argv[0]);
			}; break;
			default: usage(argv[0]);
		}
	}

	argc -= optind;
	if(argc < 1)
		usage(argv[0]);

	const char* infile = argv[optind],* outfile = NULL;
	if(argc > 1)
		outfile = argv[optind+1];
	else if(isatty(STDOUT_FILENO))
		outfile = "sixel:-";
	else usage(argv[0]);

	MagickWandGenesis();
	MagickWand* wand = NewMagickWand();
	if(MagickReadImage(wand,infile) == MagickFalse) {
		DestroyMagickWand(wand);
		MagickWandTerminus();
		return 1;
	}
	if(gamma)
		MagickTransformImageColorspace(wand,RGBColorspace);

	size_t width = MagickGetImageWidth(wand), height = MagickGetImageHeight(wand);
	coeff* coeffs = fftw(alloc_real)(width*height*3);
	MagickExportImagePixels(wand,0,0,width,height,"RGB",TypePixel,coeffs);
	DestroyMagickWand(wand);

	fftw(plan) p = fftw(plan_many_r2r)(2,(int[]){height,width},3,coeffs,NULL,3,1,coeffs,NULL,3,1,(fftw_r2r_kind[]){FFTW_REDFT10,FFTW_REDFT10},FFTW_ESTIMATE);
	fftw(execute)(p);
	fftw(destroy_plan)(p);
	fftw(cleanup)();

	if(logical_width) {
		xscale_num = logical_width;
		xscale_den = width;
	}
	if(logical_height) {
		yscale_num = logical_height;
		yscale_den = height;
	}

	if(width*xscale_num/xscale_den < 1) {
		xscale_num = 1;
		xscale_den = width;
	}
	if(height*yscale_num/yscale_den < 1) {
		yscale_num = 1;
		yscale_den = height;
	}

	intermediate xscale = xscale_num / xscale_den, yscale = yscale_num / yscale_den;
	if(showsamples && (xscale < 1 || yscale < 1)) {
		fprintf(stderr,"warning: downscaling requested, --showsamples will be disabled\n");
		showsamples = NONE;
	}

	if(!vw)
		vw = width*xscale_num/xscale_den;
	if(!vh)
		vh = height*yscale_num/yscale_den;

	if(pct_coords) {
		vx *= vw/100;
		vy *= vy/100;
	}
	else if(input_coords) {
		vx *= xscale_num/xscale_den;
		vy *= yscale_num/yscale_den;
	}
	else if(centered) {
		vx = (width*xscale_num/xscale_den-vw)/2;
		vy = (height*yscale_num/yscale_den-vh)/2;
	}

	size_t cwidth  = min(width,  round(width  * xscale_num/xscale_den)),
	       cheight = min(height, round(height * yscale_num/yscale_den));

	coeff* xbasis,* ybasis;
	xbasis = generate_scaled_basis(scaling_type,xscale_num,xscale_den,vx,vw,cwidth,width);
	if(width == height && vx == vy && xscale_num == yscale_num && xscale_den == yscale_den)
		ybasis = xbasis;
	else
		ybasis = generate_scaled_basis(scaling_type,yscale_num,yscale_den,vy,vh,cheight,height);

	coeff* icoeffs = malloc(vw*vh*3*sizeof(*icoeffs));
	intermediate* tmp = malloc(sizeof(*tmp)*cheight);

	for(int z = 0; z < 3; z++) {
		for(size_t i = 0; i < vw; i++) {
			for(size_t row = 0; row < cheight; row++) {
				tmp[row] = ((intermediate)coeffs[row*width*3+z])/2;
				for(size_t u = 1; u < cwidth; u++)
					tmp[row] += coeffs[(row*width+u)*3+z] * xbasis[i*(cwidth-1)+u-1];
			}
			for(size_t j = 0; j < vh; j++) {
				intermediate s = tmp[0]/2;
				for(size_t v = 1; v < cheight; v++)
					s += tmp[v] * ybasis[j*(cheight-1)+v-1];
				icoeffs[(j*vw+i)*3+z] = s / (width*height);
			}
		}
	}

	free(tmp);
	fftw(free)(coeffs);
	if(ybasis != xbasis)
		free(ybasis);
	free(xbasis);

	if(showsamples == POINT)
		for(size_t y = yscale-(size_t)vy%(int)yscale; y < vh; y+=yscale)
			for(size_t x = xscale-(size_t)vx%(int)xscale; x < vw; x+=xscale)
				memcpy(icoeffs+(y*vh+x)*3,((coeff[]){0,1,0}),3*sizeof(*icoeffs));
	else if(showsamples == GRID) {
		for(size_t y = yscale-(size_t)vy%(int)yscale; y < vh; y+=yscale)
			for(size_t x = 0; x < vw; x++)
				memcpy(icoeffs+(y*vh+x)*3,((coeff[]){0,1,0}),3*sizeof(*icoeffs));
		for(size_t y = 0; y < vh; y++)
			for(size_t x = xscale-(size_t)vx%(int)xscale; x < vw; x+=xscale)
				memcpy(icoeffs+(y*vh+x)*3,((coeff[]){0,1,0}),3*sizeof(*icoeffs));
	}

	wand = NewMagickWand();
	MagickConstituteImage(wand,vw,vh,"RGB",TypePixel,icoeffs);
	free(icoeffs);

	if(gamma) {
		MagickSetImageColorspace(wand,RGBColorspace);
		MagickTransformImageColorspace(wand,sRGBColorspace);
	}
	MagickWriteImage(wand,outfile);
	DestroyMagickWand(wand);
	MagickWandTerminus();

	return 0;
}
