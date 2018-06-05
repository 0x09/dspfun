/*
 * ffapi - Wrapper for pulling/pushing frames with libav*
 * Copyright 2013-2016 0x09.net.
 */

#ifndef FFAPI_H
#define FFAPI_H

#include <libswscale/swscale.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libavutil/parseutils.h>
#include <assert.h>
#include <stdbool.h>

typedef struct FFColorProperties {
	enum AVColorRange color_range;
	enum AVColorPrimaries color_primaries;
	enum AVColorTransferCharacteristic color_trc;
	enum AVColorSpace color_space;
	enum AVChromaLocation chroma_location;
	enum AVPixelFormat pix_fmt;
} FFColorProperties;

typedef struct FFContext {
	AVFormatContext* fmt;
	AVCodecContext* codec;
	AVStream* st;
	AVPixFmtDescriptor* pixdesc;
	struct SwsContext* sws;
	AVFrame* swsframe;
} FFContext;

void       ffapi_parse_color_props(FFColorProperties* c, const char* props);
FFContext* ffapi_open_input (const char* file, const char* options,
                             const char* format, FFColorProperties* color_props,
                             unsigned long* components, unsigned long (*widths)[4], unsigned long (*heights)[4], unsigned long* frames,
                             AVRational* rate, bool calc_frames);
FFContext* ffapi_open_output(const char* file, const char* options,
                             const char* format, const char* encoder, enum AVCodecID preferred_encoder,
                             const FFColorProperties* in_color_props,
                             unsigned long width, unsigned long height, AVRational rate);
AVFrame*  ffapi_alloc_frame(FFContext*);
void      ffapi_free_frame (AVFrame*);
void      ffapi_clear_frame(AVFrame*);
int       ffapi_read_frame (FFContext*, AVFrame*);
size_t    ffapi_seek_frame (FFContext*, size_t offset, void (*progress)(size_t));
int       ffapi_write_frame(FFContext*, AVFrame*);
int       ffapi_close(FFContext*);

// 8 bit only for now
#define FFA_PEL(frame,comp,x,y) frame->data[comp.plane][y*frame->linesize[comp.plane]+x*comp.step+comp.offset]
static inline void ffapi_setpel_direct(AVFrame* frame, size_t x, size_t y, AVComponentDescriptor comp, unsigned char val) {
	assert(comp.depth == 8);
	FFA_PEL(frame,comp,x,y) = val;
}
static inline unsigned char ffapi_getpel_direct(AVFrame* frame, size_t x, size_t y, AVComponentDescriptor comp) {
	assert(comp.depth == 8);
	return FFA_PEL(frame,comp,x,y);
}

static inline void ffapi_setpell_direct(AVFrame* frame, size_t x, size_t y, AVComponentDescriptor comp, long val) {
	ffapi_setpel_direct(frame,x,y,comp,val<0?0:val>255?255:val);
}

#define ffapi_setpel_direct(AVFrame,x,y,comp,val) \
	_Generic((val),\
		long double:(ffapi_setpell_direct)(AVFrame,x,y,comp,lrintl(val)),\
		     double:(ffapi_setpell_direct)(AVFrame,x,y,comp,lrint (val)),\
		      float:(ffapi_setpell_direct)(AVFrame,x,y,comp,lrintf(val)),\
		    default: (ffapi_setpel_direct)(AVFrame,x,y,comp,       val)\
	)

#define ffapi_setpel(FFContext,AVFrame,x,y,c,val) ffapi_setpel_direct(AVFrame,x,y,FFContext->pixdesc->comp[c],val)
#define ffapi_getpel(FFContext,AVFrame,x,y,c) ffapi_getpel_direct(AVFrame,x,y,FFContext->pixdesc->comp[c])
#define ffapi_setpixel(FFContext,AVFrame,x,y,val)\
	for(uint_fast8_t ffapi__i = 0; ffapi__i < FFContext->pixdesc->nb_components; ffapi__i++)\
		ffapi_setpel(FFContext,AVFrame,x,y,ffapi__i,(val)[ffapi__i])

#define ffapi_getpixel(FFContext,AVFrame,x,y,val)\
	for(uint_fast8_t ffapi__i = 0; ffapi__i < FFContext->pixdesc->nb_components; ffapi__i++)\
		(val)[ffapi__i] = ffapi_getpel(FFContext,AVFrame,x,y,ffapi__i)

//av_pix_fmt_desc_get_id does not work for descriptors copied to the stack
static inline enum AVPixelFormat ffapi_pix_fmt_desc_get_id(AVPixFmtDescriptor* desc) {
	return av_get_pix_fmt(desc->name);
}
#endif
