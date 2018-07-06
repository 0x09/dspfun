/*
 * scan - progressively reconstruct images using various frequency space scans.
 * Copyright 2018 0x09.net.
 */

#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <unistd.h>
#include <getopt.h>
#include <ctype.h>

#include <fftw3.h>

#include "scan.h"
#include "ffapi.h"
#include "magickwand.h"
#include "speclib.h"

static inline coeff* generate_basis_matrix(size_t n) {
	coeff* basis = malloc(sizeof(*basis)*(n)*n);
	for(size_t k = 0; k < n; k++) {
		basis[k*n] = 1;
		for(size_t j = 1; j < n; j++)
			basis[k*n+j] = mi(2.) * mi(cos)(M_PI*j*(k+mi(0.5))/n);
	}
	return basis;
}

static inline void pruned_idct(coeff* restrict basis[2], coeff* restrict coeffs, coeff* restrict image, size_t (*coords)[2], size_t ncoords, size_t width, size_t height, size_t channels) {
	for(size_t y = 0; y < height; y++)
		for(size_t x = 0; x < width; x++)
			for(size_t z = 0; z < channels; z++)
				image[(y*width+x)*channels+z] = coeffs[(coords[0][0]*width+coords[0][1])*channels+z] * basis[0][y*height+coords[0][0]] * basis[1][x*width+coords[0][1]];
	if(ncoords > 1)
		for(size_t y = 0; y < height; y++)
			for(size_t x = 0; x < width; x++)
				for(size_t n = 1; n < ncoords; n++)
					for(size_t z = 0; z < channels; z++)
						image[(y*width+x)*channels+z] += coeffs[(coords[n][0]*width+coords[n][1])*channels+z] * basis[0][y*height+coords[n][0]] * basis[1][x*width+coords[n][1]];
}

static inline intermediate srgb_encode(intermediate x) {
	 return x <= mi(0.00313066844250063) ?  mi(12.92)*x : mi(1.055)*mi(pow)(x,1/mi(2.4)) - mi(0.055);
}

void help(bool fullhelp) {
	fprintf(stderr,
		"usage: scan <options> input output\n"
		"options:\n"
		"   -h|--help\n"
		"   -H|--fullhelp\n"
		"   -m|--method <name>                scan method\n"
		"   -o|--options <optstring>          scan-specific options\n"
		"   -v|--visualize                    show scan in frequency-space\n"
		"   -s|--spectrogram                  show scan over image spectrogram (implies -v)\n"
		"   -i|--intermediates                show transform intermediates for current index (stacks with -v/-s)\n"
		"   -M|--max-intermediates            use full range for transform intermediates (implies -i)\n"
		"   -S|--step <int>                   number of scan iterations per frame of output\n"
		"   -I|--invert                       invert scan order\n"
		"   -n|--frames <int>                 limit the number of frames of output\n"
		"   -O|--offset <int>                 offset into scan to start at\n"
		"      --skip                         don't fill previous scan indexes when jumping to an offset with --offset\n"
		"   -g|--linear                       operate in linear light\n"
		"   -p|--pruned-idct <bool>           use built-in pruned idct instead of fftw, faster on small scan intervals (default: auto based on scan interval)\n"
		"   -f|--serialization-file <path>    serialize scan to file\n"
		"   -t|--serialization-format <fmt>   scan format to serialize (with -f)\n"
		"\n"
		"ffmpeg options:\n"
		"   --ff-format <avformat>   output format\n"
		"   --ff-encoder <avcodec>   output codec\n"
		"   --ff-rate <rate>         output framerate\n"
		"   --ff-opts <optstring>    output av options string (k=v:...)\n"
		"   --ff-loglevel <-8..64>   av loglevel\n"
		"\n"
		"spec options:\n"
		"   --spec-gain <float>       spectrogram log multiplier (with -s)\n"
		"   --spec-opts <optstring>   spectrogram options string (k=v:...) (with -s)\n"
		"\n"
	);
	if(!fullhelp)
		exit(0);

	int max_len = 0;
	for(struct scan_method* m = scan_methods(); m; m = scan_method_next(m)) {
		int len = strlen(scan_method_name(m));
		if(len > max_len)
			max_len = len;
	}
	fprintf(stderr,"%-*s - options\n", max_len+3, "scan methods");
	for(struct scan_method* m = scan_methods(); m; m = scan_method_next(m)) {
		fprintf(stderr, "   %-*s", max_len, scan_method_name(m));
		const char* opts;
		if((opts = scan_method_options(m)))
			fprintf(stderr, " - %s", opts);
		fprintf(stderr,"\n");
	}
	fprintf(stderr,"\n");

	fprintf(stderr,"serialization formats:\n");
	for(const char** k = scan_serialization_keys(); *k; k++)
		fprintf(stderr,"   %s\n", *k);
	fprintf(stderr,"\n");

	fprintf(stderr,"spectrogram option string keys and values:\n");
	for(const char** k = spec_options(); *k; k++) {
		fprintf(stderr,"   %s = ", *k);
		const char** v = spec_option_values(*k);
		fprintf(stderr,"%s", *v);
		for(++v; *v; v++)
			fprintf(stderr,", %s", *v);
		fprintf(stderr,"\n");
	}
	fprintf(stderr,"\n");
	exit(0);
}


