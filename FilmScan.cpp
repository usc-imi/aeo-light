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

// FilmScan -- objects for handling scanned images of film.
//
// FilmFrame - a scan of a single frame. Pixel values are mapped to [0-1], with
//             zero being black and one being white.
// FilmStrip - a sequence of FilmFrames (usually short; not the entire film)
// FilmScan - the main interface to a scanned film source (not used currently,
//            see project.h instead for an object that holds the working
//            project).
// SoundSignal - a sequence of values (doubles) for the sound extracted from
//               the film scan
//               each signal value is computed as the mean average of the
//               gray level of the pixels across the sound bounds area at
//               each scan row.
//

#define FILESCAN_CPP

#include <iostream>
#include <dirent.h>
#include <errno.h>
#include <ctype.h>
#include <cmath>
#include <cstring>
#include <vector>

//#include <boost/numeric/ublas/matrix.hpp>

#include <QString>
#include <QMessageBox>

#ifdef _WIN32
// Depending on the way you've built libtiff on Windows, you may have
// to adjust the way tiffio.h declares the libtiff functions.
// If you're getting errors about symbols starting with __imp_TIFF, like
//
// FilmScan.obj : error LNK2019: unresolved external symbol __imp_TIFFClose
//
// then define BUILD_LIBTIFF_DLL (which will make the linker look for
// the __exp_TIFFClose version instead)
//
// If you're getting other link errors about unresolved symbols related to
// libtiff, try defining one of these other constants:
// USE_LIBTIFF_DLL (look for __imp_TIFF)
// USE_LIBTIFF_STATIC (look for _TIFF)
//
  #define BUILD_LIBTIFF_DLL
#endif

#include <tiffio.h>

#if (defined __WIN32__) || (defined _WIN32)
# ifdef BUILD_LIBTIFF_DLL
#  define LIBTIFF_DLL_IMPEXP     __DLL_EXPORT__
# elif defined(LIBTIFF_STATIC)
#  define LIBTIFF_DLL_IMPEXP
# elif defined (USE_LIBTIFF_DLL)
#  define LIBTIFF_DLL_IMPEXP     __DLL_IMPORT__
# elif defined (USE_LIBTIFF_STATIC)
#  define LIBTIFF_DLL_IMPEXP
# else /* assume USE_LIBTIFF_DLL */
#  define LIBTIFF_DLL_IMPEXP     __DLL_IMPORT__
# endif
#else /* __WIN32__ */
# define LIBTIFF_DLL_IMPEXP
#endif

#include "DPX.h"
#include "readframedpx.h"
#include "readframeexr.h"
#include "readframetiff.h"
#include "wav.h"
#include "aeoexception.h"

#include "FilmScan.h"

#ifdef _WIN32
#define isslash(c) (((c)=='/')||((c)=='\\'))
#else
#define isslash(c) ((c)=='/')
#endif

#ifdef USELIBAV
//-----------------------------------------------------------------------------
Video::~Video()
{
	if(frameNative) av_frame_free(&frameNative);
	if(frameGray16)
	{
		av_frame_free(&frameGray16);
	}
	if(frameRGB)
	{
		av_frame_free(&frameRGB);
	}
	if(convertGray16) sws_freeContext(convertGray16);
	if(convertRGB) sws_freeContext(convertRGB);
	if(codec) avcodec_close(codec);
	if(format) avformat_close_input(&format);
}

//----------------------------------------------------------------------------
bool Video::ReadNextFrame(size_t currfnum)
{
	AVPacket packet;
	int done;
	int frame_id;

	//if(this->frameNative == NULL) this->frameNative = av_frame_alloc();

	while(av_read_frame(this->format, &packet)>=0)
	{
		// Is this a packet from the right stream?
		if(packet.stream_index == this->streamIdx)
		{
			// Decode video frame
			avcodec_decode_video2(this->codec, this->frameNative, &done,
					&packet);

			// Did we get a complete video frame?
			if(done)
			{
				this->dts = packet.dts;
				this->curFrame = currfnum;
				// this->curFrame = (this->dts - this->dtsBase)/this->dtsStep + 1;

				av_packet_unref(&packet);
				return true;
			}
		}
		av_packet_unref(&packet);
	}
	this->dts = 0;
	this->curFrame = 0;

	return false;
}

