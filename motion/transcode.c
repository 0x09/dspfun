/*
 * transcode - simple transcoder / ffapi test utility
 * Copyright 2017 0x09.net.
 */

#include "ffapi.h"
#include <unistd.h>
#include <getopt.h>

int main(int argc, char* argv[]) {
	AVRational fps = {0};
	size_t frames = 0, offset = 0;
	const char* iopt = NULL,* ifmt = NULL,* cprops = NULL;
	const char* oopt = NULL,* ofmt = NULL,* enc = NULL;
	int loglevel = AV_LOG_ERROR;
	int c;
	while((c = getopt(argc, argv, "o:O:f:F:c:e:l:r:s:")) > 0)
		switch(c) {
			case 'o': iopt = optarg; break; case 'O': oopt = optarg; break;
			case 'f': ifmt = optarg; break; case 'F': ofmt = optarg; break;
			case 'c': cprops = optarg; break; case 'e': enc  = optarg; break;
			case 'l': loglevel = strtol(optarg, NULL, 10); break;
			case 'r': av_parse_video_rate(&fps, optarg); break;
			case 's': sscanf(optarg, "%zu:%zu", &offset, &frames); break;
		}
	argv += optind;
	argc -= optind;
	if(!argc) {
		fprintf(stderr, "usage: transcode -fF <in/out format> -oO <in/out options> -c <intermediate colorspace options> -e <encoder> -l <loglevel> -r <rate> -s <start>:<frames> input output\n");
		return 0;
	}

	av_log_set_level(loglevel);

	unsigned long components = 0;
	unsigned long widths[4], heights[4], nframes;
	FFColorProperties color_props;
	ffapi_parse_color_props(&color_props, cprops);
	FFContext* in = ffapi_open_input(argv[0], iopt, ifmt, &color_props, &components, &widths, &heights, &nframes, (fps.den == 0 ? &fps : NULL), frames == 0);
	if(!in) { fprintf(stderr, "Error opening input context\n"); return 1; }

	FFContext* out = ffapi_open_output(argv[1], oopt, ofmt, enc, AV_CODEC_ID_FFV1, &color_props, *widths, *heights, fps);
	if(!out) { fprintf(stderr, "Error opening output context\n"); return 1; }

	AVFrame* iframe = ffapi_alloc_frame(in);
	if(!iframe) { fprintf(stderr, "Couldn't allocate input frame\n"); return 1; }

	AVFrame* oframe = ffapi_alloc_frame(out);
	if(!oframe) { fprintf(stderr, "Couldn't allocate output frame\n"); return 1; }

	if(frames)
		nframes = frames;
	else nframes -= FFMIN(nframes, offset);
	ffapi_seek_frame(in, offset, NULL);

	for(int z = 0; z < nframes && !ffapi_read_frame(in, iframe); z++) {
		// equivalent to ffapi_write_frame(out, iframe) since no image processing is being done
		for(int c = 0; c < components; c++)
			for(int y = 0; y < heights[c]; y++)
				for(int x = 0; x < widths[c]; x++)
					ffapi_setpel(out, oframe, x, y, c, ffapi_getpel(in, iframe, x, y, c));

		ffapi_write_frame(out, oframe);
		fprintf(stderr, "\r%d", z);
	}
	fprintf(stderr, "\n");
	ffapi_free_frame(iframe);
	ffapi_close(in);
	ffapi_free_frame(oframe);
	ffapi_close(out);
}
