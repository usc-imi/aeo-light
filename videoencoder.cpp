//-----------------------------------------------------------------------------
// This file is part of AEO-Light
//
// Copyright (c) 2016-2025 University of South Carolina
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the
// Free Software Foundation; either version 2 of the License, or (at your
// option) any later version.
//
// AEO-Light is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
// for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
//
// Funding for AEO-Light development was provided through a grant from the
// National Endowment for the Humanities
//-----------------------------------------------------------------------------
#include "videoencoder.h"

#include <stdexcept>
#define THROW(s) { throw std::runtime_error((s)); }

//#define DO_AUDIO

#ifdef USELIBAV
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/timecode.h>
//#include <libavutil/timestamp.h>
}
#endif

#include <iostream>

#ifndef UINT16_MAX
	#define UINT16_MAX (0xFFFF)
#endif
#ifndef INT16_MAX
	#define INT16_MAX (0x7FFF)
#endif


#define UMAX(b) ((1ull<<(b))-1)
#define SMAX(b) (UMAX((b)-1))

static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt)
{
	// windows MSVC can't handle the timestamp header, and we're done with this
	// anyhow, so...
#if 0
	AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;
	av_log(NULL, AV_LOG_INFO,
			"PACKET: pts:%s pts_time:%s dts:%s dts_time:%s "
			"duration:%s duration_time:%s stream_index:%d\n",
			av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
			av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
			av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
			pkt->stream_index);
#endif
}

FrameTexture::FrameTexture()
{
	buf = NULL;
	width = 0;
	height = 0;
	format = GL_UNSIGNED_INT_10_10_10_2;
	nComponents = 0;
	isNonNativeEndianess = false;
}

FrameTexture::~FrameTexture()
{
	if(buf) delete [] buf;
}

AudioFromTexture::AudioFromTexture(int _nChannels, int _rate, int _nSamples)
	: nChannels(_nChannels), samplingRate(_rate), nSamples(_nSamples)
{
	if(_nChannels) buf = new float* [_nChannels];
	else buf = NULL;
}

AudioFromTexture::~AudioFromTexture()
{
	if(buf) delete [] buf;
}