bool Video::ReadFrame(size_t frameNum = 0)
{

	//if(frameNum == 0) return ReadNextFrame();

	// asking for the current frame again?
	if(this->curFrame == frameNum) return true;

	// asking for the next frame explicitly?
	if(this->curFrame+1 == frameNum) return ReadNextFrame(frameNum);

	size_t target = this->dtsBase + (frameNum)*this->dtsStep;

	int seekflags = AVSEEK_FLAG_ANY;

	if(target < this->dts) seekflags |= AVSEEK_FLAG_BACKWARD;

	av_seek_frame(this->format, this->streamIdx, target, seekflags);

	// do
	// {
	//    if(!ReadNextFrame()) return false;
	// } while(this->dts < target);
	if(!ReadNextFrame(frameNum)) return false;

	return true;
}

double *Video::GetFrame(size_t frameNum, double *buf=NULL)
{

	if(buf == NULL)
	{
		buf = new double [this->codec->width * this->codec->height];
		if(buf == NULL)
		{
			throw AeoException("Out of Memory: DPX buf");
		}
	}

	if(this->ReadFrame(frameNum) == false)
	{
		throw AeoException(
				QString("Could not read requested frame number: %1").
					arg(frameNum));
	}

	//if(this->frameGray16 == NULL) this->frameGray16 = av_frame_alloc();

	// Convert the image from its native format to Gray16
	sws_scale(this->convertGray16,
			(uint8_t const * const *)this->frameNative->data,
			this->frameNative->linesize, 0, this->codec->height,
			this->frameGray16->data, this->frameGray16->linesize);

	// translate Gray16 to double:
	const uint16_t *buf16;
	double scale = 1.0/(0x00FFFF);
	long i(0);
	for(int y=0; y<this->codec->height; ++y)
	{
		buf16 = (uint16_t *)
				(this->frameGray16->data[0] + y*this->frameGray16->linesize[0]);

		for(int x=0; x<this->codec->width; ++x)
			buf[i++] = double(buf16[x]) * scale;
	}

	return buf;
}


unsigned char *Video::GetFrameImage(size_t frameNum, unsigned char *buf,
		int &width,int &height,bool &endian)
{
	if(buf == NULL)
	{
		buf = new unsigned char [this->codec->width * this->codec->height * 8];
		if(buf == NULL)
		{
			throw AeoException("Out of Memory: video buf");
		}
	}

	if(this->ReadFrame(frameNum) == false)
	{
		throw AeoException(
				QString("Could not read requested frame number: %1").
					arg(frameNum));
	}

	// Convert the image from its native format to RGB
	sws_scale(this->convertRGB,
			(uint8_t const * const *)this->frameNative->data,
			this->frameNative->linesize, 0, this->codec->height,
			this->frameRGB->data, this->frameRGB->linesize);

	width = this->codec->width;
	height = this->codec->height;
	endian = false;
	memcpy(buf,this->frameRGB->data[0],width*height*8);

	return buf;
}
#endif

//-----------------------------------------------------------------------------
FilmFrame::FilmFrame(unsigned int r, unsigned int c)
{
	rows = r;
	cols = c;
	buf.resize(r*c);
}

//-----------------------------------------------------------------------------
FilmFrame::~FilmFrame()
{
}

//-----------------------------------------------------------------------------

const char *SourceFormatName[] {
	"DPX",
	"OpenEXR",
	"TIFF",
	"Other image",
	"LibAV Video",
	"WAV",
	"Unknown" };

const char *FilmScan::GetFormatStr() const
{
	return SourceFormatName[this->srcFormat];
}

SourceFormat FilmScan::StrToSourceFormat(const char *str)
{
	SourceFormat i;
	for(i=0; i<SOURCE_UNKNOWN; ++i)
	{
		if(strcmp(SourceFormatName[i], str) == 0) break;
	}
	return i;
}

