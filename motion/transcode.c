/*
 * transcode - simple transcoder / ffapi test utility
 */

#include "ffapi.h"
#include <unistd.h>
#include <getopt.h>

int main(int argc, char* argv[]) {
	AVRational fps = {0};
	uint64_t frames = 0, offset = 0;
	const char* iopt = NULL,* ifmt = NULL,* cprops = NULL;
	const char* oopt = NULL,* ofmt = NULL,* enc = NULL;
	int loglevel = AV_LOG_ERROR;
	bool quiet = false;
	int c;
	while((c = getopt(argc, argv, "o:O:f:F:c:e:l:r:s:q")) > 0)
		switch(c) {
			case 'o': iopt = optarg; break; case 'O': oopt = optarg; break;
			case 'f': ifmt = optarg; break; case 'F': ofmt = optarg; break;
			case 'c': cprops = optarg; break; case 'e': enc  = optarg; break;
			case 'l': loglevel = strtol(optarg, NULL, 10); break;
			case 'r': av_parse_video_rate(&fps, optarg); break;
			case 's': sscanf(optarg, "%" SCNu64 ":" "%" SCNu64, &offset, &frames); break;
			case 'q': quiet = true; break;
			default: return 1;
		}
	argv += optind;
	argc -= optind;
	if(!argc) {
		fprintf(stderr, "usage: transcode -fF <in/out format> -oO <in/out options> -c <intermediate colorspace options> -e <encoder> -l <loglevel> -r <rate> -s <start>:<frames> -q input output\n");
		return 0;
	}

	av_log_set_level(loglevel);

	int err = 0, ret = 0;
	unsigned long components = 0;
	unsigned long widths[4], heights[4];
	uint64_t nframes;
	FFColorProperties color_props;
	ffapi_parse_color_props(&color_props, cprops);
	FFContext* in = ffapi_open_input(argv[0], iopt, ifmt, &color_props, ffapi_pixfmts_8bit_pel, &components, &widths, &heights, &nframes, (fps.den == 0 ? &fps : NULL), frames == 0);
	if(!in) {
		fprintf(stderr, "Error opening input context\n");
		ret = 1;
		goto end;
	}

	FFContext* out = ffapi_open_output(argv[1], oopt, ofmt, enc, AV_CODEC_ID_FFV1, &color_props, *widths, *heights, fps);
	if(!out) {
		fprintf(stderr, "Error opening output context\n");
		ret = 1;
		goto end;
	 }

	AVFrame* iframe = ffapi_alloc_frame(in);
	if(!iframe) {
		fprintf(stderr, "Couldn't allocate input frame\n");
		ret = 1;
		goto end;
	}

	AVFrame* oframe = ffapi_alloc_frame(out);
	if(!oframe) {
		fprintf(stderr, "Couldn't allocate output frame\n");
		ret = 1;
		goto end;
	}

	if(frames)
		nframes = frames;
	else nframes -= FFMIN(nframes, offset);

	if((err = ffapi_seek_frame(in, &offset, NULL))) {
		fprintf(stderr,"Error seeking: %s\n",av_err2str(err));
		ret = 1;
		goto end;
	}

	for(int z = 0; z < nframes && !(err = ffapi_read_frame(in, iframe)); z++) {
		// equivalent to ffapi_write_frame(out, iframe) since no image processing is being done
		for(int c = 0; c < components; c++)
			for(int y = 0; y < heights[c]; y++)
				for(int x = 0; x < widths[c]; x++)
					ffapi_setpel(out, oframe, x, y, c, ffapi_getpel(in, iframe, x, y, c));

		if((err = ffapi_write_frame(out, oframe))) {
			fprintf(stderr,"\nError writing frame: %s\n",av_err2str(err));
			ret = 1;
			goto end;
		}
		if(!quiet)
			fprintf(stderr, "\r%d", z);
	}
	if(!quiet)
		fprintf(stderr, "\n");

	if(err) {
		fprintf(stderr,"Error reading frame: %s\n",av_err2str(err));
		ret = 1;
	}

end:
	ffapi_free_frame(iframe);
	ffapi_close(in);
	ffapi_free_frame(oframe);
	ffapi_close(out);
	return ret;
}
