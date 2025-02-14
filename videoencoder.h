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
#ifndef VIDEOENCODER_H
#define VIDEOENCODER_H

#define USELIBAV

#ifdef USELIBAV
extern "C"
{
#include <libavutil/avconfig.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/timecode.h>
}
#endif

#include <QOpenGLTexture>

class FrameTexture
{
public:
	FrameTexture();
	~FrameTexture();

public:
	uint8_t *buf;
	int bufSize;
	int width;
	int height;
	GLenum format;
	int nComponents;
	bool isNonNativeEndianess;

};

class AudioFromTexture
{
public:
	AudioFromTexture(int _nChannels=1, int _rate=48000, int _nSamples=0);
	~AudioFromTexture();

public:
	float **buf;
	int nChannels;
	int samplingRate;
	int nSamples;
};

class VideoEncoder
{
public:
	VideoEncoder(const char *filename);
	void Open();
	void Close();

	void ExampleVideoFrame(FrameTexture *frame);
	void ExampleAudioFrame(const AudioFromTexture *example);

	void WriteVideoFrame(const FrameTexture *frame);
	void WriteAudioFrame(const AudioFromTexture *audio);

private:
	AVFormatContext *outFmt;

	                               // corresponding variable in muxing.c
	AVCodec *audioCodec;           // audio_codec
	AVCodec *videoCodec;           // video_codec

	AVStream *audioStream;         // audio_st->st
	AVStream *videoStream;         // video_st->st

	AVFrame *audioFrameIn;         // audio_st->tmp_frame
	AVFrame *audioFrameOut;        // audio_st->frame
	SwrContext *audioResampleCtx;  // audio_st->swr_ctx
	int64_t audioNumSamplesIn;     // audio_st->next_pts
	int64_t audioNumSamplesOut;    // audio_st->samples_count

	AVFrame *videoFrameIn;         // video_st->tmp_frame
	AVFrame *videoFrameOut;        // video_st->frame
	SwsContext *videoRescaleCtx;   // video_st->sws_ctx
	int64_t videoNumFrames;        // video_st->samples_count
};

#endif // VIDEOENCODER_H