//-----------------------------------------------------------------------------
bool FilmScan::Source(const std::string filename, SourceFormat fmt)
{
	this->Reset();
	this->inputName = filename;

	if(fmt != SOURCE_UNKNOWN)
	{
		this->srcFormat = fmt;
		switch(fmt)
		{
		case SOURCE_DPX:
			if(SourceDPX(filename)) return true;
			break;
		case SOURCE_TIFF:
			if(SourceTIFF(filename)) return true;
			break;
		case SOURCE_LIBAV:
			if(SourceLibAV(filename)) return true;
			break;
		case SOURCE_WAV:
			if(SourceWav(filename)) return true;
			break;
		default:
			throw AeoException("Unrecognized FileType Request");
		}
	}
	else
	{
		try
		{
			if(SourceDPX(filename)) return true;
		}
		catch(...) {;}

		try
		{
			if(SourceWav(filename)) return true;
		}
		catch(...) {;}

		try
		{
			if(SourceLibAV(filename)) return true;
		}
		catch(...) { throw; }
	}

	throw AeoException("Unable to recognize source file");
}

//-----------------------------------------------------------------------------
void FilmScan::Reset()
{
	if(name) { delete [] name; name = NULL; }
	if(path) { delete [] path; path = NULL; }
	if(fnbuf) { delete [] fnbuf; fnbuf = NULL; }
	srcFormat = SOURCE_UNKNOWN;
	firstFrame = 0;
	numFrames = 0;
	width = 0;
	height = 0;
	#ifdef USELIBAV
	if(vid) { delete vid; vid = NULL; }
	#endif
	if(synth) {delete synth; synth = NULL; }

	inputName.clear();
}

//-----------------------------------------------------------------------------
bool FilmScan::SourceDPX(const std::string filename)
{
	// verify the file is a DPX and get the width and height from the header
	InStream img;
	dpx::Reader dpx;

	if(!img.Open(filename.c_str()))
	{
		QString msg;
		int err;

		msg += QString("FilmScan: Cannot open ");
		msg += QString(filename.c_str());
		msg += QString("\n");
		if((err=errno)!=0) msg += QString(strerror(err));
		throw AeoException(msg);
	}

	dpx.SetInStream(&img);
	if(!dpx.ReadHeader())
	{
		QString msg;
		int err;

		msg += QString("FilmScan: Invalid DPX header in ");
		msg += QString(filename.c_str());
		img.Close();
		throw AeoException(msg);
	}

	this->width = dpx.header.Width();
	this->height = dpx.header.Height();
	this->srcFormat = SOURCE_DPX;
	img.Close();

	SourceIdentifyImageSet(filename);

	// Pull the TimeCode from the first DPX in the sequence:
	sprintf(this->fnbuf+strlen(this->path)+1, this->name, this->FirstFrame());
	if(!img.Open(this->fnbuf))
	{
		QString msg;
		int err;

		msg += QString("FilmScan: Cannot open ");
		msg += QString(this->fnbuf);
		msg += QString("\n");
		if((err=errno)!=0) msg += QString(strerror(err));
		throw AeoException(msg);
	}

	dpx::Reader firstDPX;
	firstDPX.SetInStream(&img);
	if(!firstDPX.ReadHeader())
	{
		QString msg;
		int err;

		msg += QString("FilmScan: Invalid DPX header in ");
		msg += QString(this->fnbuf);
		img.Close();
		throw AeoException(msg);
	}

	char TC[16];
	firstDPX.header.TimeCode(TC);
	this->TimeCode = TC;
	img.Close();

	return true;
}

//-----------------------------------------------------------------------------
bool FilmScan::SourceEXR(const std::string filename)
{
#ifdef USE_OPENEXR
	// Open as an EXR and get the width and height
	// The Imf library will throw an exception if the file is not an EXR
	Imf::RgbaInputFile exr(filename.c_str());
	Imath::Box2i dw = exr.dataWindow();
	this->width = dw.max.x - dw.min.x + 1;
	this->height = dw.max.y - dw.min.y + 1;
	this->fmt = SOURCE_EXR;

	SourceIdentifyImageSet(filename);

	return true;
#else
	return false;
#endif
}

