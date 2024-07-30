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
#include <libavutil/csp.h>
#include <libavutil/pixdesc.h>

#include "magickwand.h"
#include "longmath.h"
#include "precision.h"
#include "ffapi.h"

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

static size_t generate_scaled_basis(coeff** basis, enum scaling_type scaling_type, intermediate scale_num, intermediate scale_den, intermediate offset, size_t nvectors, size_t sampling_len) {
	if(sampling_len*scale_num/scale_den < 1) {
		scale_num = 1;
		scale_den = sampling_len;
	}
	size_t ncomponents = min(sampling_len,round(sampling_len*scale_num/scale_den));
	*basis = malloc(nvectors*(ncomponents-1)*sizeof(*basis));
	if(!basis)
		return 0;

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
			(*basis)[b*(ncomponents-1)+n-1] = mi(cos)(mi(M_PI) * (k+mi(0.5)) * n / N);
		}

	return ncomponents;

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
		"\n"
		"ffmpeg options:\n"
		"   --ff-format <avformat>  output format\n"
		"   --ff-encoder <avcodec>  output codec\n"
		"   --ff-rate <rate>        output framerate\n"
		"   --ff-opts <optstring>   output av options string (k=v:...)\n"
		"   --ff-loglevel <-8..64>  av loglevel\n"
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

	AVRational fps = {60,1};
	const char* oopt = NULL,* ofmt = NULL,* enc = NULL;
	int loglevel = 0;

	int c;
	const struct option opts[] = {
		{"help",no_argument,NULL,'h'},
		{"showsamples",optional_argument,NULL,1},
		{"basis",required_argument,NULL,2},

		// ffapi opts
		{"ff-opts",required_argument,NULL,3},
		{"ff-format",required_argument,NULL,4},
		{"ff-encoder",required_argument,NULL,5},
		{"ff-loglevel",required_argument,NULL,6},
		{"ff-rate",required_argument,NULL,7},
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
			case 3: oopt = optarg; break;
			case 4: ofmt = optarg; break;
			case 5: enc  = optarg; break;
			case 6: loglevel = strtol(optarg, NULL, 10); break;
			case 7: av_parse_video_rate(&fps, optarg); break;
			default: usage(argv[0]);
		}
	}

	argc -= optind;
	if(argc < 1)
		usage(argv[0]);

	const char* infile = argv[optind],* outfile = NULL;
	if(argc > 1)
		outfile = argv[optind+1];
	else usage(argv[0]);

	av_log_set_level(loglevel);

	MagickWandGenesis();
	MagickWand* wand = NewMagickWand();
	if(MagickReadImage(wand,infile) == MagickFalse) {
		DestroyMagickWand(wand);
		MagickWandTerminus();
		return 1;
	}

	FFColorProperties color_props;
	ffapi_parse_color_props(&color_props,NULL);
	ColorspaceType imagecolorspace = MagickGetImageColorspace(wand);
	if(gamma)
		MagickTransformImageColorspace(wand,RGBColorspace);
	else if(imagecolorspace == RGBColorspace)
		gamma = true;
	if(gamma || imagecolorspace == sRGBColorspace) {
		color_props.color_trc = AVCOL_TRC_IEC61966_2_1;
		color_props.color_space = AVCOL_SPC_RGB;
		color_props.color_primaries = AVCOL_PRI_BT709;
	}
	color_props.pix_fmt = AV_PIX_FMT_GBRPF32LE;
	color_props.color_range = AVCOL_RANGE_JPEG;

	size_t width = MagickGetImageWidth(wand), height = MagickGetImageHeight(wand);
	coeff* coeffs = fftw(alloc_real)(width*height*3);
	MagickExportImagePixels(wand,0,0,width,height,"RGB",TypePixel,coeffs);
	DestroyMagickWand(wand);
	MagickWandTerminus();

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
	size_t maxvectors = vh > vw ? vh : vw;

	FFContext* ffctx = ffapi_open_output(outfile, oopt, ofmt, enc, AV_CODEC_ID_FFV1, &color_props, vw, vh, fps);
	if(!ffctx) {
		fprintf(stderr,"Error opening output context\n");
		return 1;
	}

	av_csp_trc_function trc_encode = gamma ? av_csp_trc_func_from_id(ffctx->codec->color_trc) : NULL;

	AVFrame* frame = ffapi_alloc_frame(ffctx);

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

	coeff* xbasis,* ybasis;
	bool reuse_basis = width == height && vx == vy && xscale_num == yscale_num && xscale_den == yscale_den;
	size_t cwidth = generate_scaled_basis(&xbasis,scaling_type,xscale_num,xscale_den,vx,(reuse_basis ? maxvectors : vw),width);
	size_t cheight;
	if(reuse_basis) {
		ybasis = xbasis;
		cheight = cwidth;
	}
	else
		cheight = generate_scaled_basis(&ybasis,scaling_type,yscale_num,yscale_den,vy,vh,height);

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

	for(size_t y = 0; y < vh; y++)
		for(size_t x = 0; x < vw; x++)
			for(int z = 0; z < 3; z++) {
				coeff pel = icoeffs[(y*vw+x)*3+z];
				if(trc_encode)
					pel = trc_encode(pel);
				ffapi_setpelf(ffctx, frame, x, y, z, pel);
			}

	ffapi_write_frame(ffctx, frame);

	free(icoeffs);
	ffapi_free_frame(frame);
	ffapi_close(ffctx);

	return 0;
}
