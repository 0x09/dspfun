/*
 * ffapi - Wrapper for pulling/pushing frames with libav*
 * Copyright 2013-2016 0x09.net.
 */

#include "ffapi.h"

#include <libavutil/parseutils.h>
#include <libavutil/opt.h>
#include <libavutil/avstring.h>

#include <sys/stat.h>

#define UNSPECIFIED_COLOR_PROPERTIES \
	.pix_fmt = AV_PIX_FMT_NONE,\
	.color_range = AVCOL_RANGE_UNSPECIFIED,\
	.color_primaries = AVCOL_PRI_UNSPECIFIED,\
	.color_trc = AVCOL_TRC_UNSPECIFIED,\
	.color_space = AVCOL_SPC_UNSPECIFIED,\
	.chroma_location = AVCHROMA_LOC_UNSPECIFIED

const static FFColorProperties ffapi_default_color_properties = {
	UNSPECIFIED_COLOR_PROPERTIES
};

struct FFColorDefaults {
	const char* format;
	FFColorProperties props;
};

// ffmpeg doesn't provide structured info about what color properties are assumed by different formats/codecs,
// so we provide some defaults based on the actual decoder behavior when the container doesn't store this info
// anything set in the options string still takes precedence
const static struct FFColorDefaults ffapi_format_color_defaults[] = {
	{
		.format = "yuv4mpegpipe",
		.props = {
			UNSPECIFIED_COLOR_PROPERTIES,
			.color_range = AVCOL_RANGE_MPEG,
			.color_primaries = AVCOL_PRI_SMPTE170M,
			.color_trc = AVCOL_TRC_SMPTE170M,
			.color_space = AVCOL_SPC_SMPTE170M,
		}
	},{
		.format = "avi",
		.props = {
			UNSPECIFIED_COLOR_PROPERTIES,
			.color_range = AVCOL_RANGE_MPEG,
		}
	},{
		// default to sRGB
		.format = "image2",
		.props = {
			UNSPECIFIED_COLOR_PROPERTIES,
			.color_range = AVCOL_RANGE_JPEG,
			.color_primaries = AVCOL_PRI_BT709,
			.color_trc = AVCOL_TRC_IEC61966_2_1,
			.color_space = AVCOL_SPC_RGB,
		}
	},{0}
};

static void fill_color_defaults(AVOutputFormat* fmt, AVCodecContext* avc) {
	const struct FFColorDefaults* defaults;
	for(defaults = ffapi_format_color_defaults; defaults->format && strcmp(defaults->format,fmt->name); defaults++)
		;
	if(defaults->format) {
		// != isn't a typo, we want to set this only if the input color property is known
		if(defaults->props.color_range != AVCOL_RANGE_UNSPECIFIED && avc->color_range != AVCOL_RANGE_UNSPECIFIED)
			avc->color_range = defaults->props.color_range;
		if(defaults->props.color_space != AVCOL_SPC_UNSPECIFIED && avc->colorspace != AVCOL_SPC_UNSPECIFIED)
			avc->colorspace = defaults->props.color_space;
		if(defaults->props.color_primaries != AVCOL_PRI_UNSPECIFIED && avc->color_primaries != AVCOL_PRI_UNSPECIFIED)
			avc->color_primaries = defaults->props.color_primaries;
		if(defaults->props.color_trc != AVCOL_TRC_UNSPECIFIED && avc->color_trc != AVCOL_TRC_UNSPECIFIED)
			avc->color_trc = defaults->props.color_trc;
		if(defaults->props.chroma_location != AVCHROMA_LOC_UNSPECIFIED && avc->chroma_sample_location != AVCHROMA_LOC_UNSPECIFIED)
			avc->chroma_sample_location = defaults->props.chroma_location;
	}
	// y4m input format infers different chroma locations based on the colorspace header, but stock C420 is taken as center
	if(!strcmp(fmt->name,"yuv4mpegpipe")) {
		if(av_pix_fmt_desc_get(avc->pix_fmt)->flags & AV_PIX_FMT_FLAG_RGB)
			avc->pix_fmt = AV_PIX_FMT_YUV444P;
		else if(avc->pix_fmt == AV_PIX_FMT_YUV420P &&
			avc->chroma_sample_location != AVCHROMA_LOC_UNSPECIFIED &&
			avc->chroma_sample_location != AVCHROMA_LOC_TOPLEFT && //C420paldv
			avc->chroma_sample_location != AVCHROMA_LOC_LEFT) // C420mpeg2
			avc->chroma_sample_location = AVCHROMA_LOC_CENTER;
	}
	// if the input colorspace is RGB but the output pixel format isn't, default to BT601
	if(avc->colorspace == AVCOL_SPC_RGB && !(av_pix_fmt_desc_get(avc->pix_fmt)->flags & AV_PIX_FMT_FLAG_RGB))
		avc->colorspace = AVCOL_SPC_SMPTE170M;
}