int main(int argc, char* argv[]) {
	int ret = 0;
	AVRational fps = {20,1};
	const char* oopt = NULL,* ofmt = NULL,* enc = NULL;
	int loglevel = 0;
	const char* method = "diag",* scan_options = NULL,* serialized_scan;
	size_t nframes = 0, offset = 0;
	bool spec = false, invert = false, intermediates = false, linear = false, max_intermediates = false, visualize = false, fill_offset = true;
	int use_fftw = -1;
	intermediate gain = 0;
	struct spec_params sparams = {0};
	enum scan_serialization serialization_format = 0;
	size_t step = 1;
	int opt;
	int longoptind = 0;
	const struct option gopts[] = {
		{"help",no_argument,NULL,'h'},
		{"fullhelp",no_argument,NULL,'H'},
		{"method",required_argument,NULL,'m'},
		{"options",required_argument,NULL,'o'},
		{"visualize",no_argument,NULL,'v'},
		{"spectrogram",no_argument,NULL,'s'},
		{"intermediates",no_argument,NULL,'i'},
		{"max-intermediates",no_argument,NULL,'M'},
		{"step",required_argument,NULL,'S'},
		{"invert",no_argument,NULL,'I'},
		{"frames",required_argument,NULL,'n'},
		{"offset",required_argument,NULL,'O'},
		{"skip",no_argument,NULL,1},
		{"linear",no_argument,NULL,'g'},
		{"pruned-idct",required_argument,NULL,'p'},
		{"serialization-file",required_argument,NULL,'f'},
		{"serialization-format",required_argument,NULL,'t'},

		// ffapi opts
		{"ff-opts",required_argument,NULL,2},
		{"ff-format",required_argument,NULL,3},
		{"ff-encoder",required_argument,NULL,4},
		{"ff-loglevel",required_argument,NULL,5},
		{"ff-rate",required_argument,NULL,6},

		// spec opts
		{"spec-gain",required_argument,NULL,7},
		{"spec-opts",required_argument,NULL,8},
		{0}
	};

	while((opt = getopt_long(argc,argv,"hHm:o:vsiMS:In:O:gp:f:t:",gopts,&longoptind)) != -1)
		switch(opt) {
			case 'h':
			case 'H': help(opt == 'H'); break;
			case 'm': method = optarg; break;
			case 'n': nframes = strtoull(optarg,NULL,10); break;
			case 's': spec = true;
			case 'v': visualize = true; break;
			case 'S': step = strtoull(optarg,NULL,10); break;
			case 'I': invert = true; break;
			case 'o': scan_options = optarg;
			case 'i': intermediates = true; break;
			case 'g': linear = true; break;
			case 'M': intermediates = max_intermediates = true; break;
			case 'p': use_fftw = strcmp("true",optarg); break;
			case 'f': serialized_scan = optarg; break;
			case 't': {
				if(!(serialization_format = scan_serialization_val(optarg))) {
					fprintf(stderr,"Invalid serialization format. Options:\n");
					for(const char** k = scan_serialization_keys(); *k; k++)
						fprintf(stderr,"%s\n", *k);
					exit(1);
				}
			} break;
			case 'O': offset = strtol(optarg,NULL,10); break;
			case 1: fill_offset = false; break;

			case 2: oopt = optarg; break;
			case 3: ofmt = optarg; break;
			case 4: enc  = optarg; break;
			case 5: loglevel = strtol(optarg, NULL, 10); break;
			case 6: av_parse_video_rate(&fps, optarg); break;

			case 7: gain = strtold(optarg,NULL); break;
			case 8: {
				const char* e = spec_params_parse(&sparams,optarg,"=",":");
				if(e) {
					fprintf(stderr,"Couldn't parse spec option starting at: %s\nOptions: \n", e);
					for(const char** k = spec_options(); *k; k++) {
						fprintf(stderr,"   %s = ", *k);
						const char** v = spec_option_values(*k);
						fprintf(stderr,"%s", *v);
						for(++v; *v; v++)
							fprintf(stderr,", %s", *v);
						fprintf(stderr,"\n");
					}

					exit(1);
				}
			} break;
			default : help(false);
		}
	argv += optind;
	argc -= optind;
	if(!argc)
		help(false);

	struct scan_method* m = scan_method_find_prefix(method);
	if(!m) {
		fprintf(stderr, "Invalid method '%s'. Choose one of:\n", method);
		for(m = scan_methods(); m; m = scan_method_next(m))
			fprintf(stderr, "%s\n", scan_method_name(m));
		return 0;
	}

	MagickWandGenesis();
	MagickWand* wand = NewMagickWand();
	if(!MagickReadImage(wand,argv[0])) {
		fprintf(stderr,"Unable to read image '%s'\n",argv[0]);
		MagickWandTerminus();
		return 1;
	}
	size_t width = MagickGetImageWidth(wand), height = MagickGetImageHeight(wand), channels = 3;

	unsigned char blackpixel[channels];
	memset(blackpixel,0,channels);

	av_log_set_level(loglevel);
	FFColorProperties color_props;
	ffapi_parse_color_props(&color_props,"");
	color_props.pix_fmt = AV_PIX_FMT_RGB24;
	color_props.color_range = AVCOL_RANGE_JPEG;
	if(linear || MagickGetImageColorspace(wand) == sRGBColorspace) {
		color_props.color_trc = AVCOL_TRC_IEC61966_2_1;
		color_props.color_space = AVCOL_SPC_RGB;
		color_props.color_primaries = AVCOL_PRI_BT709;
	}
	if(linear)
		MagickTransformImageColorspace(wand,RGBColorspace);

	coeff* coeffs = fftw(malloc)(sizeof(*coeffs)*width*height*channels);
	MagickExportImagePixels(wand,0,0,width,height,"RGB",TypePixel,coeffs);
	DestroyMagickWand(wand);
	MagickWandTerminus();

	fftw(plan) forward = fftw(plan_many_r2r)(2,(int[2]){height,width},channels,coeffs,NULL,channels,1,coeffs,NULL,channels,1,(fftw(r2r_kind)[2]){FFTW_REDFT10,FFTW_REDFT10},FFTW_ESTIMATE);
	fftw(execute)(forward);
	fftw(destroy_plan)(forward);

	// normalize to non-uniform range -1..1
	for(size_t i = 0; i < width*height*channels; i++)
		coeffs[i] /= width*height*4;

	struct scan_context* scanctx = scan_init(m,width,height,channels,coeffs,scan_options);
	if(!scanctx) {
		ret = 1;
		fprintf(stderr, "Error initializing scan\n");
		goto fftw_end;
	}

	if(serialized_scan) {
		FILE* f = fopen(serialized_scan,"w");
		scan_serialize(scanctx, f, serialization_format);
		fclose(f);
	}
	if(argc <= 1)
		goto scan_end;

	FFContext* ffctx = ffapi_open_output(argv[1], oopt, ofmt, enc, AV_CODEC_ID_FFV1, &color_props, width*(!!visualize+1), height*(!!intermediates+1), fps);
	if(!ffctx) {
		ret = 1;
		fprintf(stderr, "Error opening output context\n");
		goto scan_end;
	}

	AVFrame* frame = ffapi_alloc_frame(ffctx);
	if(!frame) {
		ret = 1;
		fprintf(stderr, "Couldn't allocate output frame\n");
		goto ffapi_end;
	}

	size_t max_interval = scan_max_interval(scanctx);
	size_t limit = scan_limit(scanctx);

	size_t (*coords)[2] = malloc(sizeof(*coords)*max_interval*step);
	if(!nframes || nframes > limit/step)
		nframes = (limit+step-1)/step;
	if(use_fftw < 0)
		use_fftw = step*max_interval > log2(width*height);
	use_fftw |= fill_offset;

	coeff* reconstruction = fftw(malloc)(sizeof(*reconstruction)*width*height*channels);
	coeff* image = fftw(malloc)(sizeof(*image)*width*height*channels);
	memset(reconstruction,0,sizeof(*reconstruction)*width*height*channels);

	fftw(plan) inverse;
	coeff* basis[2];
	if(use_fftw)
		inverse = fftw(plan_many_r2r)(2,(int[2]){height,width},channels,reconstruction,NULL,channels,1,image,NULL,channels,1,(fftw(r2r_kind)[2]){FFTW_REDFT01,FFTW_REDFT01},FFTW_MEASURE);
	else {
		basis[0] = generate_basis_matrix(height);
		basis[1] = width == height ? basis[0] : generate_basis_matrix(width);
	}

	struct spec_scaler* sp = NULL;
	if(spec) {
		if(!gain)
			gain = mi(127.5)*mi(sqrt)(width*height*4);
		coeff max = coeffs[0];
		for(size_t i = 1; i < channels; i++)
			if(coeffs[i] > max)
				max = coeffs[i];

		sp = spec_create(&sparams,max*spec_normalization_2d(0,0),gain);
	}

	coeff* sum = calloc(width*height*channels,sizeof(*sum));

	ffapi_clear_frame(frame);

	// include DC in sum unconditionally in case 0,0 isn't the first coord
	for(size_t i = 0; i < width*height; i++)
		memcpy(sum+i*channels,coeffs,sizeof(*sum)*channels);

	if(offset >= limit)
		offset = limit-1;

	// jump ahead in the scan filling previous elements
	if(fill_offset) {
		for(size_t i = 0; i < offset; i++) {
			size_t j = (invert ? limit-i-1 : i);
			scan(scanctx,j,coords);
			size_t ncoords = scan_interval(scanctx,j);
			for(size_t ci = 0; ci < ncoords; ci++) {
				size_t y = coords[ci][0], x = coords[ci][1];
				memcpy(reconstruction+(y*width+x)*channels,coeffs+(y*width+x)*channels,sizeof(*reconstruction)*channels);
				if(visualize) {
					intermediate normalization = spec_normalization_2d(x,y);
					for(size_t z = 0; z < channels; z++) {
						intermediate c = (spec ? spec_scale(sp,coeffs[(y*width+x)*channels+z]*normalization) : 1.0)*255;
						ffapi_setpel(ffctx,frame,x+width,y,z,c);
					}
				}
			}
		}
		memset(reconstruction,0,sizeof(*coeffs)*channels);
		fftw(execute)(inverse);
		for(size_t y = 0; y < height; y++)
			for(size_t x = 0; x < width; x++)
				for(size_t z = 0; z < channels; z++) {
					sum[(y*width+x)*channels+z] += image[(y*width+x)*channels+z];
					intermediate pel = sum[(y*width+x)*channels+z];
					if(linear)
						pel = srgb_encode(pel);
					ffapi_setpel(ffctx, frame, x, y, z, pel*255);
				}
	}

	int pad = log10f(nframes/step)+1;
	for(size_t i = offset; i < offset+nframes; i++) {
		size_t ncoords = 0;
		for(size_t s = i*step; s < i*step+step && s < limit; s++) {
			size_t j = (invert ? limit-s-1 : s);
			scan(scanctx,j,coords+ncoords);
			ncoords += scan_interval(scanctx,j);
		}

		memset(reconstruction,0,sizeof(*reconstruction)*width*height*channels);
		for(size_t ci = 0; ci < ncoords; ci++) {
			size_t y = coords[ci][0], x = coords[ci][1];
			memcpy(reconstruction+(y*width+x)*channels,coeffs+(y*width+x)*channels,sizeof(*reconstruction)*channels);
			if(visualize) {
				intermediate normalization = spec_normalization_2d(x,y);
				for(size_t z = 0; z < channels; z++) {
					intermediate c = (spec ? spec_scale(sp,coeffs[(y*width+x)*channels+z]*normalization) : 1.0)*255;
					ffapi_setpel(ffctx,frame,x+width,y,z,c);
					if(intermediates)
						ffapi_setpel(ffctx,frame,x+width,y+height,z,c);
				}
			}
		}

		// clear DC, it's already been included
		memset(reconstruction,0,sizeof(*coeffs)*channels);
		if(use_fftw)
			fftw(execute)(inverse);
		else
			pruned_idct(basis, reconstruction, image, coords, ncoords, width, height, channels);

		for(size_t y = 0; y < height; y++)
			for(size_t x = 0; x < width; x++)
				for(size_t z = 0; z < channels; z++) {
					sum[(y*width+x)*channels+z] += image[(y*width+x)*channels+z];
					intermediate pel = sum[(y*width+x)*channels+z];
					if(linear)
						pel = srgb_encode(pel);
					ffapi_setpel(ffctx, frame, x, y, z, pel*255);
				}

		if(intermediates) {
			coeff max[channels], min[channels];
			if(max_intermediates) {
				for(size_t z = 0; z < channels; z++)
					max[z] = min[z] = image[z];
				for(size_t j = 1; j < width*height; j++)
					for(size_t z = 0; z < channels; z++) {
						coeff c = image[j*channels+z];
						if(c > max[z]) max[z] = c;
						else if(c < min[z]) min[z] = c;
					}
				for(size_t z = 0; z < channels; z++) {
					max[z] = (max[z]+coeffs[z]);
					min[z] = (min[z]+coeffs[z]);
				}
			}
			else
				for(size_t z = 0; z < channels; z++) {
					min[z] = 0;
					max[z] = 1;
				}

			for(size_t y = 0; y < height; y++)
				for(size_t x = 0; x < width; x++)
					for(size_t z = 0; z < channels; z++) {
						intermediate pel = (((image[(y*width+x)*channels+z]+coeffs[z])-min[z])/(max[z]-min[z]));
						if(linear)
							pel = srgb_encode(pel);
						ffapi_setpel(ffctx, frame, x, y+height, z, pel*255);
					}
		}

		ffapi_write_frame(ffctx, frame);
		fprintf(stderr, "\r%*zu / %zu", pad, i-offset, nframes);

		// just clear intermediate coords instead of wiping the entire frame
		if(intermediates && visualize)
			for(size_t ci = 0; ci < ncoords; ci++)
				ffapi_setpixel(ffctx, frame, coords[ci][1]+width, coords[ci][0]+height, blackpixel);
	}
	fprintf(stderr,"\n");

	free(sum);
	spec_destroy(sp);

	if(use_fftw)
		fftw(destroy_plan)(inverse);
	else {
		free(basis[0]);
		if(width != height)
			free(basis[1]);
	}

	fftw(free)(image);
	fftw(free)(reconstruction);
	free(coords);

ffapi_end:
	ffapi_free_frame(frame);
	ffapi_close(ffctx);

scan_end:
	scan_destroy(scanctx);

fftw_end:
	fftw(free)(coeffs);
	fftw(cleanup)();

	return ret;
}