VideoEncoder::VideoEncoder(const char *filename)
{
	int i;

	outFmt = NULL;
	audioCodec = NULL;
	videoCodec = NULL;
	audioStream = NULL;
	videoStream = NULL;

	audioFrameIn = NULL;
	audioFrameOut = NULL;
	audioResampleCtx = NULL;
	audioNumSamplesIn = 0;
	audioNumSamplesOut = 0;

	videoFrameIn = NULL;
	videoFrameOut = NULL;
	videoRescaleCtx = NULL;
	videoNumFrames = 0;

	// filename max length is hardcoded at 1024 characters, incl. NULL
	if(sizeof(filename) >= 1024)
		THROW("Filename too long for libav's hardcoded length setting");

	av_log(NULL, AV_LOG_INFO, "av_register_all()\n");
	av_register_all();

	av_log(NULL, AV_LOG_INFO, "avcodec_register_all()\n");
	avcodec_register_all();

	// *** Create the output context based on the filename ***
	av_log(NULL, AV_LOG_INFO, "avformat_alloc_output_context2()\n");
	avformat_alloc_output_context2(&outFmt, NULL, NULL, filename);

	if(!outFmt)
	{
		av_log(NULL, AV_LOG_INFO, "--avformat_alloc_context()\n");
		outFmt = avformat_alloc_context();
		if(!outFmt) THROW("Could not allocate output context");

		// filename max length is hardcoded at 1024 characters, incl. NULL
		if(sizeof(outFmt->filename) <= strlen(filename))
			THROW("Filename too long for libav's hardcoded length setting");
		strcpy(outFmt->filename, filename);

		av_log(NULL, AV_LOG_INFO, "--av_guess_format()\n");
		outFmt->oformat = av_guess_format(NULL, filename, NULL);
		// if the filename doesn't yield a suitable guess, use mpeg
		if(!outFmt->oformat) outFmt->oformat = av_guess_format("mpeg", NULL, NULL);
		if(!outFmt->oformat) THROW("No suitable output file format found.");
	}

	// ******************************
	// *** CONFIGURE VIDEO STREAM ***
	// ******************************

	// LSJ:
	//videoCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
	//av_log(NULL, AV_LOG_INFO, "avcodec_find_encoder(AV_CODEC_ID_H264)\n");
	// Tommy:
	av_log(NULL, AV_LOG_INFO, "avcodec_find_encoder_by_name(\"prores_ks\")\n");
	videoCodec = avcodec_find_encoder_by_name("prores_ks");

	if (!videoCodec) THROW("video Codec not found");
	av_log(NULL, AV_LOG_INFO, "avformat_new_stream(outFmt, videoCodec)\n");
	videoStream = avformat_new_stream(outFmt, videoCodec);
	if(!videoStream) THROW("could not create video stream");
	av_log(NULL, AV_LOG_INFO, "videoStream->id = outFmt->nb_streams - 1\n");
	videoStream->id = outFmt->nb_streams - 1;

	//videoCtx = avcodec_alloc_context3(videoCodec);
	//if (!videoCtx) THROW("Could not allocate video codec context");
	av_log(NULL, AV_LOG_INFO, "videoCtx = videoStream->codec\n");
	AVCodecContext *videoCtx = videoStream->codec;

	av_log(NULL, AV_LOG_INFO, "videoCtx->bit_rate = 400000\n");
	av_log(NULL, AV_LOG_INFO, "videoCtx->width = 640\n");
	av_log(NULL, AV_LOG_INFO, "videoCtx->height = 480\n");
	videoCtx->bit_rate = 400000;
	videoCtx->width = 640;
	videoCtx->height = 480;

	av_log(NULL, AV_LOG_INFO, "videoStream->time_base = 1/25\n");
	videoStream->time_base.num = 1;
	videoStream->time_base.den = 25;

	av_log(NULL, AV_LOG_INFO, "videoCtx->time_base = videoStream->time_base\n");
	videoCtx->time_base = videoStream->time_base;

	//videoCtx->pix_fmt = AV_PIX_FMT_YUV420P;
	av_log(NULL, AV_LOG_INFO, "videoCtx->pix_fmt = AV_PIX_FMT_YUVA444P10LE\n");
	videoCtx->pix_fmt = AV_PIX_FMT_YUVA444P10LE;
	if(videoCodec->pix_fmts)
	{
		for(i=0; videoCodec->pix_fmts[i]; i++)
		{
			if(videoCodec->pix_fmts[i] == videoCtx->pix_fmt)
				break;
		}
		if(videoCodec->pix_fmts[i] != videoCtx->pix_fmt)
		{
			av_log(NULL, AV_LOG_INFO,
					"videoCtx->pix_fmt = videoCodec->pix_fmts[0]\n");
			videoCtx->pix_fmt = videoCodec->pix_fmts[0];
		}
	}

	av_log(NULL, AV_LOG_INFO, "videoCtx->gop_size = 12\n");
	videoCtx->gop_size = 12; // emit intra frame no more than every 12 frames

	// If the container requires global headers, set the encoder to same.
	if(outFmt->oformat->flags & AVFMT_GLOBALHEADER)
	{
		av_log(NULL, AV_LOG_INFO,
                "videoCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER\n");
        videoCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}
	// ******************************
	// *** CONFIGURE AUDIO STREAM ***
	// ******************************

#ifdef DO_AUDIO
	//audioCodec = avcodec_find_encoder(AV_CODEC_ID_AAC);
	av_log(NULL, AV_LOG_INFO,
			"audioCodec = avcodec_find_encoder(AV_CODEC_ID_PCM_S24LE)\n");
	audioCodec = avcodec_find_encoder(AV_CODEC_ID_PCM_S24LE);
	if (!audioCodec) THROW("PCM S24LE audio codec not found");
	av_log(NULL, AV_LOG_INFO,
			"audioStream = avformat_new_stream(outFmt, audioCodec)\n");
	audioStream = avformat_new_stream(outFmt, audioCodec);
	if(!audioStream) THROW("could not create audio stream");
	av_log(NULL, AV_LOG_INFO, "audioStream->id = outFmt->nb_streams - 1 = %d\n",
			outFmt->nb_streams - 1);
	audioStream->id = outFmt->nb_streams - 1;

	//audioCtx = avcodec_alloc_context3(audioCodec);
	//if (!audioCtx) THROW("Could not allocate audio codec context");
	av_log(NULL, AV_LOG_INFO, "AVCodecContext *audioCtx = audioStream->codec\n");
	AVCodecContext *audioCtx = audioStream->codec;

	av_log(NULL, AV_LOG_INFO, "audioCtx->bit_rate = 64000\n");
	audioCtx->bit_rate = 64000;

	av_log(NULL, AV_LOG_INFO, "audioCtx->sample_fmt = AV_SAMPLE_FMT_S32\n");
	audioCtx->sample_fmt = AV_SAMPLE_FMT_S32;
	const enum AVSampleFormat *supp_fmt = audioCodec->sample_fmts;
	while(*supp_fmt != AV_SAMPLE_FMT_NONE)
	{
		if(*supp_fmt == audioCtx->sample_fmt) break;
		supp_fmt++;
	}
	if(*supp_fmt == AV_SAMPLE_FMT_NONE) THROW("Audio sample S32 not supported");

	av_log(NULL, AV_LOG_INFO, "audioCtx->sample_rate = 44100\n");
	audioCtx->sample_rate = 44100;
	// check if this sample rate is supported; if no, use the first supported
	if(audioCodec->supported_samplerates)
	{
		for(i=0; audioCodec->supported_samplerates[i]; i++)
		{
			if(audioCodec->supported_samplerates[i] == audioCtx->sample_rate)
				break;
		}
		if(audioCodec->supported_samplerates[i] != audioCtx->sample_rate)
		{
			av_log(NULL, AV_LOG_INFO, "audioCtx->sample_rate = %d\n",
					audioCodec->supported_samplerates[0]);
			audioCtx->sample_rate = audioCodec->supported_samplerates[0];
		}
	}

	av_log(NULL, AV_LOG_INFO,
			"audioCtx->channel_layout = AV_CH_LAYOUT_STEREO\n");
	audioCtx->channel_layout = AV_CH_LAYOUT_STEREO;
	// check if this channel layout is supported;
	// if no, use the first supported
	if(audioCodec->channel_layouts)
	{
		for(i=0; audioCodec->channel_layouts[i]; i++)
		{
			if(audioCodec->channel_layouts[i] == audioCtx->channel_layout)
				break;
		}
		if(audioCodec->channel_layouts[i] != audioCtx->channel_layout)
		{
			av_log(NULL, AV_LOG_INFO,
					"audioCtx->channel_layout = <dflt>\n");

			audioCtx->channel_layout = audioCodec->channel_layouts[0];
		}
	}
	av_log(NULL, AV_LOG_INFO, "audioCtx->channels = %d\n",
			av_get_channel_layout_nb_channels(audioCtx->channel_layout));
	audioCtx->channels =
			av_get_channel_layout_nb_channels(audioCtx->channel_layout);

	av_log(NULL, AV_LOG_INFO, "audioCtx->time_base = 1/%d\n",
			audioStream->time_base.den = audioCtx->sample_rate);
	audioStream->time_base.num = 1;
	audioStream->time_base.den = audioCtx->sample_rate;

	// XXX: Examples omit audioCtx->time_base. Is it needed? Why not?
	// audioCtx->time_base = audioStream->time_base;

	// If the container requires global headers, set the encoder to same.
	if(outFmt->oformat->flags & AVFMT_GLOBALHEADER)
	{
		av_log(NULL, AV_LOG_INFO,
				"audioCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER\n");

		audioCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}
#endif

	// *************************************
	// *** ALLOCATE THE FRAME STRUCTURES ***
	// *************************************

	av_log(NULL, AV_LOG_INFO, "videoFrameIn = av_frame_alloc()\n");
	videoFrameIn = av_frame_alloc();
	if(!videoFrameIn) THROW("libav can't allocate frame videoFrameIn");

	av_log(NULL, AV_LOG_INFO, "videoFrameOut = av_frame_alloc()\n");
	videoFrameOut = av_frame_alloc();
	if (!videoFrameOut) THROW("libav can't allocate frame videoFrameOut");

#ifdef DO_AUDIO
	av_log(NULL, AV_LOG_INFO, "audioFrameIn = av_frame_alloc()\n");
	audioFrameIn = av_frame_alloc();
	if(!audioFrameIn) THROW("libav can't allocate audioFrameIn");
	av_log(NULL, AV_LOG_INFO, "audioFrameIn->format = AV_SAMPLE_FM_S16\n");
	audioFrameIn->format = AV_SAMPLE_FMT_S16;

	av_log(NULL, AV_LOG_INFO, "audioFrameOut = av_frame_alloc()\n");
	audioFrameOut = av_frame_alloc();
	if(!audioFrameOut) THROW("libav can't allocate audioFrameOut");
#endif

}