void ffapi_parse_color_props(FFColorProperties* c, const char* props) {
	*c = ffapi_default_color_properties;
	if(!props || !*props)
		return;
	AVDictionary* d = NULL;
	av_dict_parse_string(&d, props, "=", ":", 0);
	AVDictionaryEntry* e;
	if((e = av_dict_get(d,"pixel_format",NULL,0)))
		c->pix_fmt = av_get_pix_fmt(e->value);
	if((e = av_dict_get(d,"color_range",NULL,0)))
		c->color_range = av_color_range_from_name(e->value);
	if((e = av_dict_get(d,"color_primaries",NULL,0)))
		c->color_primaries = av_color_primaries_from_name(e->value);
	if((e = av_dict_get(d,"color_trc",NULL,0)))
		c->color_trc = av_color_transfer_from_name(e->value);
	if((e = av_dict_get(d,"colorspace",NULL,0)))
		c->color_space = av_color_space_from_name(e->value);
	if((e = av_dict_get(d,"chroma_sample_location",NULL,0)))
		c->chroma_location = av_chroma_location_from_name(e->value);
	av_dict_free(&d);
}

#define validate_color_prop(c,ret,prop,strprop,getter) do {\
	if(!getter(c->prop)) {\
		av_log(NULL,AV_LOG_ERROR,"ffapi: Invalid " strprop "\n");\
		ret = false;\
	}\
} while(0)

bool ffapi_validate_color_props(const FFColorProperties* c) {
	bool ret = true;
	if(c->pix_fmt != AV_PIX_FMT_NONE)
		validate_color_prop(c,ret,pix_fmt,"pixel_format",av_pix_fmt_desc_get);
	validate_color_prop(c,ret,color_range,"color_range",av_color_range_name);
	validate_color_prop(c,ret,color_primaries,"color_primaries",av_color_primaries_name);
	validate_color_prop(c,ret,color_trc,"color_trc",av_color_transfer_name);
	validate_color_prop(c,ret,color_space,"colorspace",av_color_space_name);
	validate_color_prop(c,ret,chroma_location,"chroma_sample_location",av_chroma_location_name);
	return ret;
}

