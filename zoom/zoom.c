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

static double* generate_scaled_basis(enum scaling_type scaling_type, long double scale_num, long double scale_den, long double offset, size_t nvectors, size_t ncomponents, size_t sampling_len) {
	double* basis = malloc(nvectors*(ncomponents-1)*sizeof(*basis));
	if(!basis)
		return NULL;

	for(size_t b = 0; b < nvectors; b++)
		for(size_t n = 1; n < ncomponents; n++) {
			long double k, N;
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
			basis[b*(ncomponents-1)+n-1] = cos(M_PIl * (k+0.5) * n / N);
		}
	return basis;
}

static void usage(const char* self) {
	fprintf(stderr,"Usage: %s [-s <scale> -p <pos> -v <size> --basis <type> --showsamples[=<type>] -c -g -P] <input> <output>\n", self);
	exit(1);
}
static void help(const char* self) {
	fprintf(stderr,
		"Usage: %s [options] <input> <output>\n"
		"\n"
		"  -h, --help  This help text.\n"
		"  -s <scale>  Rational or decimal scale factor.\n"
		"  -p <pos>    Floating point offset in image, in the form XxY (e.g. 100.0x100.0). Coordinates are in terms of the scaled output unless -P is set\n"
		"  -v <size>   Output view size in WxH.\n"
		"  -c          Anchor view to center of image\n"
		"  -P          Position coordinates with -p are relative to the input rather than the scaled output\n"
		"  -g          Scale in linear RGB\n"
		"\n"
		"  --showsamples[=<type>]  Show where integer coordinates in the input are located in the scaled image.\n"
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
	long double vx = 0, vy = 0;
	size_t vw = 0, vh = 0;
	bool centered = false, input_coords = false, gamma = false;
	int showsamples = 0;
	long double scale_num = 1;
	unsigned long long scale_den = 1;
	int scaling_type = INTERPOLATED;
	int c;
	const struct option opts[] = {
		{"help",no_argument,NULL,'h'},
		{"showsamples",optional_argument,NULL,1},
		{"basis",required_argument,NULL,2},
		{0}
	};

	while((c = getopt_long(argc,argv,"hs:v:p:cgaP",opts,NULL)) != -1) {
		switch(c) {
			case  0 : break;
			case 'h': help(argv[0]);
			case 's': sscanf(optarg,"%Lf/%llu",&scale_num,&scale_den); break;
			case 'v': sscanf(optarg,"%zux%zu",&vw,&vh); break;
			case 'p': sscanf(optarg,"%Lfx%Lf",&vx,&vy); break;
			case 'c': centered = true; break;
			case 'P': input_coords = true; break;
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
	if(!vw) vw = width*scale_num/scale_den;
	if(!vh) vh = height*scale_num/scale_den;

	double* coeffs = malloc(sizeof(double)*width*height*3);
	MagickExportImagePixels(wand,0,0,width,height,"RGB",DoublePixel,coeffs);
	DestroyMagickWand(wand);

	if(input_coords || centered) {
		vx *= scale_num/scale_den;
		vy *= scale_num/scale_den;
		if(centered) {
			vx -= vw/2.L;
			vy -= vh/2.L;
		}
	}

	long double scale = scale_num / scale_den;
	size_t cwidth  = min(width,  round(width  * scale_num/scale_den)),
	       cheight = min(height, round(height * scale_num/scale_den));

	double* icoeffs = malloc(vw*vh*3*sizeof(*icoeffs));
	double* tmp = malloc(sizeof(double)*cheight);

	double* basis[2] = {
		generate_scaled_basis(scaling_type,scale_num,scale_den,vx,vw,cwidth,width),
		generate_scaled_basis(scaling_type,scale_num,scale_den,vy,vh,cheight,height)
	};

	fftw_plan p = fftw_plan_many_r2r(2,(int[]){height,width},3,coeffs,NULL,3,1,coeffs,NULL,3,1,(fftw_r2r_kind[]){FFTW_REDFT10,FFTW_REDFT10},FFTW_ESTIMATE);
	fftw_execute(p);
	fftw_destroy_plan(p);

	for(int z = 0; z < 3; z++) {
		for(int i = 0; i < vw; i++) {
			for(int row = 0; row < cheight; row++) {
				tmp[row] = coeffs[row*width*3+z]/2;
				for(int u = 1; u < cwidth; u++)
					tmp[row] += coeffs[(row*width+u)*3+z] * basis[0][i*(cwidth-1)+u-1];
			}
			for(int j = 0; j < vh; j++) {
				double s = tmp[0]/2;
				for(int v = 1; v < cheight; v++)
					s += tmp[v] * basis[1][j*(cheight-1)+v-1];
				s /= width*height;
				icoeffs[(j*vw+i)*3+z] = s;
			}
		}
	}

	if(showsamples == POINT)
		for(size_t y = scale-(size_t)vy%(int)scale; y < vh; y+=scale)
			for(size_t x = scale-(size_t)vx%(int)scale; x < vw; x+=scale)
				memcpy(icoeffs+(y*vh+x)*3,((double[]){0,1,0}),3*sizeof(*icoeffs));
	else if(showsamples == GRID) {
		for(size_t y = scale-(size_t)vy%(int)scale; y < vh; y+=scale)
			for(size_t x = 0; x < vw; x++)
				memcpy(icoeffs+(y*vh+x)*3,((double[]){0,1,0}),3*sizeof(*icoeffs));
		for(size_t y = 0; y < vh; y++)
			for(size_t x = scale-(size_t)vx%(int)scale; x < vw; x+=scale)
				memcpy(icoeffs+(y*vh+x)*3,((double[]){0,1,0}),3*sizeof(*icoeffs));
	}

	wand = NewMagickWand();
	MagickConstituteImage(wand,vw,vh,"RGB",DoublePixel,icoeffs);
	if(gamma) {
		MagickSetImageColorspace(wand,RGBColorspace);
		MagickTransformImageColorspace(wand,sRGBColorspace);
	}
	MagickWriteImage(wand,outfile);

	free(coeffs);
	free(icoeffs);
	free(tmp);
	free(basis[0]);
	free(basis[1]);

	DestroyMagickWand(wand);
	MagickWandTerminus();

	return 0;
}