void VideoEncoder::ExampleVideoFrame(FrameTexture *frame)
{
	#if 0
	switch(frame->format)
	{
	case GL_UNSIGNED_INT_10_10_10_2: // dpx r10b10g10 source
		if(frame->isNonNativeEndianess)
		{
			#if AV_HAVE_BIGENDIAN
			videoFrameIn->format = AV_PIX_FMT_GBRP10LE;
			av_log(NULL, AV_LOG_INFO,
					"videoFrameIn->format = AV_PIX_FMT_GBRP10LE\n");
			#else
			videoFrameIn->format = AV_PIX_FMT_GBRP10BE;
			av_log(NULL, AV_LOG_INFO,
					"videoFrameIn->format = AV_PIX_FMT_GBRP10BE\n");
			#endif
		}
		else
		{
			videoFrameIn->format = AV_PIX_FMT_GBRP10;
			av_log(NULL, AV_LOG_INFO,
					"videoFrameIn->format = AV_PIX_FMT_GBRP10\n");
		}
		break;
	case GL_UNSIGNED_SHORT:
		if(frame->isNonNativeEndianess)
			THROW("Unsupported endian in 16-bit source format");
		switch(frame->nComponents)
		{
		case 4:
			videoFrameIn->format = AV_PIX_FMT_RGBA64;
			av_log(NULL, AV_LOG_INFO,
					"videoFrameIn->format = AV_PIX_FMT_RGBA64\n");
			break;
		case 3:
			videoFrameIn->format = AV_PIX_FMT_RGB48;
			av_log(NULL, AV_LOG_INFO,
					"videoFrameIn->format = AV_PIX_FMT_RGB48\n");
			break;
		case 1:
			videoFrameIn->format = AV_PIX_FMT_GRAY16;
			av_log(NULL, AV_LOG_INFO,
					"videoFrameIn->format = AV_PIX_FMT_GRAY16\n");
		default:
			THROW("Unsupported nChannels in 16-bit source format");
		}
		break;
	case GL_UNSIGNED_BYTE:
		switch(frame->nComponents)
		{
		case 4:
			videoFrameIn->format = AV_PIX_FMT_RGBA;
			av_log(NULL, AV_LOG_INFO,
					"videoFrameIn->format = AV_PIX_FMT_RGBA\n");
			break;
		case 3:
			videoFrameIn->format = AV_PIX_FMT_RGB24;
			av_log(NULL, AV_LOG_INFO,
					"videoFrameIn->format = AV_PIX_FMT_RGB24\n");
			break;
		case 1:
			videoFrameIn->format = AV_PIX_FMT_GRAY8;
			av_log(NULL, AV_LOG_INFO,
					"videoFrameIn->format = AV_PIX_FMT_GRAY8\n");
			break;
		default:
			THROW("Unsupported nChannels in 8-bit source format");
		}
		break;
	default:
		THROW("Unsupported pixel format in source format");
	}
	#endif

	av_log(NULL, AV_LOG_INFO, "videoFrameIn->format = AV_PIX_FMT_RGBA\n");
	av_log(NULL, AV_LOG_INFO, "videoFrameIn->width = frame->width\n");
	av_log(NULL, AV_LOG_INFO, "videoFrameIn->height = frame->height\n");
	videoFrameIn->format = AV_PIX_FMT_RGBA;
	videoFrameIn->width = frame->width;
	videoFrameIn->height = frame->height;

	av_log(NULL, AV_LOG_INFO, "av_frame_get_buffer(videoFrameIn, 32)\n");
	if(av_frame_get_buffer(videoFrameIn, 32) < 0)
		THROW("Could not allocate videoFrameIn buffer");

	// XXX: Shouldn't be modifying frame here. But if this is left out,
	// AEO-Light responds with "critical error". Need to investigate.
	frame->buf = videoFrameIn->data[0];
}

