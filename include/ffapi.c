/*
 * ffapi - Wrapper for pulling/pushing frames with libav*
 * Copyright 2013-2016 0x09.net.
 */

#include "ffapi.h"

#include <libavutil/parseutils.h>

void ffapi_init(int loglevel) {
	av_log_set_level(loglevel * 8);
	av_register_all();
	avcodec_register_all();
}

FFContext* ffapi_open_input(const char* file, const char* options,
                         const char* format, const char* pix_fmt,
                         unsigned long* components, unsigned long (*widths)[4], unsigned long (*heights)[4], unsigned long* frames, AVRational* rate, bool calc_frames) {
	FFContext* in = calloc(1,sizeof(*in));

	AVDictionary* opts = NULL;
	av_dict_parse_string(&opts,options,"=",":",0);

	AVInputFormat* ifmt = NULL;
	if(format) ifmt = av_find_input_format(format);
	avformat_open_input(&in->fmt,file,ifmt,&opts);
	AVCodec* dec;
	avformat_find_stream_info(in->fmt,NULL);
	int stream = av_find_best_stream(in->fmt,AVMEDIA_TYPE_VIDEO,-1,-1,&dec,0);
	//if(stream < 0) goto error;
	in->st = in->fmt->streams[stream];
	AVCodecContext* avc = in->st->codec;
	avc->refcounted_frames = 1;
	avcodec_open2(avc,dec,&opts);

	av_dict_free(&opts);
	//av_dump_format(in->fmt,0,in->fmt->filename,1); //bus error

	if(rate)
		*rate = in->st->r_frame_rate;

	in->pixdesc = (AVPixFmtDescriptor*)av_pix_fmt_desc_get(avc->pix_fmt);
	if(frames) {
		*frames = in->st->nb_frames;
		if(!*frames) {
			if(!strcmp(in->fmt->iformat->name,"image2") || !strcmp(in->fmt->iformat->name,"png_pipe"))
				*frames = 1;
			else if(calc_frames) {
				int64_t start = avio_tell(in->fmt->pb);
				AVFrame* frame = ffapi_alloc_frame(in);
				while(!ffapi_read_frame(in,frame))
					;
				ffapi_free_frame(frame);
				*frames = avc->frame_number;
				//avio_seek(in->fmt->pb,start,SEEK_SET);
				ffapi_close(in);
				return ffapi_open_input(file,options,format,pix_fmt,components,widths,heights,NULL,rate,false);
			}
		}
	}

	if(pix_fmt) {
		enum AVPixelFormat out_pix_fmt = av_get_pix_fmt(pix_fmt);
		if(out_pix_fmt != AV_PIX_FMT_NONE && out_pix_fmt != avc->pix_fmt) {
			in->sws = sws_getCachedContext(NULL,avc->width,avc->height,avc->pix_fmt,avc->width,avc->height,out_pix_fmt,SWS_BICUBIC,NULL,NULL,NULL);
			in->swsframe = av_frame_alloc();
			in->pixdesc = (AVPixFmtDescriptor*)av_pix_fmt_desc_get(out_pix_fmt);
		}
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
}

FFContext* ffapi_open_output(const char* file, const char* options,
                         const char* format, const char* encoder, enum AVCodecID preferred_encoder, const char* pix_fmt, enum AVPixelFormat in_pix_fmt,
                         unsigned long width, unsigned long height, AVRational rate) {
	 FFContext* out = calloc(1,sizeof(*out));
	 if(avformat_alloc_output_context2(&out->fmt,NULL,format,file))
		goto error;

	AVCodec* enc = NULL;
	if(encoder)
		enc = avcodec_find_encoder_by_name(encoder);
	if(!enc || avformat_query_codec(out->fmt->oformat,enc->id,FF_COMPLIANCE_NORMAL) != 1)
		enc = avcodec_find_encoder(preferred_encoder);
	if(!enc || avformat_query_codec(out->fmt->oformat,enc->id,FF_COMPLIANCE_NORMAL) != 1)
		enc = avcodec_find_encoder(av_guess_codec(out->fmt->oformat,NULL,out->fmt->filename,NULL,AVMEDIA_TYPE_VIDEO));
	if(!enc)
		goto error;

	out->st = avformat_new_stream(out->fmt,enc);
	AVCodecContext* avc = out->st->codec;
	out->pixdesc = (AVPixFmtDescriptor*)av_pix_fmt_desc_get(in_pix_fmt);

	if(pix_fmt) {
		avc->pix_fmt = av_get_pix_fmt(pix_fmt);
		if(enc->pix_fmts) {
			const enum AVPixelFormat* i;
			for(i = enc->pix_fmts; *i != avc->pix_fmt && *i != AV_PIX_FMT_NONE; i++)
				;
			if(*i == AV_PIX_FMT_NONE)
				goto error;
		}
	}
	else if(enc->pix_fmts)
		avc->pix_fmt = avcodec_find_best_pix_fmt_of_list(enc->pix_fmts,in_pix_fmt,0,NULL);
	else avc->pix_fmt = in_pix_fmt;

	avc->width  = width;
	avc->height = height;
	avc->time_base = out->st->time_base = av_inv_q(out->st->r_frame_rate = out->st->avg_frame_rate = rate);

	AVDictionary* opts = NULL;
	av_dict_parse_string(&opts,options,"=",":",0);
	if(avcodec_open2(avc,enc,&opts))
		goto error;
	if(avio_open2(&out->fmt->pb,out->fmt->filename,AVIO_FLAG_WRITE,NULL,&opts))
		goto error;
	if(avformat_write_header(out->fmt,&opts))
		goto error;
	av_dict_free(&opts);
	av_dump_format(out->fmt,0,out->fmt->filename,1);

	if(avc->pix_fmt != in_pix_fmt) {
		out->sws = sws_getCachedContext(NULL,width,height,in_pix_fmt,avc->width,avc->height,avc->pix_fmt,SWS_BICUBIC,NULL,NULL,NULL);
		out->swsframe = av_frame_alloc();
		out->swsframe->width  = avc->width;
		out->swsframe->height = avc->height;
		out->swsframe->format = avc->pix_fmt;
		av_frame_get_buffer(out->swsframe,1);
	}

	 return out;
error:
	avformat_free_context(out->fmt);
	free(out);
	return NULL;
}

AVFrame* ffapi_alloc_frame(FFContext* ctx) {
	AVFrame* frame = av_frame_alloc();
	if(ctx->fmt->oformat || ctx->sws) {
		frame->width  = ctx->st->codec->width;
		frame->height = ctx->st->codec->height;
		frame->format = av_pix_fmt_desc_get_id(ctx->pixdesc);
		if(av_frame_get_buffer(frame,1))
			return NULL;
	}
	return frame;
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
	AVPacket packet = {0};
	int err, got_frame;
	do {
		while(!(err = av_read_frame(in->fmt,&packet)) && packet.stream_index != in->st->index)
			av_packet_unref(&packet);
		if(err) {
			packet.data = NULL;
			packet.size = 0;
		}
		AVPacket pcpy = packet;
		do {
			int ret;
			if((ret = avcodec_decode_video2(in->st->codec,readframe,&got_frame,&pcpy)) < 0) {
				av_packet_unref(&packet);
				return ret;
			}
			pcpy.data += ret;
			pcpy.size -= ret;
		} while(!got_frame && pcpy.size > 0);
		assert(pcpy.size == 0);
		av_packet_unref(&packet);
	} while(!(err || got_frame));
	if(got_frame) {
		if(in->sws)
			sws_scale(in->sws,(const uint8_t* const*)in->swsframe->data,in->swsframe->linesize,0,in->st->codec->height,frame->data,frame->linesize);
		frame->pts = av_frame_get_best_effort_timestamp(readframe);
	}
	return err * !got_frame;
}

int ffapi_write_frame(FFContext* out, AVFrame* frame) {
	AVFrame* writeframe = out->swsframe;
	if(out->sws) sws_scale(out->sws,(const uint8_t* const*)frame->data,frame->linesize,0,out->st->codec->height,out->swsframe->data,out->swsframe->linesize);
	else writeframe = frame;
	AVCodecContext* codec = out->st->codec;
	writeframe->pts = frame->pts ? frame->pts : codec->frame_number; //for now timebase == 1/rate
	AVPacket packet = {0};
	av_init_packet(&packet);
	int got_packet;
	int result = avcodec_encode_video2(codec,&packet,writeframe,&got_packet);
	if(got_packet) {
		if(packet.pts != AV_NOPTS_VALUE)
			packet.pts = av_rescale_q(packet.pts,codec->time_base,out->st->time_base);
		else packet.pts = av_frame_get_best_effort_timestamp(writeframe);
		if(packet.dts != AV_NOPTS_VALUE)
			packet.dts = av_rescale_q(packet.dts,codec->time_base,out->st->time_base);
		packet.stream_index = out->st->index;
		result = av_write_frame(out->fmt,&packet);
		av_packet_unref(&packet);
	}
	return result;
}

static int write_end(FFContext* out) {
	AVCodecContext* codec = out->st->codec;
	AVPacket packet = {0};
	av_init_packet(&packet);
	int got_packet;
	int err;
	while(!(err = avcodec_encode_video2(codec,&packet,NULL,&got_packet)) && got_packet) {
		if(packet.pts != AV_NOPTS_VALUE)
			packet.pts = av_rescale_q(packet.pts,codec->time_base,out->st->time_base);
		if(packet.dts != AV_NOPTS_VALUE)
			packet.dts = av_rescale_q(packet.dts,codec->time_base,out->st->time_base);
		packet.stream_index = out->st->index;
		err = av_write_frame(out->fmt,&packet);
		av_packet_unref(&packet);
	}
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
	avcodec_close(ctx->st->codec);
	if(ctx->sws) {
		ffapi_free_frame(ctx->swsframe);
		sws_freeContext(ctx->sws);
	}

	if(ctx->fmt->oformat) {
		if(!((ctx->fmt->oformat->flags & AVFMT_NOFILE) || (ctx->fmt->flags & AVFMT_FLAG_CUSTOM_IO)))
			avio_close(ctx->fmt->pb);
		avformat_free_context(ctx->fmt);
	}
	else avformat_close_input(&ctx->fmt);

	free(ctx);
	return ret;
}
