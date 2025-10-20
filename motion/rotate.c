/*
 * rotate - rotate video by right angles on a 3-dimensional axis.
 */

/* 90ª ccw = +y-x+z
   90ª  cw = -y+x+z
  180ª     = -x-y+z
*/

#include "ffapi.h"
#include <unistd.h>
#include <getopt.h>

static bool pix_fmt_filter(const AVPixFmtDescriptor* desc) {
	return ffapi_pixfmts_8bit_pel(desc) && !(desc->log2_chroma_w || desc->log2_chroma_h);
}

void usage() {
	fprintf(stderr,"Usage: rotate [options] [-]xyz <infile> <outfile>\n");
	exit(1);
}
void help() {
	puts(
		"Usage: rotate [options] [-]xyz <infile> <outfile>\n"
		"\n"
		"  [-]xyz  how to rearrange the input dimensions, with -/+ to indicate direction.\n"
		"	       e.g. \"zyx\" swaps the x and z axis while \"x-yz\" results in a vertical flip\n"
		"\n"
		"  -h                  this help text\n"
		"  -s <start:nframes>  starting frame number and total number of frames of input to use\n"
		"  -r <rational>       output framerate or \"same\" to match input duration [default: input rate]\n"
		"  -q                  don't print progress\n"
		"\n"
		"  -o  input av options string\n"
		"  -O  output av options string\n"
		"  -f  input format\n"
		"  -F  output format\n"
		"  -c  intermediate colorspace options\n"
		"  -e  encoder\n"
		"  -l  loglevel\n"
	);
	exit(0);
}

int main(int argc, char* argv[]) {
	AVRational fps = {0};
	bool samedur = false, quiet = false;
	uint64_t frames = 0, offset = 0;
	const char* iopt = NULL,* ifmt = NULL,* cprops = NULL;
	const char* oopt = NULL,* ofmt = NULL,* enc = NULL;
	int loglevel = AV_LOG_ERROR;
	int c;
	while((c = getopt(argc,argv,":o:O:f:F:c:e:l:r:s:hq")) > 0)
		switch(c) {
			case 'o': iopt = optarg; break; case 'O': oopt = optarg; break;
			case 'f': ifmt = optarg; break; case 'F': ofmt = optarg; break;
			case 'c': cprops = optarg; break; case 'e': enc  = optarg; break;
			case 'l': loglevel = strtol(optarg,NULL,10); break;
			case 's': sscanf(optarg, "%" SCNu64 ":" "%" SCNu64, &offset, &frames); break;
			case 'r': {
				if(!strcmp(optarg,"same"))
					samedur = true;
				else av_parse_video_rate(&fps,optarg);
			}; break;
			case 'q': quiet = true; break;
			case 'h': help();
		}
	argv += optind;
	argc -= optind;
	if(argc < 3)
		usage();

	int map[3] = {-1,-1,-1};
	bool invert[3] = {0};
	for(int i = 0; i < 3; i++) {
		if(argv[0][i] == '-') {
			invert[i] = 1;
			argv[0]++;
		}
		else if(argv[0][i] == '+')
			argv[0]++;
		if(!argv[0][i])
			break;
		map[i] = argv[0][i]-'x';
	}
	for(int i = 0; i < 3; i++)
		if(map[i] < 0 || map[i] > 2)
			usage();

	av_log_set_level(loglevel);

	int err = 0, ret = 0;
	AVRational r;
	unsigned long components = 0;
	unsigned long widths[4], heights[4];
	uint64_t nframes;
	FFColorProperties color_props;
	ffapi_parse_color_props(&color_props, cprops);

	FFContext* in = ffapi_open_input(argv[1],iopt,ifmt,&color_props,pix_fmt_filter,&components,&widths,&heights,&nframes,&r,frames == 0);
	if(!in) {
		fprintf(stderr,"error opening input file %s\n",argv[1]);
		return 1;
	}

	if((err = ffapi_seek_frame(in,&offset,NULL))) {
		fprintf(stderr,"Error seeking: %s\n",av_err2str(err));
		ffapi_close(in);
		return 1;
	}

	if(nframes)
		nframes -= offset;
	unsigned long len[] = {*widths, *heights, nframes};

	if(frames && len[2])
		len[2] = FFMIN(frames,len[2]);
	else if(frames)
		len[2] = frames;

	if(fps.num == 0 && fps.den == 0) {
		if(samedur)
			fps = (AVRational){len[map[2]]*r.num,len[2]*r.den};
		else fps = r;
	}

	FFContext* out = ffapi_open_output(argv[2],oopt,ofmt,enc,AV_CODEC_ID_FFV1,&color_props,len[map[0]],len[map[1]],fps);
	if(!out) {
		fprintf(stderr,"error opening output file %s\n",argv[2]);
		return 1;
	}

	AVFrame* iframe = ffapi_alloc_frame(in);
	if(!iframe) { fprintf(stderr,"inframe error\n"); return 1; }
	AVFrame* oframe = ffapi_alloc_frame(out);
	if(!oframe) { fprintf(stderr,"outframe error\n"); return 1; }

	unsigned char (*buf)[in->pixdesc->nb_components] = malloc(len[0]*len[1]*len[2]*sizeof(*buf));
	for(int z = 0; z < len[2]; z++) {
		if((err = ffapi_read_frame(in,iframe))) {
			fprintf(stderr,"Error reading frame: %s\n",av_err2str(err));
			ffapi_free_frame(iframe);
			ffapi_close(in);
			ret = 1;
			goto end;
		}
		for(int y = 0; y < len[1]; y++)
			for(int x = 0; x < len[0]; x++)
				ffapi_getpixel(in,iframe,x,y,buf[(z*len[1]+y)*len[0]+x]);
		if(!quiet)
			fprintf(stderr,"\r%d",z);
	}
	if(!quiet)
		fprintf(stderr,"\n");
	ffapi_free_frame(iframe);
	ffapi_close(in);

	unsigned long axis[3];
#define INV(i) ((len[i]-axis[i]-1)*invert[map[i]]+axis[i]*!invert[map[i]])
	for(axis[map[2]] = 0; axis[map[2]] < len[map[2]]; axis[map[2]]++) {
		for(axis[map[1]] = 0; axis[map[1]] < len[map[1]]; axis[map[1]]++)
			for(axis[map[0]] = 0; axis[map[0]] < len[map[0]]; axis[map[0]]++)
				ffapi_setpixel(out,oframe,axis[map[0]],axis[map[1]],buf[(INV(2)*len[1]+INV(1))*len[0]+INV(0)]);
		if((err = ffapi_write_frame(out,oframe))) {
			fprintf(stderr,"Error writing frame: %s\n",av_err2str(err));
			ret = 1;
			goto end;
		}
		if(!quiet)
			fprintf(stderr,"\r%lu",axis[map[2]]);
	}

	if(!quiet)
		fputc('\n',stderr);
end:
	ffapi_free_frame(oframe);
	ffapi_close(out);
	return ret;
}
