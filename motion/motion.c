/*
 * motion - apply various 2- or 3-dimensional frequency-domain operations to an image or video.
 * Copyright 2012-2016 0x09.net.
 */

#include <fftw3.h>
#include <getopt.h>
#include <stdbool.h>
#include <libavutil/eval.h>

#include "ffapi.h"

long double sinc(long double x) {
	return cos(M_PI*x)/2+0.5;
	x*=M_PI;
	return x?(sin(x)/(x)):1;
}
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define MAX(x,y) ((x) > (y) ? (x) : (y))

static void seek_progress(size_t seek) {
	fprintf(stderr,"\rseek: %zu",seek);
}

struct coords {	unsigned long long w, h, d; };
typedef struct coords coords[4];
typedef struct range { coords begin, end; } range;
static void propagate_planes(coords c, const coords subsample_factors) {
	for(int i = 0; i < 4; i++) {
		if(!c[i].w) c[i].w = -((-((int)c[0].w)) >> subsample_factors[i].w);
		if(!c[i].h) c[i].h = -((-((int)c[0].h)) >> subsample_factors[i].h);
		if(!c[i].d) c[i].d = -((-((int)c[0].d)) >> subsample_factors[i].d);
	}
}
static void fill_coords(const coords src, coords dest) {
	for(int i = 0; i < 4; i++, src++, dest++) {
		if(!dest->w) dest->w = src->w;
		if(!dest->h) dest->h = src->h;
		if(!dest->d) dest->d = src->d;
	}
}
static void limit_coords(const coords src, coords dest) {
	for(int i = 0; i < 4; i++, src++, dest++) {
		if(src->w < dest->w) dest->w = src->w;
		if(src->h < dest->h) dest->h = src->h;
		if(src->d < dest->d) dest->d = src->d;
	}
}
#define match_planes(left,right) (left.w == right.w && left.h == right.h && left.d == right.d)
#define print_coords(x) printf("%llux%llux%llu\n",x->w,x->h,x->d)
#undef print_coords
#define print_coords(x) do {\
for(int i1 = 0; i1 < components; i1++)\
	fprintf(stderr,"%llu%s",x[i1].w,i1 == components -1? " x " : ":");\
for(int i1 = 0; i1 < components; i1++)\
	fprintf(stderr,"%llu%s",x[i1].h,i1 == components -1? " x " : ":");\
for(int i1 = 0; i1 < components; i1++)\
	fprintf(stderr,"%llu%s",x[i1].d,i1 == components -1? "\n" : ":");\
} while(0)

int parse_fftw_flag(const char* arg) {
	if(!strcasecmp(arg,"estimate"))
		return FFTW_ESTIMATE;
	if(!strcasecmp(arg,"measure"))
		return FFTW_MEASURE;
	if(!strcasecmp(arg,"patient"))
		return FFTW_PATIENT;
	if(!strcasecmp(arg,"exhaustive"))
		return FFTW_EXHAUSTIVE;
	return -1;
}