FFContext* ffapi_open_input(const char* file, const char* options,
                         const char* format, FFColorProperties* color_props,
                         unsigned long* components, unsigned long (*widths)[4], unsigned long (*heights)[4], unsigned long* frames, AVRational* rate, bool calc_frames) {

	if(!ffapi_validate_color_props(color_props))
		return NULL;

	FFContext* in = calloc(1,sizeof(*in));

	AVCodecContext* avc = NULL;
	AVDictionary* opts = NULL;
	av_dict_parse_string(&opts,options,"=",":",0);

	if(!strcmp(file,"-"))
		file = "pipe:";
	struct stat st;
	if(!format && (!strncmp(file,"pipe:",5) || (!stat(file,&st) && S_ISFIFO(st.st_mode))))
		format = "yuv4mpegpipe";

	AVInputFormat* ifmt = NULL;
	if(format) ifmt = av_find_input_format(format);
	if(avformat_open_input(&in->fmt,file,ifmt,&opts))
		goto error;

	avformat_find_stream_info(in->fmt,NULL);
	AVCodec* dec;
	int stream = av_find_best_stream(in->fmt,AVMEDIA_TYPE_VIDEO,-1,-1,&dec,0);
	if(stream < 0)
		goto error;
	in->st = in->fmt->streams[stream];
	AVCodecParameters* params = in->st->codecpar;

	avc = in->codec = avcodec_alloc_context3(dec);
	avcodec_parameters_to_context(avc, params);
	if(avcodec_open2(avc,dec,&opts) < 0)
		goto error;

	av_dict_free(&opts);
	opts = NULL;

	if(rate)
		*rate = in->st->r_frame_rate;

	in->pixdesc = (AVPixFmtDescriptor*)av_pix_fmt_desc_get(avc->pix_fmt);
	if(!in->pixdesc) {
		ffapi_close(in);
		return NULL;
	}
	if(frames) {
		*frames = in->st->nb_frames;
		if(!*frames) {
			if(!strcmp(in->fmt->iformat->name,"image2") || !strcmp(in->fmt->iformat->name,"png_pipe"))
				*frames = 1;
			else if(calc_frames) {
				*frames = ffapi_seek_frame(in,SIZE_MAX,NULL);
				ffapi_close(in);
				return ffapi_open_input(file,options,format,color_props,components,widths,heights,NULL,rate,false);
			}
		}
	}

	FFColorProperties color_props_copy = ffapi_default_color_properties;
	if(!color_props)
		color_props = &color_props_copy;
	if(color_props->color_range == AVCOL_RANGE_UNSPECIFIED)
		color_props->color_range = avc->color_range;
	if(color_props->color_primaries == AVCOL_PRI_UNSPECIFIED)
		color_props->color_primaries = avc->color_primaries;
	if(color_props->color_trc == AVCOL_TRC_UNSPECIFIED)
		color_props->color_trc = avc->color_trc;
	if(color_props->color_space == AVCOL_SPC_UNSPECIFIED)
		color_props->color_space = avc->colorspace;
	if(color_props->chroma_location == AVCHROMA_LOC_UNSPECIFIED)
		color_props->chroma_location = avc->chroma_sample_location;
	if(color_props->pix_fmt == AV_PIX_FMT_NONE)
		color_props->pix_fmt = avc->pix_fmt;

	if(
		color_props->pix_fmt != avc->pix_fmt ||
		color_props->color_range != avc->color_range ||
		color_props->color_primaries != avc->color_primaries ||
		color_props->color_trc != avc->color_trc ||
		color_props->color_space != avc->colorspace ||
		color_props->chroma_location != avc->chroma_sample_location
	) {
		in->sws = sws_alloc_context();
		av_opt_set_int(in->sws, "srcw", avc->width, 0);
		av_opt_set_int(in->sws, "srch", avc->height, 0);
		av_opt_set_int(in->sws, "src_format", avc->pix_fmt, 0);
		av_opt_set_int(in->sws, "dstw", avc->width, 0);
		av_opt_set_int(in->sws, "dsth", avc->height, 0);
		av_opt_set_int(in->sws, "dst_format", color_props->pix_fmt, 0);
		av_opt_set_int(in->sws, "sws_flags", SWS_BICUBIC, 0);
		int xpos, ypos;
		if(!avcodec_enum_to_chroma_pos(&xpos, &ypos, avc->chroma_sample_location)) {
			av_opt_set_int(in->sws,"src_h_chr_pos",xpos,0);
			av_opt_set_int(in->sws,"src_v_chr_pos",ypos,0);
		}
		if(!avcodec_enum_to_chroma_pos(&xpos, &ypos, color_props->chroma_location)) {
			av_opt_set_int(in->sws,"dst_h_chr_pos",xpos,0);
			av_opt_set_int(in->sws,"dst_v_chr_pos",ypos,0);
		}
		if(sws_init_context(in->sws,NULL,NULL)) {
			ffapi_close(in);
			return NULL;
		}

		int brightness, contrast, saturation;
		sws_getColorspaceDetails(in->sws, &(int*){0}, &(int){0}, &(int*){0}, &(int){0}, &brightness, &contrast, &saturation);
		sws_setColorspaceDetails(in->sws,
			sws_getCoefficients(avc->colorspace), avc->color_range == AVCOL_RANGE_JPEG,
			sws_getCoefficients(color_props->color_space), color_props->color_range == AVCOL_RANGE_JPEG,
			brightness, contrast, saturation);

		in->swsframe = av_frame_alloc();
		in->pixdesc = (AVPixFmtDescriptor*)av_pix_fmt_desc_get(color_props->pix_fmt);
	}

	if(components)
		*components = in->pixdesc->nb_components;
	for(int i = 0; i < in->pixdesc->nb_components; i++) {
		if(widths)
			(*widths)[i] = avc->width;
		if(heights)
			(*heights)[i] = avc->height;
	}
	for(int i = 1; i < FFMIN(in->pixdesc->nb_components,3); i++) {
		if(widths)
			(*widths)[i] = -(-(int)(*widths)[i] >> in->pixdesc->log2_chroma_w);
		if(heights)
			(*heights)[i]= -(-(int)(*heights)[i] >> in->pixdesc->log2_chroma_h);
	}
	return in;

error:
	avcodec_free_context(&avc);
	avformat_close_input(&in->fmt);
	av_dict_free(&opts);
	free(in);
	return NULL;
}