//-----------------------------------------------------------------------------
bool FilmScan::SourceTIFF(const std::string filename)
{
	// verify the file is a TIFF and get the width and height from the header
	InStream img;

	TIFF *tif;

	if((tif = TIFFOpen(filename.c_str(), "r")) == NULL)
	{
		QString msg;
		int err;

		msg += QString("FilmScan: Cannot open ");
		msg += QString(filename.c_str());
		msg += QString("\n");
		if((err=errno)!=0) msg += QString(strerror(err));
		throw AeoException(msg);
	}

	// just grab the width and height from the header
	uint32 w, h;
	if(TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w) != 1 ||
			TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h) != 1)
	{
		TIFFClose(tif);
		throw AeoException("Invalid image size specification in TIFF.");
	}

	TIFFClose(tif);

	this->width = w;
	this->height = h;
	this->srcFormat = SOURCE_TIFF;

	this->TimeCode = "00:00:00:00";
	SourceIdentifyImageSet(filename);

	return true;
}

//-----------------------------------------------------------------------------
bool FilmScan::SourceIdentifyImageSet(const std::string filename)
{
	// Then find all the dpx files that go with this one by adjusting
	// the number string at the end of the filename (before the extension)
	// to find all the adjacent frames both before and after this one.
	// break apart the filename to make a filename pattern for the rest
	// of the frames.

	int len, lenE, lenF, lenP;
	const char *cp, *ext;
	len = filename.length();

	// allocate a buffer for temporary filenames later
	this->fnbuf = new char [len+1];
	strcpy(this->fnbuf, filename.c_str());

	// find the extension (or leave it blank if none)
	ext = "";
	lenE = 1;
	for(cp = filename.c_str()+len;
			!isslash(*cp) && cp>filename.c_str();
			--cp, lenE++)
	{
		if(*cp == '.')
		{
			ext = cp;
			break;
		}
	}

	// Count digits from the end (before the extension, if any)
	if(*ext=='.')
		cp = ext;
	else
	{
		cp = filename.c_str()+len;
		lenE = 0;
	}

	int numdigits;
	for(numdigits = 0; cp>=filename.c_str() && isdigit(*(--cp)); numdigits++) ;

	if(numdigits==0)
	{
		throw AeoException("DPX filename contains no digits for frame number");
	}

	long inputFrame;
	sscanf(cp+1, "%ld", &inputFrame);

	// Continue back to the path portion.

	lenF = 0;
	for( ; cp>=filename.c_str() && !isslash(*cp); --cp, lenF++) ;

	// store the filename pattern (with extension, but without the path)
	this->name = new char[
		lenF                             // filename prefix
		+2                               // "%0"
		+int(floor(log10(numdigits)))+1  // number of digits in <numdigits>
		+2                               // "ld"
		+lenE                            // extension (including dot)
		+1                               // null terminator
		];

	sprintf(this->name, "%.*s%%0%dld%.*s",lenF,cp+1,numdigits,lenE,ext);

	// store the path (without the trailing slash)
	if(!isslash(*cp))
	{
		this->path = new char[2];
		strcpy(this->path,".");
	}
	else
	{
		int pl = cp - filename.c_str();

		this->path = new char[pl+1];

		strncpy(this->path, filename.c_str(), pl);
		this->path[pl]='\0';

		// adjust slashes on windows to avoid complications later
		#ifdef _WIN32
		for(char *cpw=this->path; *cpw; cpw++)
			if(*cpw == '\\') *cpw = '/';
		#endif
	}

	DIR *dirp;

	if((dirp = opendir(this->path)) == NULL)
	{
		QString msg;
		int err;

		msg += QString("FilmScan: Cannot read directory folder ");
		msg += QString(this->path);
		msg += QString("\n");
		if((err=errno)!=0) msg += QString(strerror(err));
		throw AeoException(msg);
	}

	// find all files that match the pattern and record their frame numbers
	std::vector<long> frames;
	long curFrame(0);
	frames.reserve(1000);
	while(1)
	{
		errno = 0;
		struct dirent *dp;
		if((dp = readdir(dirp)) != NULL)
		{
			// is it a file?
			#ifdef _DIRENT_HAVE_D_TYPE
			if(!(dp->d_type == DT_REG || dp->d_type == DT_UNKNOWN)) continue;
			#endif

			// leading string of characters matches?
			if(strncmp(dp->d_name, this->name, lenF)!=0) continue;

			// ... followed by correct number of digits?
			for(cp = dp->d_name+lenF; isdigit(*cp); cp++) ;
			if(cp - dp->d_name != lenF+numdigits) continue;

			// ... followed by extension?
			if(strcmp(cp, ext) != 0) continue;

			// Then save this frame number in the list (vector) of frames
			sscanf(dp->d_name+lenF, "%ld", &curFrame);
			if(frames.capacity() == frames.size())
				frames.reserve(frames.capacity()+1000);
			frames.push_back(curFrame);
		}
		else
			break;
	}
	if(errno!=0)
	{
		QString msg;
		int err;

		msg += QString("FilmScan: Error reading directory folder ");
		msg += QString(this->path);
		msg += QString("\n");
		if((err=errno)!=0) msg += QString(strerror(err));
		throw AeoException(msg);
	}


	// find the frame given in the input argument filename
	std::vector<long>::iterator framei;
	std::sort(frames.begin(), frames.end());
	framei = std::lower_bound(frames.begin(), frames.end(), inputFrame);

	// seek in both directions to find the largest contiguous set of
	// frames containing the given frame.
	long firstIdx(framei - frames.begin());
	while(firstIdx > 0 && frames[firstIdx-1] == frames[firstIdx] - 1)
		firstIdx--;
	this->firstFrame = frames[firstIdx];

	long lastIdx(framei - frames.begin());
	while(lastIdx < frames.size() && frames[lastIdx+1] == frames[lastIdx] + 1)
		lastIdx++;
	this->numFrames = frames[lastIdx] - this->firstFrame + 1;

	return true;
}