void VideoEncoder::ExampleAudioFrame(const AudioFromTexture *example)
{
#ifdef DO_AUDIO
	av_log(NULL, AV_LOG_INFO,
			"audioFrameIn->channel_layout = AV_CH_LAYOUT_STEREO (%d)\n",
			AV_CH_LAYOUT_STEREO);
	audioFrameIn->channel_layout = AV_CH_LAYOUT_STEREO;
	av_log(NULL, AV_LOG_INFO, "audioFrameIn->format = AV_SAMPLE_FMT_S32\n");
	audioFrameIn->format = AV_SAMPLE_FMT_S32;
	av_log(NULL, AV_LOG_INFO, "audioFrameIn->sample_rate = %d\n",
			example->samplingRate);
	audioFrameIn->sample_rate = example->samplingRate;
	av_log(NULL, AV_LOG_INFO, "audioFrameIn->nb_samples = %d\n",
			example->nSamples);
	audioFrameIn->nb_samples = example->nSamples;

	if(audioFrameIn->nb_samples)
	{
		av_log(NULL, AV_LOG_INFO, "av_frame_get_buffer(audioFrameIn, 0)\n");
		if(av_frame_get_buffer(audioFrameIn, 0) < 0)
			THROW("Could not allocate audio frame buffer");
	}

#endif
}

