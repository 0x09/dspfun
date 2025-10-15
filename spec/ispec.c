/*
 * spec - Generate invertible frequency spectrums for viewing and editing.
 * Copyright 2014-2016 0x09.net.
 */

#include "spec.h"

int main(int argc, char* argv[]) {
	int ret = 0;
	bool preserve_dc = false;
	int opt;
	const char* signmap = NULL;

	struct specopts opts = spec_opt_defaults;
	while((opt = getopt(argc,argv,SPEC_OPT_FLAGS "pm:h")) > 0) {
		switch(opt) {
			case 'p': preserve_dc = true; break;
			case 'm': signmap = optarg; break;
			case 'h':
				printf("Usage: %s [options] <infile> <outfile>\n",argv[0]);
				printf("options:\n"
				"  -h            This help text.\n"
				"  -p            Apply the DC coeffient from the original image if available.\n"
				"  -m <signmap>  Path to a file generated with `spec -t sign`\n\n");
				printf("spectrogram options:\n");
				spec_help();
				return 0;
			default: {
				int err = spec_opt_proc(&opts,opt,optarg);
				if(err) {
					fprintf(stderr,"Usage: %s -h -p -m <signmap> ",argv[0]);
					spec_usage(stderr);
					fprintf(stderr," <infile> <outfile>\n");
					return 1;
				}
			}
		}
	}

	argc -= optind;
	argv += optind;
	opts.input = opts.output = "-";
	if(argc > 0) opts.input  = argv[0];
	if(argc > 1) opts.output = argv[1];
	else if(isatty(STDOUT_FILENO))
		opts.output = "sixel:-";

	size_t l, w, h, d;
	size_t i, y, z;
	MagickWandGenesis();
	MagickWand* wand = NewMagickWand();
	MagickReadImage(wand,opts.input);
	w = MagickGetImageWidth(wand), h = MagickGetImageHeight(wand), d = strlen(opts.csp), l = w*h*d;

	coeff max[d];
	double DC[d];
	char* dcprop = MagickGetImageProperty(wand,"DC");
	if(dcprop) {
		base16dec(dcprop,DC,sizeof(DC));
		MagickRelinquishMemory(dcprop);
	}
	else if(!signmap && (preserve_dc || opts.params.rangetype == rangetype_dc || opts.params.rangetype == rangetype_dcs)) {
		fprintf(stderr,"DC not found in header\n");
		ret = 1;
		goto end;
	}
//	MagickSetImageProperty(wand,"spectype",enum_key(spectype,opts.type));

	coeff* f = fftw(alloc_real)(l);
	MagickExportImagePixels(wand,0,0,w,h,opts.csp,TypePixel,f);
	DestroyMagickWand(wand);

	switch(opts.params.signtype) {
		case signtype_none:
		case signtype_abs:
		if(signmap) {
			wand = NewMagickWand();
			MagickReadImage(wand,signmap);
			unsigned char* tmp = malloc(l);
			MagickExportImagePixels(wand,0,0,w,h,opts.csp,CharPixel,tmp);
			for(i = 0; i < d; i++)
				DC[i] = tmp[i]/mi(255.);
			for(i = d; i < l; i++)
				f[i] = mc(copysign)(f[i],tmp[i]-128);
			free(tmp);
			DestroyMagickWand(wand);
		}
		break;
		case signtype_shift:
		for(i = 0; i < l; i++)
			f[i] = (f[i]*mi(255.)/254-mi(0.5))*2;
		break;
		case signtype_saturate:
		for(i = d; i < l; i++)
			f[i] = f[i]*2-1;
		break;
		case signtype_retain: break;
	}

	intermediate gain;
	switch(opts.params.gaintype) {
		case gaintype_none:
		case gaintype_native:    gain = mi(127.5) * mi(sqrt)(w*h*4); break;
		case gaintype_reference: gain = mi(127.5) * 1024; break;
		case gaintype_custom:    gain = opts.gain; break;
	}

	switch(opts.params.rangetype) {
	case rangetype_one: *max = gain; break;
	case rangetype_none:
	case rangetype_dc:
		for(*max = DC[0]*gain, z = 1; z < d; z++)
			if(DC[z]*gain > *max)
				*max = DC[z]*gain;
		break;
	case rangetype_dcs:
		for(z = 0; z < d; z++)
			max[z] = DC[z]*gain;
		break;
	}
	if(opts.params.rangetype != rangetype_dcs)
		for(z = 1; z < d; z++)
			max[z] = max[0];

	switch(opts.params.scaletype) {
		case scaletype_none:
		case scaletype_log:
			for(z = 0; z < d; z++)
				max[z] = log1p(max[z]);
			for(i = 0; i < l; i++)
				f[i] = mi(copysign)(mi(expm1)(mc(fabs)(f[i]*max[i%d])),f[i]);
			break;
		case scaletype_linear:
			for(i = 0; i < l; i++)
				f[i] *= max[i%d];
			break;
	}

	for(i = 0; i < l; i++)
		f[i] /= gain;

	for(size_t xz = 0; xz < w*d; xz++)
		f[xz] *= mi(M_SQRT2);
	for(y = 0; y < h; y++)
		for(z = 0; z < d; z++)
			f[y*w*d+z] *= mi(M_SQRT2); //coeffs in non-uniform range -2..2
	for(i = 0; i < l; i++)
		f[i]/=2;

	if(preserve_dc)
		for(z = 0; z < d; z++)
			f[z] = DC[z];

	fftw(plan) p = fftw(plan_many_r2r)(2,(int[]){h,w},d,f,NULL,d,1,f,NULL,d,1,(fftw_r2r_kind[]){FFTW_REDFT01,FFTW_REDFT01},FFTW_ESTIMATE);
	fftw(execute)(p);
	fftw(destroy_plan)(p);


	wand = NewMagickWand();
	MagickConstituteImage(wand,w,h,opts.csp,TypePixel,f);
	fftw(free)(f);
	if(opts.gamma) {
		MagickSetImageColorspace(wand,RGBColorspace);
		MagickTransformImageColorspace(wand,sRGBColorspace);
	}
end:
	MagickWriteImage(wand,opts.output);
	DestroyMagickWand(wand);
	MagickWandTerminus();
	return ret;
}