//-----------------------------------------------------------------------------
bool FilmScan::SourceWav(const std::string filename)
{
	this->synth = new wav;
	if(!synth->read(filename.c_str()))
		return false;

	// set 10% overlap
	this->height = (unsigned int)((double(synth->samplesPerSec) / 24.0) * 1.1);
	this->width = this->height;

	this->firstFrame = 1;

	// round down to whole seconds (ignoring frames in the last partial second)
	this->numFrames = (synth->bufSize / synth->samplesPerSec) * 24;

	// and reduce by one more, for good measure, in case the last frame
	// completed the time second (to ensure the 10% overlap has something to
	// overlap into).
	this->numFrames--;

	int len = filename.length();
	const char *cp = filename.c_str() + len;
	while(cp>filename.c_str() && *cp != '/' && *cp != '\\') cp--;
	this->name = new char[strlen(cp)+1];
	strcpy(this->name, cp);
	this->path = new char[cp - filename.c_str() + 1];
	strncpy(this->path, filename.c_str(), cp-filename.c_str());
	this->TimeCode = "00:00:00:00";
	this->srcFormat = SOURCE_WAV;

	return true;
}

//-----------------------------------------------------------------------------

bool FilmScan::SourceLibAV(const std::string filename)
{
	#ifdef USELIBAV
	{
	// accept any recognized codec
	av_register_all();

	vid = new Video;

	AVDictionary *dict = NULL;

	// Open video file
	if(avformat_open_input(&vid->format, filename.c_str(), NULL, &dict)<0)
	{
		av_dict_free(&dict);
		delete vid;
		vid = NULL;
		return false; // Couldn't open file
	}

	av_dict_free(&dict);
	dict = NULL;

	// Retrieve stream information
	if(avformat_find_stream_info(vid->format, NULL) < 0)
	{
		avformat_close_input(&(vid->format));
		delete vid;
		vid = NULL;
		return false; // Couldn't find stream information
	}

	// Dump information about file onto standard error
	//av_dump_format(vid->format, 0, filename.c_str(), 0);
	char TC [20];
	AVDictionaryEntry * DE=NULL;
	// Find the first video stream
	vid->streamIdx = -1;
	for(int i=0; i<vid->format->nb_streams; ++i)
	{
		if(vid->format->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO)
		{
			vid->streamIdx=i;
			DE = av_dict_get(vid->format->streams[i]->metadata,"timecode",NULL ,0);

			if(DE)
				this->TimeCode = DE->value;
			else
				this->TimeCode = "00:00:00:00" ;

			break;
		}
	}

	if(vid->streamIdx == -1)
	{
		avformat_close_input(&(vid->format));
		delete vid;
		vid = NULL;
		return false; // Couldn't find any video stream
	}

	int ret = QMessageBox::question(NULL,
		"Buffer Input?",
		"Do you want to buffer the incoming video?",
		QMessageBox::Yes | QMessageBox::No,
		QMessageBox::Yes);

	if(ret == QMessageBox::No)
	{
		av_dict_set(&dict, "fflags", "nobuffer",0);
		avformat_close_input(&(vid->format));
		delete vid;
		vid = new Video;

		if(avformat_open_input(&vid->format, filename.c_str(), NULL, &dict)<0)
		{
			av_dict_free(&dict);
			delete vid;
			vid = NULL;
			return false; // Couldn't open file
		}

		if(avformat_find_stream_info(vid->format, NULL) < 0)
		{
			avformat_close_input(&(vid->format));
			delete vid;
			vid = NULL;
			return false; // Couldn't find stream information
		}
		// Find the first video stream
		vid->streamIdx = -1;
		for(int i=0; i<vid->format->nb_streams; ++i)
		{
			if(vid->format->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO)
			{
				vid->streamIdx=i;
				break;
			}
		}

		if(vid->streamIdx == -1)
		{
			avformat_close_input(&(vid->format));
			delete vid;
			vid = NULL;
			return false; // Couldn't find any video stream
		}
	}

	// ask the stream how many frames it has
	if((this->numFrames=vid->format->streams[vid->streamIdx]->nb_frames) == 0)
	{
		// TODO: if it doesn't know, guess based on duration and then validate.
		//
		this->numFrames =
				(int)(( vid->format->streams[vid->streamIdx]->duration /
					(double)AV_TIME_BASE ) *
					vid->format->streams[vid->streamIdx]->codec->time_base.den +
					0.5);

		/* FAILED ATTEMPT TO USE LIBAV TO READ IMAGES
		if(this->numFrames < 0)
		{
			SourceIdentifyImageSet(filename);
		}
		*/

		if(this->numFrames <= 0)
		{
			avformat_close_input(&(vid->format));
			delete vid;
			vid = NULL;
			throw AeoException("Could not determine number of frames in video");
		}
	}

	this->firstFrame = 0;

	AVCodecContext *codecOrig;
	AVCodec *decoder;

	// Get a pointer to the codec context for the video stream
	codecOrig=vid->format->streams[vid->streamIdx]->codec;

	// Find the decoder for the video stream
	decoder=avcodec_find_decoder(codecOrig->codec_id);
	if(decoder==NULL)
	{
		avformat_close_input(&(vid->format));
		delete vid;
		vid = NULL;
		throw AeoException("Unsupported codec.");
	}

	// Copy codec context
	vid->codec = avcodec_alloc_context3(decoder);
	if(avcodec_copy_context(vid->codec, codecOrig) != 0)
	{
		avformat_close_input(&(vid->format));
		delete vid;
		vid = NULL;
		throw AeoException("Couldn't copy codec context");
	}
	avcodec_close(codecOrig);

	// Open codec
	if(avcodec_open2(vid->codec, decoder, NULL)<0)
	{
		avformat_close_input(&(vid->format));
		delete vid;
		vid = NULL;
		throw AeoException("Couldn't open decoder context");
	}

	// Allocate video frame
	vid->frameNative=av_frame_alloc();
	if(vid->frameNative==NULL)
	{
		avformat_close_input(&(vid->format));
		delete vid;
		vid = NULL;
		throw AeoException("Couldn't allocate frame structure");
	}

	// Allocate an RGB frame
	vid->frameRGB=av_frame_alloc();
	if(vid->frameRGB==NULL)
	{
		avformat_close_input(&(vid->format));
		delete vid;
		vid = NULL;
		throw AeoException ("Couldn't allocate frame structure");
	}

	this->width = vid->codec->width;
	this->height = vid->codec->height;

	// allocate RGB buffer (to be freed when vid is destroyed)
	uint8_t *buffer = (uint8_t *)av_malloc(
			av_image_get_buffer_size(
					AV_PIX_FMT_RGBA64LE,
					vid->codec->width, vid->codec->height,1));

	// Assign appropriate parts of buffer to image planes in frameRGB
	// Note that frameRGB is an AVFrame, but AVFrame is a superset
	// of AVPicture
	//avpicture_fill((AVPicture *)vid->frameRGB, buffer, AV_PIX_FMT_RGBA64LE,
	//		vid->codec->width, vid->codec->height);

	av_image_fill_arrays(vid->frameRGB->data, vid->frameRGB->linesize, buffer,
			AV_PIX_FMT_RGBA64LE, vid->codec->width, vid->codec->height, 1);

	// Allocate a Gray16 frame
	vid->frameGray16=av_frame_alloc();
	if(vid->frameGray16==NULL)
	{
		avformat_close_input(&(vid->format));
		delete vid;
		vid = NULL;
		throw AeoException("Couldn't allocate frame structure");
	}
	// allocate Gray16 buffer (to be freed when vid is destroyed)
	buffer = (uint8_t *)av_malloc(
			av_image_get_buffer_size(
					AV_PIX_FMT_GRAY16, vid->codec->width, vid->codec->height, 1
					));

	// Assign appropriate parts of buffer to image planes in frameGray16
	//avpicture_fill((AVPicture *)vid->frameGray16, buffer, AV_PIX_FMT_GRAY16,
	//		vid->codec->width, vid->codec->height);
	av_image_fill_arrays(vid->frameGray16->data, vid->frameGray16->linesize,
			buffer, AV_PIX_FMT_GRAY16,
			vid->codec->width, vid->codec->height, 1);

	// initialize SWS context for conversion to RGB
	vid->convertRGB = sws_getContext(
			vid->codec->width,
			vid->codec->height,
			vid->codec->pix_fmt,
			vid->codec->width,
			vid->codec->height,
			AV_PIX_FMT_RGBA64LE, //rgba
			SWS_BILINEAR,
			NULL,
			NULL,
			NULL
			);

	// initialize SWS context for conversion to Gray16
	vid->convertGray16 = sws_getContext(
			vid->codec->width,
			vid->codec->height,
			vid->codec->pix_fmt,
			vid->codec->width,
			vid->codec->height,
			AV_PIX_FMT_GRAY16,
			SWS_BILINEAR,
			NULL,
			NULL,
			NULL
			);


	// Read two frames to calibrate seek-to-frame parameters
	vid->ReadNextFrame(0);
	vid->dtsBase = vid->dts;
	vid->ReadNextFrame(1);
	vid->dtsStep = vid->dts;

	// reset to first frame
	vid->ReadFrame(0);

	int len = filename.length();
	const char *cp = filename.c_str() + len;
	while(cp>filename.c_str() && *cp != '/' && *cp != '\\') cp--;
	this->name = new char[strlen(cp)+1];
	strcpy(this->name, cp);
	this->path = new char[cp - filename.c_str() + 1];
	strncpy(this->path, filename.c_str(), cp-filename.c_str());

	this->srcFormat = SOURCE_LIBAV;

	return true;
	}
	#else
	{
	return false;
	}
	#endif
}

