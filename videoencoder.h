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