static void ffapi_io_close_pipe(AVFormatContext *s, AVIOContext* pb) {
    avio_close(pb);
	if(s->opaque)
		pclose((FILE*)s->opaque);
}

FFContext* ffapi_open_output(const char* file, const char* options,
                         const char* format, const char* encoder, enum AVCodecID preferred_encoder,
                         const FFColorProperties* in_color_props,
                         unsigned long width, unsigned long height, AVRational rate) {

	if(!ffapi_validate_color_props(in_color_props))
		return NULL;

	AVDictionary* opts = NULL;
	FFContext* out = calloc(1,sizeof(*out));

	if(!strcmp(file,"-"))
		file = "pipe:";
	struct stat st;
	if(!format) {
		if(!strcmp(file,"ffplay:"))
			format = "rawvideo";
		else if(!strncmp(file,"pipe:",5) || (!stat(file,&st) && S_ISFIFO(st.st_mode)))
			format = "yuv4mpegpipe";
	}

	if(avformat_alloc_output_context2(&out->fmt,NULL,format,file))
		goto error;

	AVCodec* enc = NULL;
	if(encoder)
		enc = avcodec_find_encoder_by_name(encoder);
	if(!enc || avformat_query_codec(out->fmt->oformat,enc->id,FF_COMPLIANCE_NORMAL) != 1)
		enc = avcodec_find_encoder(preferred_encoder);
	if(!enc || avformat_query_codec(out->fmt->oformat,enc->id,FF_COMPLIANCE_NORMAL) != 1)
		enc = avcodec_find_encoder(av_guess_codec(out->fmt->oformat,NULL,out->fmt->url,NULL,AVMEDIA_TYPE_VIDEO));
	if(!enc)
		goto error;

	out->st = avformat_new_stream(out->fmt,enc);
	AVCodecContext* avc = out->codec = avcodec_alloc_context3(enc);
	out->pixdesc = (AVPixFmtDescriptor*)av_pix_fmt_desc_get(in_color_props->pix_fmt);

	avc->pix_fmt = enc->pix_fmts ? avcodec_find_best_pix_fmt_of_list(enc->pix_fmts,in_color_props->pix_fmt,0,NULL) : in_color_props->pix_fmt;
	avc->width  = width;
	avc->height = height;
	avc->time_base = out->st->time_base = av_inv_q(out->st->r_frame_rate = out->st->avg_frame_rate = rate);
	avc->color_range = in_color_props->color_range;
	avc->color_primaries = in_color_props->color_primaries;
	avc->color_trc = in_color_props->color_trc;
	avc->colorspace = in_color_props->color_space;
	avc->chroma_sample_location = in_color_props->chroma_location;
	fill_color_defaults(out->fmt->oformat, avc);

	avcodec_parameters_from_context(out->st->codecpar,avc);

	av_dict_parse_string(&opts,options,"=",":",0);
	if(avcodec_open2(avc,enc,&opts))
		goto error;
	avcodec_parameters_from_context(out->st->codecpar,avc);

	if(!strcmp(file,"ffplay:")) {
		char* cmd;
		asprintf(&cmd,
			"ffplay -loglevel quiet -f %s -vcodec %s -video_size %dx%d -framerate %d/%d -pixel_format %s -color_range %s -color_primaries %s -color_trc %s -colorspace %s -chroma_sample_location %s -",
			out->fmt->oformat->name, enc->name, avc->width, avc->height, rate.num, rate.den,
			av_pix_fmt_desc_get(avc->pix_fmt)->name,
			av_color_range_name(avc->color_range),
			av_color_primaries_name(avc->color_primaries),
			av_color_transfer_name(avc->color_trc),
			(avc->colorspace == AVCOL_SPC_RGB ? "rgb" : av_color_space_name(avc->colorspace)),
			av_chroma_location_name(avc->chroma_sample_location)
		);

		av_log(NULL,AV_LOG_INFO,"ffapi: Invoking %s\n",cmd);
		if(!(out->fmt->opaque = popen(cmd,"w"))) {
			free(cmd);
			goto error;
		}
		free(cmd);
		out->fmt->io_close = ffapi_io_close_pipe;
		int fd = fileno((FILE*)out->fmt->opaque);
		av_free(out->fmt->url);
		out->fmt->url = av_asprintf("pipe:%d",fd);
	}

	if(avio_open2(&out->fmt->pb,out->fmt->url,AVIO_FLAG_WRITE,NULL,&opts))
		goto error;
	if(avformat_write_header(out->fmt,&opts))
		goto error;
	av_dict_free(&opts);
	opts = NULL;
	av_dump_format(out->fmt,0,out->fmt->url,1);

	if(
		in_color_props->pix_fmt != avc->pix_fmt ||
		in_color_props->color_range != avc->color_range ||
		in_color_props->color_primaries != avc->color_primaries ||
		in_color_props->color_trc != avc->color_trc ||
		in_color_props->color_space != avc->colorspace ||
		in_color_props->chroma_location != avc->chroma_sample_location
	) {
		out->sws = sws_alloc_context();
		av_opt_set_int(out->sws, "srcw", width, 0);
		av_opt_set_int(out->sws, "srch", height, 0);
		av_opt_set_int(out->sws, "src_format", in_color_props->pix_fmt, 0);
		av_opt_set_int(out->sws, "dstw", avc->width, 0);
		av_opt_set_int(out->sws, "dsth", avc->height, 0);
		av_opt_set_int(out->sws, "dst_format", avc->pix_fmt, 0);
		av_opt_set_int(out->sws, "sws_flags", SWS_BICUBIC, 0);
		int xpos, ypos;
		if(!avcodec_enum_to_chroma_pos(&xpos, &ypos, in_color_props->chroma_location)) {
			av_opt_set_int(out->sws,"src_h_chr_pos",xpos,0);
			av_opt_set_int(out->sws,"src_v_chr_pos",ypos,0);
		}
		if(!avcodec_enum_to_chroma_pos(&xpos, &ypos, avc->chroma_sample_location)) {
			av_opt_set_int(out->sws,"dst_h_chr_pos",xpos,0);
			av_opt_set_int(out->sws,"dst_v_chr_pos",ypos,0);
		}

		if(sws_init_context(out->sws,NULL,NULL)) {
			sws_freeContext(out->sws);
			goto error;
		}

		int brightness, contrast, saturation;
		sws_getColorspaceDetails(out->sws, &(int*){0}, &(int){0}, &(int*){0}, &(int){0}, &brightness, &contrast, &saturation);
		sws_setColorspaceDetails(out->sws,
			sws_getCoefficients(in_color_props->color_space), in_color_props->color_range == AVCOL_RANGE_JPEG,
			sws_getCoefficients(avc->colorspace), avc->color_range == AVCOL_RANGE_JPEG,
			brightness, contrast, saturation);

		out->swsframe = av_frame_alloc();
		out->swsframe->width  = avc->width;
		out->swsframe->height = avc->height;
		out->swsframe->format = avc->pix_fmt;
		av_frame_get_buffer(out->swsframe,1);
	}


	 return out;
error:
	av_dict_free(&opts);
	avcodec_close(out->codec);
	avformat_free_context(out->fmt);
	free(out);
	return NULL;
}