//-----------------------------------------------------------------------------
FilmScan::~FilmScan()
{
	if(this->name) delete [] this->name;
	if(this->path) delete [] this->path;
	if(this->fnbuf) delete [] this->fnbuf;
	#ifdef USELIBAV
	if(this->vid) delete this->vid;
	#endif
}


//-----------------------------------------------------------------------------

double* FilmScan::GetFrame(long frameNum, double *buf) const
{
	if(frameNum < this->FirstFrame() || frameNum > this->LastFrame())
	{
		throw AeoException(QString("Frame out of range: %1").arg(frameNum));
	}

	switch(this->srcFormat)
	{
	case SOURCE_DPX:
		sprintf(this->fnbuf+strlen(this->path)+1, this->name, frameNum);
		buf = ReadFrameDPX(fnbuf, buf);
		break;
	case SOURCE_TIFF:
		sprintf(this->fnbuf+strlen(this->path)+1, this->name, frameNum);
		buf = ReadFrameTIFF(fnbuf, buf);
		break;
	case SOURCE_LIBAV:
		if(this->vid) buf = this->vid->GetFrame(frameNum, buf);
		else throw AeoException("Internal video structure not ready");
		break;
	case SOURCE_WAV:
		if(this->synth) buf = this->synth->GetFrame(frameNum, buf);
		else throw AeoException("Internal wav synth structure not ready");
	default:
		throw AeoException("Internal scan format not set correctly");
	}

#if 0
	#ifdef USELIBAV
	if(this->vid)
	{
		buf = this->vid->GetFrame(frameNum, buf);
	}
	else
	#endif
	if(this->synth)
	{
		buf = this->synth->GetFrame(frameNum, buf);
	}
	else
	{
		sprintf(this->fnbuf+strlen(this->path)+1, this->name, frameNum);
		buf = ReadFrameDPX(fnbuf, buf);
	}

#endif

	return buf;
}




