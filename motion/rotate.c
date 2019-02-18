/*
 * rotate - rotate video by right angles on a 3-dimensional axis.
 * Copyright 2013-2017 0x09.net.
 */

/* 90ª ccw = +y-x+z
   90ª  cw = -y+x+z
  180ª     = -x-y+z
*/

#include "ffapi.h"
#include <unistd.h>
#include <getopt.h>

int main(int argc, char* argv[]) {
	AVRational fps = {0};
	bool samedur = false;
	size_t frames = 0, offset = 0;
	const char* iopt = NULL,* ifmt = NULL,* cprops = NULL;
	const char* oopt = NULL,* ofmt = NULL,* enc = NULL;
	int loglevel = AV_LOG_ERROR;
	int c;
	while((c = getopt(argc,argv,":o:O:f:F:c:e:l:r:s:")) > 0)
		switch(c) {
			case 'o': iopt = optarg; break; case 'O': oopt = optarg; break;
			case 'f': ifmt = optarg; break; case 'F': ofmt = optarg; break;
			case 'c': cprops = optarg; break; case 'e': enc  = optarg; break;
			case 'l': loglevel = strtol(optarg,NULL,10); break;
			case 's': sscanf(optarg,"%zu:%zu",&offset,&frames); break;
			case 'r': {
				if(!strcmp(optarg,"same"))
					samedur = true;
				else av_parse_video_rate(&fps,optarg);
			}; break;
		}
	argv += optind;
	argc -= optind;
	if(!argc) {
		fprintf(stderr,
"usage: rot <ffapi args> -r framerate -s start:frames [-]xyz in out\n"
"[-]xyz: new dimensional arrangement, with -/+ to indicate direction\n"
"ffapi args: -o/O   input/output dictionary options\n"
"            -f/F   input/output format\n"
"            -c     intermediate colorspace options\n"
"            -e     encoder\n"
"            -l     loglevel\n"
);
		return 0;
	}

	int map[3];
	bool invert[3] = {0};
	for(int i = 0; i < 3; i++) {
		if(argv[0][i] == '-') {
			invert[i] = 1;
			argv[0]++;
		}
		else if(argv[0][i] == '+')
			argv[0]++;
		map[i] = argv[0][i]-'x';
	}

	av_log_set_level(loglevel);
	AVRational r;
	unsigned long components = 0;
	unsigned long widths[4], heights[4], nframes;
	FFColorProperties color_props;
	ffapi_parse_color_props(&color_props, cprops);
	FFContext* in = ffapi_open_input(argv[1],iopt,ifmt,&color_props,&components,&widths,&heights,&nframes,&r,frames == 0);
	ffapi_seek_frame(in,offset,NULL);
	unsigned long len[] = {*widths, *heights, nframes - offset};

	if(frames && len[2])
		len[2] = FFMIN(frames,len[2]);
	else if(frames)
		len[2] = frames;

	if((in->pixdesc->flags & AV_PIX_FMT_FLAG_PLANAR) || in->pixdesc->log2_chroma_w || in->pixdesc->log2_chroma_h) {
		fprintf(stderr,"Unsupported planar or subsampled pixel format (%s), use -c pixel_format=rgb24/gray8/etc\n",in->pixdesc->name);
		return 1;
	}

	if(fps.num == 0 && fps.den == 0) {
		if(samedur)
			fps = (AVRational){len[map[2]]*r.num,len[2]*r.den};
		else fps = r;
	}

	FFContext* out = ffapi_open_output(argv[2],oopt,ofmt,enc,AV_CODEC_ID_FFV1,&color_props,len[map[0]],len[map[1]],fps);
	if(!out) { puts("out error"); return 1; }

	AVFrame* iframe = ffapi_alloc_frame(in);
	if(!iframe) { fprintf(stderr,"inframe error\n"); return 1; }
	AVFrame* oframe = ffapi_alloc_frame(out);
	if(!oframe) { fprintf(stderr,"outframe error\n"); return 1; }

	unsigned char (*buf)[in->pixdesc->nb_components] = malloc(len[0]*len[1]*len[2]*sizeof(*buf));
	for(int z = 0; z < len[2]; z++) {
		ffapi_read_frame(in,iframe);
		for(int y = 0; y < len[1]; y++)
			for(int x = 0; x < len[0]; x++)
				ffapi_getpixel(in,iframe,x,y,buf[(z*len[1]+y)*len[0]+x]);
		fprintf(stderr,"\r%d",z);
	}
	fprintf(stderr,"\n");
	ffapi_free_frame(iframe);
	ffapi_close(in);

	unsigned long axis[3];
#define INV(i) ((len[i]-axis[i]-1)*invert[map[i]]+axis[i]*!invert[map[i]])
	for(axis[map[2]] = 0; axis[map[2]] < len[map[2]]; axis[map[2]]++) {
		for(axis[map[1]] = 0; axis[map[1]] < len[map[1]]; axis[map[1]]++)
			for(axis[map[0]] = 0; axis[map[0]] < len[map[0]]; axis[map[0]]++)
				ffapi_setpixel(out,oframe,axis[map[0]],axis[map[1]],buf[(INV(2)*len[1]+INV(1))*len[0]+INV(0)]);
		ffapi_write_frame(out,oframe);
		fprintf(stderr,"\r%lu",axis[map[2]]);
	}

	fputc('\n',stderr);
	ffapi_free_frame(oframe);
	ffapi_close(out);
}