AVFrame* ffapi_alloc_frame(FFContext* ctx) {
	AVFrame* frame = av_frame_alloc();
	if(frame && (ctx->fmt->oformat || ctx->sws)) {
		frame->width  = ctx->st->codecpar->width;
		frame->height = ctx->st->codecpar->height;
		frame->format = av_pix_fmt_desc_get_id(ctx->pixdesc);
		if(av_frame_get_buffer(frame,1))
			return NULL;
	}
	return frame;
}

size_t ffapi_seek_frame(FFContext* ctx, size_t offset, void (*progress)(size_t)) {
	if(!offset)
		return 0;

	AVFrame* frame = av_frame_alloc();
	size_t seek;
	FFContext ctx_copy = (FFContext){ .fmt = ctx->fmt, .codec = ctx->codec, .st = ctx->st };

	// just unswitch this manually
	if(progress)
		for(seek = 0; seek < offset && !ffapi_read_frame(&ctx_copy, frame); seek++)
			progress(seek);
	else for(seek = 0; seek < offset && !ffapi_read_frame(&ctx_copy, frame); seek++);

	av_frame_free(&frame);
	return seek;
}

void ffapi_clear_frame(AVFrame* frame) {
	av_frame_make_writable(frame);
	for(int i = 0; i < AV_NUM_DATA_POINTERS && frame->buf[i]; i++)
		memset(frame->buf[i]->data,0,frame->buf[i]->size);
}