void VideoEncoder::Open()
{
	if(!outFmt) return;

	av_log(NULL, AV_LOG_INFO, "videoCtx = videoStream->codec\n");
	AVCodecContext *videoCtx = videoStream->codec;

#ifdef DO_AUDIO
	av_log(NULL, AV_LOG_INFO, "AVCodecContext *audioCtx = audioStream->codec\n");
	AVCodecContext *audioCtx = audioStream->codec;
#endif

	av_log(NULL, AV_LOG_INFO, "avcodec_open2(videoCtx, videoCodec, NULL)\n");
	if(avcodec_open2(videoCtx, videoCodec, NULL) < 0)
		THROW("Could not open video codec");

#ifdef DO_AUDIO
	av_log(NULL, AV_LOG_INFO, "avcodec_open2(audioCtx, audioCodec, NULL)\n");
	if(avcodec_open2(audioCtx, audioCodec, NULL) < 0)
		THROW("Could not open audio codec");
#endif

	// ********************************
	// *** PREPARE ENCODING BUFFERS ***
	// ********************************

	av_log(NULL, AV_LOG_INFO, "videoFrameOut->format = videoCtx->pix_fmt\n");
	av_log(NULL, AV_LOG_INFO, "videoFrameOut->width = videoCtx->width\n");
	av_log(NULL, AV_LOG_INFO, "videoFrameOut->height = videoCtx->height\n");
	videoFrameOut->format = videoCtx->pix_fmt;
	videoFrameOut->width = videoCtx->width;
	videoFrameOut->height = videoCtx->height;
	av_log(NULL, AV_LOG_INFO, "av_frame_get_buffer(videoFrameOut, 32)\n");
	if(av_frame_get_buffer(videoFrameOut, 32) < 0)
		THROW("Could not allocate videoFrameOut buffer");

	av_log(NULL, AV_LOG_INFO,
			"videoRescaleCtx = sws_getContext(%d,%d,%s,%d,%d,%s,"
			"SWS_BICUBIC, NULL, NULL, NULL)\n",
			videoFrameIn->width, videoFrameIn->height,
			av_get_pix_fmt_name(AVPixelFormat(videoFrameIn->format)),
			videoCtx->width, videoCtx->height,
			av_get_pix_fmt_name(AVPixelFormat(videoFrameOut->format))
			);
	videoRescaleCtx = sws_getContext(
			videoFrameIn->width, videoFrameIn->height,
			AVPixelFormat(videoFrameIn->format),
			videoCtx->width, videoCtx->height,
			AVPixelFormat(videoCtx->pix_fmt),
			SWS_BICUBIC, NULL, NULL, NULL);
	if(!videoRescaleCtx)
		THROW("Couldn't initialize video rescale context");

#ifdef DO_AUDIO
	audioFrameOut->format = audioCtx->sample_fmt;
	audioFrameOut->channel_layout = audioCtx->channel_layout;
	audioFrameOut->sample_rate = audioCtx->sample_rate;
	audioFrameOut->nb_samples = audioCtx->frame_size;
	av_log(NULL, AV_LOG_INFO, "audioFrameOut->format = audioCtx->sample_fmt\n");
	av_log(NULL, AV_LOG_INFO, "audioFrameOut->channel_layout = audioCtx->channel_layout\n");
	av_log(NULL, AV_LOG_INFO, "audioFrameOut->sample_rate = audioCtx->sample_rate\n");
	av_log(NULL, AV_LOG_INFO, "audioFrameOut->nb_samples = audioCtx->frame_size\n");

	av_log(NULL, AV_LOG_INFO,
			"audioResampleCtx = (NULL, %llu, %s, %d, %lld, %s, %d, 0, NULL)\n",
			audioFrameOut->channel_layout,
			av_get_sample_fmt_name(AVSampleFormat(audioFrameOut->format)),
			audioFrameOut->sample_rate,
			audioFrameIn->channel_layout,
			av_get_sample_fmt_name(AVSampleFormat(audioFrameIn->format)),
			audioFrameIn->sample_rate);

	audioResampleCtx = swr_alloc_set_opts(NULL,
			audioFrameOut->channel_layout,
			AVSampleFormat(audioFrameOut->format),
			audioFrameOut->sample_rate,
			audioFrameIn->channel_layout,
			AVSampleFormat(audioFrameIn->format),
			audioFrameIn->sample_rate,
			0, NULL);

	if(swr_init(audioResampleCtx) < 0)
		THROW("Couldn't initialize audio resample context");
#endif

	if (!(outFmt->flags & AVFMT_NOFILE))
	{
		av_log(NULL, AV_LOG_INFO,
				"avio_open(&outFmt->pb, outFmt->filename, AVIO_FLAG_WRITE)\n");
		if(avio_open(&outFmt->pb, outFmt->filename, AVIO_FLAG_WRITE) < 0)
			THROW("Could not open output file.");
	}

	av_log(NULL, AV_LOG_INFO, "avformat_write_header(outFmt, NULL)\n");
	if(avformat_write_header(outFmt, NULL) < 0)
		THROW("Error opening output file");

}