static void usage() {
	fprintf(stderr,"Usage: motion -i infile [-o outfile]\n"
	               "[-s|--size WxHxD] [-b|--blocksize WxHxD] [-p|--bandpass X1xY1xZ1-X2xY2xZ2]\n"
	               "[-B|--boost float] [-D|--damp float]  [--spectrogram=type] [-q|--quant quant] [-d|--dither] [--preserve-dc=type] [--eval expression]\n"
	               "[--fftw-planning-method method] [--fftw-wisdom-file file]\n"
	               "[--keep-rate] [--samesize-chroma] [--frames lim] [--offset pos] [--csp|c colorspace options] [--iformat|--format fmt] [--codec codec] [--encopts|--decopts opts]\n");
	exit(0);
}
int main(int argc, char* argv[]) {
	int opt;
	int longoptind = 0;
	char* infile = NULL,* outfile = NULL,* colorspace = NULL,* iformat = NULL,* format = NULL,* encoder = NULL,* decopts = NULL,* encopts = NULL,* exprstr = NULL,* fftw_wisdom_file = NULL;
	coords block = {0}, scaled = {0};
	unsigned long long int offset = 0, maxframes = 0;
	int samerate = false, samesize = false, spec = 0, dithering = false;
	range bandpass = {0};
	float boost[4] = {1,1,1,1};
	float damp[4] = {0,0,0,0};
	float quant = 0;
	int shell = 0, preserve_dc = 0, fftw_flags = FFTW_ESTIMATE;
	int loglevel = AV_LOG_ERROR;
	const struct option gopts[] = {
		{"size",required_argument,NULL,'s'},
		{"blocksize",required_argument,NULL,'b'},
		{"offset",required_argument,NULL,2},
		{"frames",required_argument,NULL,3},
		{"keep-rate",no_argument,&samerate,1},
		{"samesize-chroma",no_argument,&samesize,1},
		{"spectrogram",optional_argument,NULL,4},
		{"bandpass",required_argument,NULL,'p'},
		{"boost",required_argument,NULL,'B'},
		{"damp",required_argument,NULL,'D'},
		{"quant",required_argument,NULL,'q'},
		{"dither",no_argument,&dithering,1},
		{"csp",required_argument,NULL,'c'},
		{"format",required_argument,NULL,5},
		{"codec",required_argument,NULL,6},
		{"encopts",required_argument,NULL,7},
		{"iformat",required_argument,NULL,8},
		{"decopts",required_argument,NULL,9},
		{"shell",no_argument,&shell,1},
		{"loglevel",required_argument,NULL,10},
		{"preserve-dc",optional_argument,NULL,11},
		{"eval",required_argument,NULL,12},
		{"fftw-planning-method",required_argument,NULL,13},
		{"fftw-wisdom-file",required_argument,NULL,14},
		{0}
	};
	while((opt = getopt_long(argc,argv,"i:o:b:s:p:B:D:c:q:rP:",gopts,&longoptind)) != -1)
		switch(opt) {
			case 'i': infile = optarg; break;
			case 'o': outfile = optarg; break;
			case 'b': sscanf(optarg,"%llux%llux%llu",&block->w,&block->h,&block->d); break;
			case 's': sscanf(optarg,"%llux%llux%llu",&scaled->w,&scaled->h,&scaled->d); break;
			case 'p': sscanf(optarg,"%llux%llux%llu-%llux%llux%llu",&bandpass.begin->w,&bandpass.begin->h,&bandpass.begin->d,&bandpass.end->w,&bandpass.end->h,&bandpass.end->d); break;
			case 'B': for(int i = sscanf(optarg,"%f:%f:%f:%f",boost,boost+1,boost+2,boost+3); i < 4; i++) boost[i] = i ? boost[i-1] : 1; break;
			case 'D': for(int i = sscanf(optarg,"%f:%f:%f:%f",damp,damp+1,damp+2,damp+3); i < 4; i++) damp[i] = i ? damp[i-1] : 0; break;
			case 'c': colorspace = optarg; break;
			case  2 : offset = strtoull(optarg,NULL,10); break;
			case  3 : maxframes = strtoull(optarg,NULL,10); break;
			case  4 : spec = 1;
			          if(optarg) spec = strtol(optarg,NULL,10);
			          break;
			case  5 : format = optarg; break;
			case  6 : encoder = optarg; break;
			case  7 : encopts = optarg; break;
			case 'q': quant = strtod(optarg,NULL); break;
			case  8 : iformat = optarg; break;
			case  9 : decopts = optarg; break;
			case 10 : loglevel = strtol(optarg,NULL,10); break;
			case 11 : preserve_dc = optarg && !strcmp(optarg,"grey") ? 2 : 1; break;
			case 12 : exprstr = optarg; break;
			case 13 :
				if((fftw_flags = parse_fftw_flag(optarg)) < 0) {
					fprintf(stderr, "invalid FFTW flag, use one of: estimate, measure, patient, exhaustive");
					exit(1);
				}; break;
			case 14 : fftw_wisdom_file = optarg; break;
			case  0 : if(gopts[longoptind].flag != NULL) break;
			default : usage();
		}
	if(!infile) usage();

	// Setup input
	av_log_set_level(loglevel);

	coords source = {0};
	AVRational r_frame_rate;
	FFColorProperties color_props;
	ffapi_parse_color_props(&color_props, colorspace);
	if(spec > 0) {
		if(color_props.pix_fmt == AV_PIX_FMT_NONE)
			color_props.pix_fmt = AV_PIX_FMT_RGB24;
		if(color_props.color_range == AVCOL_RANGE_UNSPECIFIED)
			color_props.color_range = AVCOL_RANGE_JPEG;
	}
	unsigned long w[4], h[4], components;
	FFContext* in = ffapi_open_input(infile,decopts,iformat,&color_props,&components,&w,&h,(unsigned long*)&source->d,&r_frame_rate,!shell && (!outfile || !maxframes));
	if(!in) {
		fprintf(stderr, "Error opening \"%s\"\n", infile);
		return 1;
	}
	source->w = *w; //compute subsampling separately
	source->h = *h;
	AVPixFmtDescriptor pixdesc = *(in->pixdesc);

	if(maxframes) {
		if(source->d && maxframes + offset > source->d) {
			if(maxframes > source->d) maxframes = source->d;
			if(offset >= source->d) offset = source->d - maxframes;
			else maxframes = source->d - offset;
		}
		source->d = maxframes;
	}
	else {
		if(offset >= source->d) offset = source->d - 1;
		source->d -= offset;
	}
	coords subsample_factors = {{0,0,0},{pixdesc.log2_chroma_w,pixdesc.log2_chroma_h,0},{pixdesc.log2_chroma_w,pixdesc.log2_chroma_h,0},{0,0,0}};
	propagate_planes(source,subsample_factors);
	if(!shell) { fprintf(stderr,"  source: ");print_coords(source); }

	if(!shell && !outfile) {
		ffapi_close(in);
		return 0;
	}

	if(samesize) {
		if(block->w <= source[1].w) {
			subsample_factors[1].w = 0;
			if(!block->w) block->w = source[1].w;
		}
		if(block->h <= source[1].h) {
			subsample_factors[1].h = 0;
			if(!block->h) block->h = source[1].h;
		}
		if(block->w <= source[2].w) {
			subsample_factors[2].w = 0;
			if(!block->w) block->w = source[2].w;
		}
		if(block->h <= source[2].h) {
			subsample_factors[2].h = 0;
			if(!block->h) block->h = source[2].h;
		}
	}

	propagate_planes(block,subsample_factors);
	propagate_planes(scaled,subsample_factors);
	propagate_planes(bandpass.begin,subsample_factors);
	propagate_planes(bandpass.end,subsample_factors);

	fill_coords(source,block);
	limit_coords(source,block);
	fill_coords(block,scaled);
	fill_coords(block,bandpass.end);
	limit_coords(block,bandpass.begin);
	limit_coords(block,bandpass.end);

	if(!shell && (source->w % block->w || source->h % block->h || source->d % block->d))
	 	fprintf(stderr,"Warning: Blocks not evenly divisible, truncating dimensions\n");

	coords nblocks, truncated, newres;
	for(int i = 0; i < components; i++) {
		nblocks[i].w = source[i].w / block[i].w;
		nblocks[i].h = source[i].h / block[i].h;
		nblocks[i].d = source[i].d / block[i].d;

		newres[i].w = nblocks[i].w * scaled[i].w;
		newres[i].h = nblocks[i].h * scaled[i].h;
		newres[i].d = nblocks[i].d * scaled[i].d;

		truncated[i].w = nblocks[i].w * block[i].w;
		truncated[i].h = nblocks[i].h * block[i].h;
		truncated[i].d = nblocks[i].d * block[i].d;
	}

	AVRational scale = {1,1};
	if(!samerate)
		av_reduce(&scale.num,&scale.den,scaled->d,block->d,INT_MAX);
	r_frame_rate = av_mul_q(r_frame_rate,scale);
	if(shell) {
		printf(
			"w=%llu h=%llu fps_num=%d fps_den=%d pixel_format=%s color_range=%s color_primaries=%s color_trc=%s colorspace=%s chroma_sample_location=%s",
			newres->w,newres->h,r_frame_rate.num,r_frame_rate.den,pixdesc.name,
			av_color_range_name(color_props.color_range),
			av_color_primaries_name(color_props.color_primaries),
			av_color_transfer_name(color_props.color_trc),
			av_color_space_name(color_props.color_space),
			av_chroma_location_name(color_props.chroma_location)
		);
		ffapi_close(in);
		return 0;
	}

	fprintf(stderr,"   using: ");print_coords(truncated);
	fprintf(stderr,"   block: ");print_coords(block);
	fprintf(stderr,"bp begin: ");print_coords(bandpass.begin);
	fprintf(stderr,"bp   end: ");print_coords(bandpass.end);
	fprintf(stderr,"  scaled: ");print_coords(scaled);
	fprintf(stderr," nblocks: ");print_coords(nblocks);
	fprintf(stderr," outsize: ");print_coords(newres);
	fprintf(stderr,"\n");

	// Setup output
	FFContext* out = ffapi_open_output(outfile,encopts,format,encoder,AV_CODEC_ID_FFV1,&color_props,newres->w,newres->h,r_frame_rate);
	if(!out) {
		fprintf(stderr,"Output setup failed for '%s' / '%s'\n",outfile,format);
		ffapi_close(in);
		return 1;
	}

	fprintf(stderr,"pixel_format %s --> %s --> %s\n",av_get_pix_fmt_name(in->codec->pix_fmt),pixdesc.name,av_get_pix_fmt_name(out->codec->pix_fmt));
	fprintf(stderr,"color_range %s --> %s --> %s\n",av_color_range_name(in->codec->color_range),av_color_range_name(color_props.color_range),av_color_range_name(out->codec->color_range));
	fprintf(stderr,"color_primaries %s --> %s --> %s\n",av_color_primaries_name(in->codec->color_primaries),av_color_primaries_name(color_props.color_primaries),av_color_primaries_name(out->codec->color_primaries));
	fprintf(stderr,"color_trc %s --> %s --> %s\n",av_color_transfer_name(in->codec->color_trc),av_color_transfer_name(color_props.color_trc),av_color_transfer_name(out->codec->color_trc));
	fprintf(stderr,"colorspace %s --> %s --> %s\n",av_color_space_name(in->codec->colorspace),av_color_space_name(color_props.color_space),av_color_space_name(out->codec->colorspace));
	fprintf(stderr,"chroma_sample_location %s --> %s --> %s\n",av_chroma_location_name(in->codec->chroma_sample_location),av_chroma_location_name(color_props.chroma_location),av_chroma_location_name(out->codec->chroma_sample_location));

	AVExpr* expr = NULL;
	const char* names[] = {"c","x","y","z","i","width","height","depth","components","bx","by","bz","bwidth","bheight","bdepth",NULL};
	if(exprstr && av_expr_parse(&expr,exprstr,names,NULL,NULL,NULL,NULL,0,NULL) < 0)
		return 1;

	// Seeking
	if(offset) {
		ffapi_seek_frame(in, offset, seek_progress);
		fprintf(stderr,"\n");
	}
	// Main loop
	coords minbuf, active;
	size_t mincomponent = 0;
	for(int i = 0; i < components; i++) {
		minbuf[i].w = MAX(block[i].w,scaled[i].w);
		minbuf[i].h = MAX(block[i].h,scaled[i].h);
		minbuf[i].d = MAX(block[i].d,scaled[i].d);
		active[i].w = MIN(block[i].w,scaled[i].w);
		active[i].h = MIN(block[i].h,scaled[i].h);
		active[i].d = MIN(block[i].d,scaled[i].d);

		if(minbuf[i].w*minbuf[i].h*minbuf[i].d > mincomponent) mincomponent = minbuf[i].w*minbuf[i].h*minbuf[i].d;
	}
	float* coeffs = fftwf_malloc(sizeof(float)*mincomponent);
	unsigned char** pixels[components];
	for(int i = 0; i < components; i++) {
		pixels[i] = malloc(sizeof(char*)*nblocks[i].w*nblocks[i].h);
		for(int b = 0; b < nblocks[i].h * nblocks[i].w; b++)
			pixels[i][b] = malloc(minbuf[i].w*minbuf[i].h*minbuf[i].d);
	}

	if(fftw_wisdom_file)
		fftwf_import_wisdom_from_filename(fftw_wisdom_file);

	int unique_plans = 0;
	fftwf_plan plans[components*2];
	fftwf_plan planforward[components];
	fftwf_plan planinverse[components];
	for(int i = 0; i < components; i++) {
		if(spec >= 0) {
			planforward[i] = NULL;
			for(int j = 0; j < i; j++)
				if(match_planes(block[i],block[j]) && match_planes(minbuf[i],minbuf[j])) {
					planforward[i] = planforward[j];
					break;
				}
			if(!planforward[i])
				plans[unique_plans++] = planforward[i] =
					fftwf_plan_many_r2r(3,(const int[3]){block[i].d,block[i].h,block[i].w},1,
						coeffs,(const int[3]){minbuf[i].d,minbuf[i].h,minbuf[i].w},1,0,
						coeffs,(const int[3]){minbuf[i].d,minbuf[i].h,minbuf[i].w},1,0,
						(const fftw_r2r_kind[3]){FFTW_REDFT10,FFTW_REDFT10,FFTW_REDFT10},fftw_flags);
		}
		if(spec <= 0) {
			planinverse[i] = NULL;
			for(int j = 0; j < i; j++)
				if(match_planes(scaled[i],scaled[j]) && match_planes(minbuf[i],minbuf[j])) {
					planinverse[i] = planinverse[j];
					break;
				}
			if(!planinverse[i])
				plans[unique_plans++] = planinverse[i] =
					fftwf_plan_many_r2r(3,(const int[3]){scaled[i].d,scaled[i].h,scaled[i].w},1,
						coeffs,(const int[3]){minbuf[i].d,minbuf[i].h,minbuf[i].w},1,0,
						coeffs,(const int[3]){minbuf[i].d,minbuf[i].h,minbuf[i].w},1,0,
						(const fftw_r2r_kind[3]){FFTW_REDFT01,FFTW_REDFT01,FFTW_REDFT01},fftw_flags);
		}
	}

	if(fftw_wisdom_file)
		fftwf_export_wisdom_to_filename(fftw_wisdom_file);

	long double scalefactor[components];
	long double normalization[components];
	long double c[components];
	float quantizer[components];
	for(int i = 0; i < components; i++) {
		scalefactor[i] = (scaled[i].w*scaled[i].h*scaled[i].d)/(long double)(block[i].w*block[i].h*block[i].d);
		normalization[i] = 1/sqrtl(scaled[i].w*scaled[i].h*scaled[i].d*8);
		if(spec && spec != 1) c[i] = 127.5/logl(scaled[i].w*scaled[i].h*scaled[i].d*255*8+1);
		quantizer[i] = (quant*8.L*sqrtl(scaled[i].w*scaled[i].h*scaled[i].d));
	}

	int padb = log10f(source->d)+1, pads = log10f(newres->d)+1;
	fprintf(stderr,"read: %*d wrote: %*d",padb,0,pads,0);
	AVFrame* readframe = ffapi_alloc_frame(in);
	AVFrame* writeframe = ffapi_alloc_frame(out);
	unsigned long long coeffs_coded = 0;
	for(int bz = 0; bz < nblocks->d; bz++) {
		for(int z = 0; z < block->d; z++) {
			int error = ffapi_read_frame(in,readframe);
			for(int i = 0; i < components; i++) {
				if(bz >= nblocks[i].d || z >= block[i].d) continue;
				AVComponentDescriptor comp = pixdesc.comp[i];
				for(int by = 0; by < nblocks[i].h; by++)
					for(int bx = 0; bx < nblocks[i].w; bx++)
						for(int y = 0; y < block[i].h; y++)
							for(int x = 0; x < block[i].w; x++)
								pixels[i][by*nblocks[i].w+bx][(z*minbuf[i].h+y)*minbuf[i].w+x] = ffapi_getpel_direct(readframe,bx*block[i].w+x,by*block[i].h+y,comp);
			}
			fprintf(stderr,"\rread: %*llu wrote: %*llu",padb,bz*block->d+z+1,pads,bz*scaled->d);
		}
		for(int i = 0; i < components; i++) {
			if(bz >= nblocks[i].d) continue;
			for(int b = 0; b < nblocks[i].h * nblocks[i].w; b++) {
				unsigned char* pblock = pixels[i][b];
				memset(coeffs,0,sizeof(float)*mincomponent);
				for(int z = 0; z < block[i].d; z++)
					for(int y = 0; y < block[i].h; y++)
						for(int x = 0; x < block[i].w; x++) {
							unsigned char pel = pblock[(z*minbuf[i].h+y)*minbuf[i].w+x];
							if(spec < 0)
								coeffs[(z*minbuf[i].h+y)*minbuf[i].w+x] = copysignl((powl(M_E,fabsl((pel-127.5L)/c[i]))-1),pel-127.5L);
							else
								coeffs[(z*minbuf[i].h+y)*minbuf[i].w+x] = pel;
						}
				if(spec >= 0) fftwf_execute(planforward[i]);
				float dc = coeffs[0];

				if(expr)
					for(int z = 0; z < active[i].d; z++)
						for(int y = 0; y < active[i].h; y++)
							for(int x = 0; x < active[i].w; x++) {
								double vals[] = {
									coeffs[(z*minbuf[i].h+y)*minbuf[i].w+x]*normalization[i]*normalization[i]/255,
									x, y, z, i, block[i].w, block[i].h, block[i].d, components,
									b%nblocks[i].w, b/nblocks[i].h, bz, nblocks[i].w, nblocks[i].h, nblocks->d,
									0
								};
								coeffs[(z*minbuf[i].h+y)*minbuf[i].w+x] = av_expr_eval(expr,vals,NULL)/(normalization[i]*normalization[i])*255;
							}

				if(damp[i] != 1) {
					if(bandpass.begin[i].d) // front
						for(long long z = 0; z < bandpass.begin[i].d; z++)
							for(long long y = 0; y < active[i].h; y++)
								for(long long x = 0; x < active[i].w; x++)
									coeffs[(z*minbuf[i].h+y)*minbuf[i].w+x] *= damp[i];
					if(bandpass.end[i].d < active[i].d) // back
						for(long long z = bandpass.end[i].d; z < active[i].d; z++)
							for(long long y = 0; y < active[i].h; y++)
								for(long long x = 0; x < active[i].w; x++)
									coeffs[(z*minbuf[i].h+y)*minbuf[i].w+x] *= damp[i];
					if(bandpass.begin[i].h) // top
						for(long long z = bandpass.begin[i].d; z < bandpass.end[i].d; z++)
							for(long long y = 0; y < bandpass.begin[i].h; y++)
								for(long long x = 0; x < active[i].w; x++)
									coeffs[(z*minbuf[i].h+y)*minbuf[i].w+x] *= damp[i];
					if(bandpass.end[i].h < active[i].h) // bottom
						for(long long z = bandpass.begin[i].d; z < bandpass.end[i].d; z++)
							for(long long y = bandpass.end[i].h; y < active[i].h; y++)
								for(long long x = 0; x < active[i].w; x++)
									coeffs[(z*minbuf[i].h+y)*minbuf[i].w+x] *= damp[i];
					if(bandpass.begin[i].w) // left
						for(long long z = bandpass.begin[i].d; z < bandpass.end[i].d; z++)
							for(long long y = bandpass.begin[i].h; y < bandpass.end[i].h; y++)
								for(long long x = 0; x < bandpass.begin[i].w; x++)
									coeffs[(z*minbuf[i].h+y)*minbuf[i].w+x] *= damp[i];
					if(bandpass.end[i].w < active[i].w) //right
						for(long long z = bandpass.begin[i].d; z < bandpass.end[i].d; z++)
							for(long long y = bandpass.begin[i].h; y < bandpass.end[i].h; y++)
								for(long long x = bandpass.end[i].w; x < active[i].w; x++)
									coeffs[(z*minbuf[i].h+y)*minbuf[i].w+x] *= damp[i];
				}
				if(boost[i] != 1)
					for(long long z = bandpass.begin[i].d; z < bandpass.end[i].d; z++)
						for(long long y = bandpass.begin[i].h; y < bandpass.end[i].h; y++)
							for(long long x = bandpass.begin[i].w; x < bandpass.end[i].w; x++)
								coeffs[(z*minbuf[i].h+y)*minbuf[i].w+x] *= boost[i];

				if(preserve_dc) {
					bool dcstop = bandpass.begin[i].d || bandpass.begin[i].h || bandpass.begin[i].w;
					if(expr || dcstop || boost[i] != 1) {
						if(preserve_dc < 2)
							coeffs[0] = dc;
						else
							coeffs[0] += (1-(dcstop ? damp[i] : boost[i])) * 127.5/(normalization[i]*normalization[i]*scalefactor[i]);
					}
				}

				if(quant)
					for(int z = 0; z < active[i].d; z++)
						for(int y = 0; y < active[i].h; y++)
							for(int x = 0; x < active[i].w; x++) {
								float q = quantizer[i] * (z?1:M_SQRT2) * (y?1:M_SQRT2) * (x?1:M_SQRT2);
								coeffs_coded += !!(coeffs[(z*minbuf[i].h+y)*minbuf[i].w+x] = roundf(coeffs[(z*minbuf[i].h+y)*minbuf[i].w+x] / q)*q);
							}
				if(spec <= 0) fftwf_execute(planinverse[i]);
				else if(spec == 1) c[i] = 255/logl(fabsl(dc * scalefactor[i] * normalization[i])+1);
				for(int z = 0; z < scaled[i].d; z++)
					for(int y = 0; y < scaled[i].h; y++)
						for(int x = 0; x < scaled[i].w; x++) {
							long double pel = coeffs[(z*minbuf[i].h+y)*minbuf[i].w+x] * scalefactor[i];
							if(spec > 0) {
								if(spec == 1)
									pblock[(z*minbuf[i].h+y)*minbuf[i].w+x] = round(c[i]*logl(fabsl(pel*normalization[i])+1));
								else
									pblock[(z*minbuf[i].h+y)*minbuf[i].w+x] = round(c[i]*copysignl(logl(fabsl(pel)+1),pel)+127.5);
							}
							else {
								pel *= normalization[i]*normalization[i];
								pblock[(z*minbuf[i].h+y)*minbuf[i].w+x] = pel > 255 ? 255 : pel < 0 ? 0 : round(pel);
								if(dithering) {
									double dp = coeffs[(z*minbuf[i].h+y)*minbuf[i].w+x]-pblock[(z*minbuf[i].h+y)*minbuf[i].w+x]/(normalization[i]*normalization[i]*scalefactor[i]);
									if(x < scaled[i].w-1) coeffs[(z*minbuf[i].h+y)*minbuf[i].w+x+1] += dp*7.0/16.0;
									if(y < scaled[i].h-1) {
										if(x) coeffs[(z*minbuf[i].h+y+1)*minbuf[i].w+x-1] += dp*3.0/16.0;
										coeffs[(z*minbuf[i].h+y+1)*minbuf[i].w+x] += dp*5.0/16.0;
										if(x < scaled[i].w-1) coeffs[(z*minbuf[i].h+y+1)*minbuf[i].w+x+1] += dp/16.0;
									}
								}
							}
						}
			}
		}
		for(int z = 0; z < scaled->d; z++) {
			for(int i = 0; i < components; i++) {
				if(bz >= nblocks[i].d || z >= scaled[i].d) continue;
				AVComponentDescriptor comp = pixdesc.comp[i];
				for(int by = 0; by < nblocks[i].h; by++)
					for(int bx = 0; bx < nblocks[i].w; bx++)
						for(int y = 0; y < scaled[i].h; y++)
							for(int x = 0; x < scaled[i].w; x++)
								ffapi_setpel_direct(writeframe,bx*scaled[i].w+x,by*scaled[i].h+y,comp,pixels[i][by*nblocks[i].w+bx][(z*minbuf[i].h+y)*minbuf[i].w+x]);
			}
			ffapi_write_frame(out,writeframe);
			fprintf(stderr,"\rread: %*llu wrote: %*llu",padb,(bz+1)*block->d,pads,bz*scaled->d+z+1);
		}
	}
	fprintf(stderr,"\n");

	if(quant) {
		unsigned long long total = 0;
		for(int i = 0; i < components; i++)
			total += newres[i].w * newres[i].h * newres[i].d;
		fprintf(stderr,"coeffs: %llu / %llu (%2.0f%%)\nzeroes: %llu / %llu (%2.0f%%)\n",coeffs_coded,total,coeffs_coded*100.0/total,total-coeffs_coded,total,(total-coeffs_coded)*100.0/total);
	}

	fftwf_free(coeffs);
	for(int i = 0; i < components; i++) {
		for(int b = 0; b < nblocks[i].h * nblocks[i].w; b++)
			free(pixels[i][b]);
		free(pixels[i]);
	}
	for(int i = 0; i < unique_plans; i++)
		fftwf_destroy_plan(plans[i]);
	fftwf_cleanup();

	ffapi_close(out);
	ffapi_close(in);

	return 0;
}