void ffapi_free_frame(AVFrame* frame) {
	av_frame_free(&frame);
}

int ffapi_read_frame(FFContext* in, AVFrame* frame) {
	AVFrame* readframe = in->sws ? in->swsframe : frame;
	int err;
	while((err = avcodec_receive_frame(in->codec, readframe)) == AVERROR(EAGAIN)) {
		AVPacket packet = {0};
		while(!(err = av_read_frame(in->fmt,&packet)) && packet.stream_index != in->st->index)
			av_packet_unref(&packet);
		if(err) {
			packet.data = NULL;
			packet.size = 0;
		}
		avcodec_send_packet(in->codec, &packet);
		av_packet_unref(&packet);
	}
	if(!err) {
		if(in->sws)
			sws_scale(in->sws,(const uint8_t* const*)in->swsframe->data,in->swsframe->linesize,0,in->st->codecpar->height,frame->data,frame->linesize);
		frame->pts = readframe->best_effort_timestamp;
	}
	return err;
}

static int flush_frame(FFContext* out, AVFrame* frame) {
	AVCodecContext* codec = out->codec;
	AVPacket packet = {0};
	av_init_packet(&packet);
	int err = 0;
	while(!err && !(err = avcodec_receive_packet(codec,&packet))) {
		if(packet.pts != AV_NOPTS_VALUE)
			packet.pts = av_rescale_q(packet.pts,codec->time_base,out->st->time_base);
		else if(frame) packet.pts = frame->best_effort_timestamp;
		if(packet.dts != AV_NOPTS_VALUE)
			packet.dts = av_rescale_q(packet.dts,codec->time_base,out->st->time_base);
		packet.stream_index = out->st->index;
		err = av_write_frame(out->fmt,&packet);
		av_packet_unref(&packet);
	}
	if(err == AVERROR(EOF))
		err = 0;
	return err;
}

int ffapi_write_frame(FFContext* out, AVFrame* frame) {
	AVFrame* writeframe = out->swsframe;
	if(out->sws) sws_scale(out->sws,(const uint8_t* const*)frame->data,frame->linesize,0,out->st->codecpar->height,out->swsframe->data,out->swsframe->linesize);
	else writeframe = frame;
	AVCodecContext* codec = out->codec;
	writeframe->pts = codec->frame_number; //for now timebase == 1/rate
	avcodec_send_frame(codec,writeframe);
	return flush_frame(out,writeframe);
}

static int write_end(FFContext* out) {
	AVCodecContext* codec = out->codec;
	AVPacket packet = {0};
	av_init_packet(&packet);
	avcodec_send_frame(codec,NULL);
	int err = flush_frame(out,NULL);
	while(!err)
		err = av_write_frame(out->fmt,NULL);
	return FFMIN(err,0);
}

int ffapi_close(FFContext* ctx) {
	int ret = 0;
	if(ctx->fmt->oformat) {
		ret = write_end(ctx);
		av_write_trailer(ctx->fmt);
	}
	avcodec_close(ctx->codec);
	avcodec_free_context(&ctx->codec);
	if(ctx->sws) {
		ffapi_free_frame(ctx->swsframe);
		sws_freeContext(ctx->sws);
	}

	if(ctx->fmt->oformat) {
		if(!((ctx->fmt->oformat->flags & AVFMT_NOFILE) || (ctx->fmt->flags & AVFMT_FLAG_CUSTOM_IO)))
			ctx->fmt->io_close(ctx->fmt,ctx->fmt->pb);
		avformat_free_context(ctx->fmt);
	}
	else avformat_close_input(&ctx->fmt);

	free(ctx);
	return ret;
}