void VideoEncoder::Close()
{
	if(!outFmt) return;

	av_log(NULL, AV_LOG_INFO, "av_write_trailer(outFmt)\n");
	if(av_write_trailer(outFmt) < 0)
		THROW("Error writing output file trailer");

#ifdef DO_AUDIO
	av_log(NULL, AV_LOG_INFO, "avcodec_close(audioStream->codec)\n");
	avcodec_close(audioStream->codec);
#endif
	av_log(NULL, AV_LOG_INFO, "avcodec_close(videoStream->codec)\n");
	avcodec_close(videoStream->codec);

	if(!(outFmt->flags & AVFMT_NOFILE))
	{
		av_log(NULL, AV_LOG_INFO, "avio_closep(&outFmt->pb)\n");
		avio_closep(&outFmt->pb);
	}
	av_log(NULL, AV_LOG_INFO, "avformat_free_context(outFmt)\n");
	avformat_free_context(outFmt);
	outFmt = NULL;
}

void VideoEncoder::WriteVideoFrame(const FrameTexture *frame)
{
	av_log(NULL, AV_LOG_INFO,
			"VideoEncoder::WriteVideoFrame(FrameTexture *frame)\n");

	av_log(NULL, AV_LOG_INFO,
			"avsize = av_image_get_buffer_size()\n");
	int avsize = av_image_get_buffer_size(
			AVPixelFormat(videoFrameIn->format),
			videoFrameIn->width, videoFrameIn->height, 1);
	av_log(NULL, AV_LOG_INFO, "avsize = %d\n", avsize);

	// XXX: 10-10-10-2 packed reads as 6 bytes to av_image_get_buffer_size,
	//  so fudge and just verify that bufSize isn't greater than avsize,
	//  rather than verifying that they're equal.

	//if(frame->bufSize != avsize)
	if(frame->bufSize > avsize)
	{
		char msg[240];
		snprintf(msg, 240,
				"Video frame size mismatch: "
				"tex %d %dx%d vs. avframe %d (%s) %dx%d",
				frame->bufSize, frame->width, frame->height,
				avsize,
				av_get_pix_fmt_name(AVPixelFormat(videoFrameIn->format)),
				videoFrameIn->width, videoFrameIn->height);
		av_log(NULL, AV_LOG_INFO, "%s", msg);
		THROW(msg);
	}

	/* when we pass a frame to the encoder, it may keep a reference to it
	 * internally;
	 * make sure we do not overwrite it here
	 */
	//av_frame_make_writable(this->videoFrameIn);
	//memcpy(this->videoFrameIn->data[0], frame->buf, frame->bufSize);

	av_log(NULL, AV_LOG_INFO, "videoCtx = this->videoStream->codec\n");
	AVCodecContext *videoCtx = this->videoStream->codec;

	/* when we pass a frame to the encoder, it may keep a reference to it
	 * internally;
	 * make sure we do not overwrite it here
	 */
	av_log(NULL, AV_LOG_INFO, "av_frame_make_writable(this->videoFrameOut)\n");
	av_frame_make_writable(this->videoFrameOut);

	av_log(NULL, AV_LOG_INFO,
			"sws_scale(this->videoRescaleCtx, videoFrameIn, videoFrameOut\n");
	sws_scale(this->videoRescaleCtx,
			(uint8_t const *const *)(this->videoFrameIn->data),
			this->videoFrameIn->linesize,
			0,
			videoCtx->height,
			this->videoFrameOut->data,
			this->videoFrameOut->linesize);

	if(this->outFmt)
	{
		AVPacket pkt;
		int ret;

		av_log(NULL, AV_LOG_INFO, "AVPacket pkt(0)\n");
		pkt.data = NULL; //packet data will be allocated by the encoder
		pkt.size = 0;
		av_log(NULL, AV_LOG_INFO, "pkt.pos = this->videoNumFrames = %lld\n",
				this->videoNumFrames);
		pkt.pos = this->videoNumFrames;
		av_log(NULL, AV_LOG_INFO, "this->videoNumFrames++\n");
		this->videoNumFrames++;

		av_log(NULL, AV_LOG_INFO, "av_init_packet(&pkt)\n");
		av_init_packet(&pkt);
		av_log(NULL, AV_LOG_INFO,
				"pkt.stream_index = this->videoStream->index = %d\n",
				this->videoStream->index);
		pkt.stream_index = this->videoStream->index;
		av_log(NULL, AV_LOG_INFO,
				"v_packet_rescale_ts(&pkt, videoCtx->time_base, "
				"videoStream->time_base)\n");
		av_packet_rescale_ts(&pkt, videoCtx->time_base,
				this->videoStream->time_base);

		int got_packet;
		av_log(NULL, AV_LOG_INFO,
				"avcodec_encode_video2(videoCtx, &pkt, videoFrameOut, &got)\n");
		ret = avcodec_encode_video2(
				videoCtx, &pkt, this->videoFrameOut, &got_packet);
		if(ret < 0)	THROW("failed to encode video frame");

		if(got_packet)
		{
			log_packet(this->outFmt, &pkt);
			av_log(NULL, AV_LOG_INFO,
					"av_interleaved_write_frame(this->outFmt, &pkt)\n");
			ret = av_interleaved_write_frame(this->outFmt, &pkt);
			if(ret < 0)	THROW("Error writing video frame to output");
		}

		av_log(NULL, AV_LOG_INFO, "av_packet_unref(&pkt)\n");
		av_packet_unref(&pkt);
		// av_free_packet(&pkt);

		// XXX: what about delayed frames?
		// ffmpeg.org/doxygen/trunk/decoding__encoding_8c_source.html#l00459
	}
}

