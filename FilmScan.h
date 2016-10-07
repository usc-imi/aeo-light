//-----------------------------------------------------------------------------
// This file is part of AEO-Light
//
// Copyright (c) 2016 University of South Carolina
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

// FilmScan -- objects for handling scanned images of film.
//
// FilmFrame - a scan of a single frame
// FilmStrip - a sequence of FilmFrames (usually short; not the entire film)
// FilmScan - the main interface to a scanned film source (not used currently,
//            see project.h instead for an object that holds the working
//            project).
// SoundSignal - a sequence of values (doubles) for the sound extracted from
//               the film scan
//
//
#ifndef FILMSCAN_H
#define FILMSCAN_H

#define USELIBAV

#ifdef USELIBAV
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/timecode.h>
}
#endif

#include <vector>
#include "wav.h"

#include <QOpenGLTexture>

#ifdef USE_OPENEXR
#include <OpenEXR/ImfRgbaFile.h>
#endif

class FilmFrame {
private:
	std::vector <double> buf;
	unsigned int rows, cols;
public:
	FilmFrame() : buf(), rows(0), cols(0) {} ;
	FilmFrame(unsigned int r, unsigned int c);
	~FilmFrame();
	// data() is only available in C++11, so use the old &(p[0]) construction
	//double *operator[](unsigned int r) { return buf.data()+(r*cols); } ;
	//operator double*() { return buf.data(); }
	double *operator[](unsigned int r) { return &(buf[r*cols]); } ;
	operator double*() { return &(buf[0]); }
	unsigned int Width() const { return cols; } ;
	unsigned int Height() const { return rows; } ;
	unsigned int Cols() const { return cols; } ;
	unsigned int Rows() const { return rows; } ;

};

typedef std::vector< FilmFrame > FilmStrip;

typedef std::vector< double > SoundSignal;

#ifdef USELIBAV
class Video {

public:
	AVFormatContext *format;
	AVCodecContext *codec;
	int streamIdx;
	SwsContext *convertRGB;
	SwsContext *convertGray16;
	AVFrame *frameNative;
	AVFrame *frameRGB;
	AVFrame *frameGray16;
	size_t curFrame;
	size_t dts;
	size_t dtsBase; // DTS of first frame
	size_t dtsStep; // DTS between frames

	Video()
		: format(NULL), codec(NULL), streamIdx(0),
		convertRGB(NULL), convertGray16(NULL),
		frameNative(NULL), frameRGB(NULL), frameGray16(NULL),
		curFrame(0), dts(0), dtsBase(0), dtsStep(1) {};
	~Video();

public:
	bool ReadNextFrame(void);
	bool ReadFrame(size_t frameNum);

	double *GetFrame(size_t frameNum, double *buf);

	unsigned char *GetFrameImage(size_t frameNum, unsigned char *buf,
			int &width,int &height,bool &endian);

};
#endif

typedef enum _SourceFormat {
	SOURCE_DPX,
	SOURCE_EXR,
	SOURCE_TIFF,
	SOURCE_OTHER_IMG,
	SOURCE_LIBAV,
	SOURCE_WAV,
	SOURCE_UNKNOWN
} SourceFormat;

class FilmScan {
private:
	char *name;
	char *path;
	char *fnbuf;
	SourceFormat srcFormat;
	long firstFrame;
	long numFrames;
	unsigned int width;
	unsigned int height;

#ifdef USELIBAV
	Video *vid = NULL;
#endif
	wav *synth = NULL;

	std::string inputName;

public:
	QString TimeCode;

public:
#ifdef USELIBAV
	FilmScan()
		: name(NULL), path(NULL), fnbuf(NULL),
		firstFrame(0), numFrames(0), width(0), height(0), vid(NULL)	{} ;
#else
	FilmScan()
		: name(NULL), path(NULL), fnbuf(NULL),
		firstFrame(0), numFrames(0), width(0), height(0) {} ;
#endif
	FilmScan(const std::string filename) { Source(filename); };
	~FilmScan();

	void Reset();
	bool Source(const std::string filename, SourceFormat fmt=SOURCE_UNKNOWN);
	bool SourceDPX(const std::string filename);
	bool SourceEXR(const std::string filename);
	bool SourceTIFF(const std::string filename);
	bool SourceLibAV(const std::string filename);
	bool SourceWav(const std::string filename);
	bool SourceIdentifyImageSet(const std::string filename);

	bool IsReady() const { return (numFrames>0); };
	long NumFrames() const { return numFrames; };
	long FirstFrame() const { return firstFrame; };
	long LastFrame() const { return firstFrame + numFrames - 1; };
	unsigned int Width() const { return width; };
	unsigned int Height() const { return height; };
	std::string GetFileName() const { return inputName; }
	const char *GetPath() const { return path; }
	const char *GetBaseName() const { return name; }

	double *GetFrame(long frameNum, double *buf) const;
    unsigned char *GetFrameImage(long frameNum, unsigned char *buf,
            int &width, int &height, GLenum &pix_fmt, int &num_components, bool &endian) const;
	FilmFrame GetFrame(long frameNum) const;
	FilmStrip GetFrameRange(long frameRange[2]) const;

	double *GetSoundSignal(long frameNum, unsigned int l,
		unsigned int r) const;

	SoundSignal GetSoundLocal(FilmFrame frame, unsigned int l,
		unsigned int r) const;

	bool IsSynth(void) const { return (synth != NULL); };
	int SynthOverlap(void) const { return (synth? synth->GetOverlap() : 0); };
};

#endif
