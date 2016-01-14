/*
 * spec - Generate invertible frequency spectrums for viewing and editing.
 * Copyright 2014-2016 0x09.net.
 */

#include "spec.h"

int main(int argc, char* argv[]) {
	struct specopts opts = spec_args(argc,argv,"");
	if(opts.help) {
		fprintf(stderr,"Usage: %s %s <infile> <outfile>\n",argv[0],opts.help);
		return 0;
	}

	size_t l, w, h, d;
	size_t i, x, y, z;
	MagickWandGenesis();
	MagickWand* wand = NewMagickWand();
	MagickReadImage(wand,opts.input);
	w = MagickGetImageWidth(wand), h = MagickGetImageHeight(wand), d = strlen(opts.csp), l = w*h*d;
	if(opts.gamma) MagickTransformImageColorspace(wand,RGBColorspace);

	coeff* f = fftw(alloc_real)(l);
	MagickExportImagePixels(wand,0,0,w,h,opts.csp,TypePixel,f);
	DestroyMagickWand(wand);

	fftw(plan) p = fftw(plan_many_r2r)(2,(int[]){h,w},d,f,NULL,d,1,f,NULL,d,1,(fftw_r2r_kind[]){FFTW_REDFT10,FFTW_REDFT10},FFTW_ESTIMATE);
	fftw(execute)(p); //coeffs in range -w*h*4..w*h*4
	fftw(destroy_plan)(p);
	double DC[d];
	for(z = 0; z < d; z++)
		DC[z] = f[z]/(w*h*4);

	for(size_t xz = 0; xz < w*d; xz++)
		f[xz] /= mi(M_SQRT2);
	for(y = 0; y < h; y++)
		for(z = 0; z < d; z++)
			f[y*w*d+z] /= mi(M_SQRT2); //coeffs in uniform range -w*h*2..w*h*2

	intermediate norm = w*h*2; //puts in range -1..1
	for(i = 0; i < l; i++)
		f[i]/=norm;


	intermediate gain;
	switch(opts.params.gaintype) {
		case gaintype_none:
		case gaintype_native: gain = mi(127.5) * mi(sqrt)(w*h*4); break;
		case gaintype_lenna:  gain = mi(127.5) * 1024; break;
		case gaintype_custom: gain = opts.gain; break;
	}

	for(i = 0; i < l; i++)
		f[i] *= gain;

	coeff max[d];
	switch(opts.params.rangetype) {
		case rangetype_one: *max = gain; break;
		case rangetype_none:
		case rangetype_dc:
			for(*max = f[0], z = 1; z < d; z++)
				if(f[z] > *max)
					*max = f[z];
			break;
		case rangetype_dcs:
			for(z = 0; z < d; z++)
				max[z] = f[z];
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
				f[i] = mi(copysign)(mi(log1p)(mc(fabs)(f[i])),f[i])/max[i%d];
			break;
		case scaletype_linear:
			for(i = 0; i < l; i++)
				f[i] /= max[i%d];
			break;
	}

	switch(opts.params.signtype) {
		case signtype_none:
		case signtype_abs:
			for(i = 0; i < l; i++)
				f[i] = mc(fabs)(f[i]);
			break;
		case signtype_shift:
			for(i = 0; i < l; i++)
				f[i] = (f[i]/mi(2.)+mi(0.5))*(mi(254.)/255);
			break;
		case signtype_saturate:
			for(i = d; i < l; i++)
				f[i] = !signbit(f[i]);
			break;
	}

	wand = NewMagickWand();
	MagickConstituteImage(wand,w,h,opts.csp,TypePixel,f);
	fftw(free)(f);

	//if(opts.gamma) MagickSetImageColorspace(wand,RGBColorspace);
	char tmp[sizeof(DC)*2+1];
	tmp[sizeof(tmp)-1]='\0';
	base16enc(DC,tmp,sizeof(DC));
	MagickSetImageProperty(wand,"DC",tmp);
//	MagickSetImageProperty(wand,"spectype",enum_key(spectype,opts.type));
	MagickWriteImage(wand,opts.output);
	DestroyMagickWand(wand);
	MagickWandTerminus();
}
