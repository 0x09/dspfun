/*
 * ffapi - Wrapper for pulling/pushing frames with libav*
 */

#ifndef FFAPI_H
#define FFAPI_H

#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libavutil/parseutils.h>
#include <libavutil/intreadwrite.h>
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
	struct FFColorProperties color_props;
} FFContext;

typedef bool (ffapi_pix_fmt_filter)(const AVPixFmtDescriptor*);
// pix fmts supported by ffapi_getpel(f)
ffapi_pix_fmt_filter ffapi_pixfmts_8bit_pel, ffapi_pixfmts_32_bit_float_pel;

void       ffapi_parse_color_props(FFColorProperties* c, const char* props);
FFContext* ffapi_open_input (const char* file, const char* options,
                             const char* format, FFColorProperties* color_props, ffapi_pix_fmt_filter*,
                             unsigned long* components, unsigned long (*widths)[4], unsigned long (*heights)[4], uint64_t* frames,
                             AVRational* rate, bool calc_frames);
FFContext* ffapi_open_output(const char* file, const char* options,
                             const char* format, const char* encoder, enum AVCodecID preferred_encoder,
                             const FFColorProperties* in_color_props,
                             unsigned long width, unsigned long height, AVRational rate);
AVFrame*  ffapi_alloc_frame(FFContext*);
void      ffapi_free_frame (AVFrame*);
void      ffapi_clear_frame(AVFrame*);
int       ffapi_read_frame (FFContext*, AVFrame*);
uint64_t  ffapi_seek_frame (FFContext*, uint64_t offset, void (*progress)(uint64_t));
int       ffapi_write_frame(FFContext*, AVFrame*);
int       ffapi_close(FFContext*);

#define FFA_PEL(frame,comp,x,y) frame->data[comp.plane][y*frame->linesize[comp.plane]+x*comp.step+comp.offset]

// 8-bit pel accessors
static inline void ffapi_setpel_direct(AVFrame* frame, size_t x, size_t y, AVComponentDescriptor comp, unsigned char val) {
	FFA_PEL(frame,comp,x,y) = val;
}
static inline unsigned char ffapi_getpel_direct(AVFrame* frame, size_t x, size_t y, AVComponentDescriptor comp) {
	return FFA_PEL(frame,comp,x,y);
}

// float pel accessors
static inline void ffapi_setpelf(FFContext* ctx, AVFrame* frame, size_t x, size_t y, int c, float val) {
	AVComponentDescriptor comp = ctx->pixdesc->comp[c];
	uint32_t valu = (union { float f; uint32_t u; }){val}.u;
	uint8_t* data = &FFA_PEL(frame,comp,x,y);
	if(ctx->pixdesc->flags & AV_PIX_FMT_FLAG_BE)
		AV_WB32(data,valu);
	else
		AV_WL32(data,valu);
}

static inline float ffapi_getpelf(FFContext* ctx, AVFrame* frame, size_t x, size_t y, int c) {
	AVComponentDescriptor comp = ctx->pixdesc->comp[c];
	uint8_t* data = &FFA_PEL(frame,comp,x,y);
	return (union { uint32_t u; float f; }) {
		.u = ctx->pixdesc->flags & AV_PIX_FMT_FLAG_BE ? AV_RB32(data) : AV_RL32(data)
	}.f;
}

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