//-----------------------------------------------------------------------------

FrameTexture* FilmScan::GetFrameImage(long frameNum, FrameTexture *frame) const
{
	if(frameNum < this->FirstFrame() || frameNum > this->LastFrame())
	{
		throw AeoException(QString("Frame out of range: %1").arg(frameNum));
	}

	if(!frame) frame = new FrameTexture;

	switch(this->srcFormat)
	{
	case SOURCE_DPX:
		sprintf(this->fnbuf+strlen(this->path)+1, this->name, frameNum);
		frame->buf = ReadFrameDPX_ImageData(fnbuf, frame->buf, frame->bufSize,
				frame->width, frame->height, frame->isNonNativeEndianess,
				frame->format, frame->nComponents);
		break;
	case SOURCE_TIFF:
		sprintf(this->fnbuf+strlen(this->path)+1, this->name, frameNum);
		frame->buf = ReadFrameTIFF_ImageData(fnbuf, frame->buf,
				frame->width, frame->height, frame->isNonNativeEndianess,
				frame->format, frame->nComponents);
		break;
	case SOURCE_LIBAV:
		if(this->vid)
		{
			frame->buf = this->vid->GetFrameImage(frameNum, frame->buf,
					frame->width, frame->height, frame->isNonNativeEndianess);
			frame->nComponents = 4;
			frame->format = GL_UNSIGNED_SHORT;
		}
		else throw AeoException("Internal video structure not ready");
		break;
	case SOURCE_WAV:
		if(this->synth)
		{
			frame->buf = this->synth->GetFrameImage(frameNum, frame->buf,
					frame->width, frame->height, frame->isNonNativeEndianess);
			frame->nComponents = 4;
			frame->format = GL_UNSIGNED_SHORT;
		}
		else throw AeoException("Internal wav synth structure not ready");
	default:
		throw AeoException("Internal scan format not set correctly");
	}

	return frame;
}