void VideoEncoder::WriteAudioFrame(const AudioFromTexture *audio)
{
#ifdef DO_AUDIO
	if(audio->nChannels != 2) THROW("audio sample must be stereo");

	int16_t *samples;
	const int nbits = 16;

	av_log(NULL, AV_LOG_INFO,
			"VideoEncoder::WriteAudioFrame(AudioFromTexture *audio)\n");

	// from line 212 of
	// libav.org/documentation/doxygen/master/encode__audio_8c_source.html
	// better, from ffmpeg, is this one, using int16_t instead of uint16_t:
	// https://www.ffmpeg.org/doxygen/3.0/muxing_8c_source.html#l00267
	samples = (int16_t *)this->audioFrameIn->data[0];

	int s = 0;
	for(int i = 0; i<this->audioFrameIn->nb_samples; ++i)
	{
		for(int c = 0; c < audio->nChannels; ++c)
		{
			samples[s++] =
					int32_t((audio->buf[c][i]*UMAX(nbits))-(UMAX(nbits)/2));
		}
	}

	this->audioFrameIn->pts = this->audioNumSamplesIn;
	this->audioNumSamplesIn += this->audioFrameIn->nb_samples;

	AVCodecContext *audioCtx = this->audioStream->codec;

	// compute destination number of samples
	// https://www.ffmpeg.org/doxygen/3.0/muxing_8c_source.html#l00312
	av_log(NULL, AV_LOG_INFO, "dest_nb_samples = av_rescale_rnd(...)\n");
	int dest_nb_samples = av_rescale_rnd(
			swr_get_delay(this->audioResampleCtx, audioCtx->sample_rate) +
				this->audioFrameIn->nb_samples,
			audioCtx->sample_rate, audioCtx->sample_rate, AV_ROUND_UP);

	av_log(NULL, AV_LOG_INFO, "dest_nb_samples: %d, frame->nb_samples: %d\n",
			dest_nb_samples, this->audioFrameIn->nb_samples);

	if(dest_nb_samples != this->audioFrameIn->nb_samples)
		THROW("av_assert: nb_samples do not match");

	/* when we pass a frame to the encoder, it may keep a reference to it
	 * internally;
	 * make sure we do not overwrite it here
	 */
	av_frame_make_writable(this->audioFrameOut);

	av_log(NULL, AV_LOG_INFO, "audioCtx = this->audioStream->codec\n");

	// XXX: No need to account for multiple channels? Is that pulled from
	// the resample context?
	// the bizarre/twitchy cast is direct from
	// https://www.ffmpeg.org/doxygen/3.0/muxing_8c_source.html#l00328
	av_log(NULL, AV_LOG_INFO,
			"swr_convert(this->audioResampleCtx, audioFrameIn, audioFrameOut\n");
	swr_convert(this->audioResampleCtx,
			this->audioFrameOut->data, dest_nb_samples,
			(const uint8_t **)this->audioFrameIn->data,
			this->audioFrameIn->nb_samples);

	this->audioFrameOut->pts = av_rescale_q(this->audioNumSamplesOut,
			(AVRational){1, audioCtx->sample_rate}, audioCtx->time_base);

	this->audioNumSamplesOut += dest_nb_samples;

	if(this->outFmt)
	{
		AVPacket pkt;
		int ret;

		av_log(NULL, AV_LOG_INFO, "AVPacket pkt(0)\n");
		pkt.data = NULL; //packet data will be allocated by the encoder
		pkt.size = 0;

		// XXX: Guess we don't need to set pkt.pos for audio?
		// https://www.ffmpeg.org/doxygen/3.0/muxing_8c_source.html#l00305
		//pkt.pos = this->videoNumFrames;
		//this->videoNumFrames++;
		//av_log(NULL, AV_LOG_INFO, "pkt.pos = this->videoNumFrames++\n");

		av_log(NULL, AV_LOG_INFO, "av_init_packet(&pkt)\n");
		av_init_packet(&pkt);
		av_log(NULL, AV_LOG_INFO,
				"pkt.stream_index = this->audioStream->index = %d\n",
				this->audioStream->index);
		pkt.stream_index = this->audioStream->index;
		av_log(NULL, AV_LOG_INFO,
				"v_packet_rescale_ts(&pkt, audioCtx->time_base, "
				"audioStream->time_base)\n");
		av_packet_rescale_ts(&pkt, audioCtx->time_base,
				this->audioStream->time_base);

		int got_packet;
		av_log(NULL, AV_LOG_INFO,
				"avcodec_encode_audio2(audioCtx, &pkt, audioFrameOut, &got)\n");
		ret = avcodec_encode_audio2(
				audioCtx, &pkt, this->audioFrameOut, &got_packet);
		if(ret < 0)	THROW("failed to encode audio frame");

		if(got_packet)
		{
			log_packet(this->outFmt, &pkt);
			ret = av_interleaved_write_frame(this->outFmt, &pkt);
			av_log(NULL, AV_LOG_INFO,
					"av_interleaved_write_frame(this->outFmt, &pkt)\n");
			if(ret < 0)	THROW("Error writing audio frame to output");
		}

		av_packet_unref(&pkt);
		av_log(NULL, AV_LOG_INFO, "av_packet_unref(&pkt)\n");
		// av_free_packet(&pkt);

		// XXX: what about delayed frames?
		// ffmpeg.org/doxygen/trunk/decoding__encoding_8c_source.html#l00459
	}
#endif
}

/* OPEN FILE:
 * avformat_alloc_output_context2
 * add video stream ( av_new_stream )
 * add audio stream
 * open video stream
 * open audio stream
 *
 * SET METADATA:
 * av_dict_set
 *
 * WRITE FRAMES:
 * av_interleaved_write_frame
 *
 * CLOSE FILE:
 * close video stream
 * close audio stream
 * close file ( avio_closep )
 */