//-----------------------------------------------------------------------------

FilmStrip FilmScan::GetFrameRange(long frameRange[2]) const
{
	if(frameRange[0] < this->FirstFrame() ||
			frameRange[1] < frameRange[0] ||
			frameRange[1] > this->LastFrame())
	{
		throw AeoException(
				QString("Frames out of range: %1 - %2").
					arg(frameRange[0]).arg(frameRange[1]));
	}

	long nFrames, i;
	unsigned int r, c;
	FilmStrip frames;

	nFrames = frameRange[1] - frameRange[0] + 1;
	r = this->Height();
	c = this->Width();

	frames.resize(nFrames, FilmFrame(r,c));

	for(i = 0; i<nFrames; i++)
	{
		(void)GetFrame(frameRange[0] + i, frames[i]);
	}

	return frames;
}


//-----------------------------------------------------------------------------

double* FilmScan::GetSoundSignal(long frameNum,
		unsigned int col0, unsigned int col1) const
{
	double *frame = this->GetFrame(frameNum, NULL);
	double *sound = new double[this->height];
	double *row;

	int r,c;

	// get the average value over the column range for each row
	for(r=0; r<this->height; r++)
	{
		sound[r] = 0;
		row = frame + r*this->width + col0;
		for(c=col0; c<=col1; c++)
			sound[r] += *(row++);
		sound[r] /= (col1-col0+1);
	}

	return sound;
}


//-----------------------------------------------------------------------------

SoundSignal FilmScan::GetSoundLocal(FilmFrame frame,
		unsigned int col0, unsigned int col1) const
{
	SoundSignal sound(this->height);

	unsigned int r,c;

	// get the average value over the column range for each row
	for(r=0; r<this->height; r++)
	{
		sound[r] = 0;
		for(c=col0; c<=col1; c++)
			sound[r] += double(frame[r][c]);
		sound[r] /= (col1-col0+1);
	}

	return sound;
}

