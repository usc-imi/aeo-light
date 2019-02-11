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
#include "wav.h"
#include <vector>
#include <cstring>
#include <cstdio>
#include <iostream>
#include <math.h>
#include <assert.h>
#ifndef UINT16_MAX
	#define UINT16_MAX (0xFFFF)
#endif
#ifndef INT16_MAX
	#define INT16_MAX (0x7FFF)
#endif


#define UMAX(b) ((1ull<<(b))-1)
#define SMAX(b) (UMAX((b)-1))

wav::wav(unsigned int rate)
{
	strncpy (chunkID,"RIFF",4);
	riffSize = 0; // filesize minus 8
	strncpy(typeID,"WAVE",4);
	strncpy(formatChunkID,"fmt ",4);
	formatChunkSize = 16;
	formatTag = 1; // 1 = PCM linear quantization (uncompressed)
	nChannels = 1; // mono
	samplesPerSec = rate; // e.g., 48000

	bwfckSize=702; /* size of extension chunk ==702 */
	strncpy (bwfchunkID,"bext",4);
	strncpy (Description,"Film Sound Extraction",256);
	strncpy (Originator,"AEO-Light",32);
	strncpy (OriginationTime,"10:50:45",8); /* ASCII : hh:mm:ss */
	strncpy (OriginationDate,"2015:05:05",10);/* ASCII : yyyy:mm:dd */

	strncpy(bwfchunkID,"bext",4);
	Version=2;
	bitsPerSample = 16;
	strncpy(dataChunkID,"data",4);
	numframes=0;
	avg_inc=0;
	//timecon = 100;
	//dcrestore = new int16_t[timecon];
	val_avg = 0;
	buffer = NULL;
	bufSize = 0;
	samplesPerFrame = 2000;
	TimeReferenceHigh=0x000;
	TimeReferenceLow=0x0FF;
	yml1 = 0.5;
	xml1 = 0.5;
	ymr1 = 0.5;
	xr=0.5;
	yr=0.50;
	xl=0.5;
	yl=0.5;
	xmr1 = 0.5;
}

wav::~wav()
{
	// a wav file opened for reading (sythetic image) will have the
	// sound signal in this->buffer.
	if(buffer) delete [] buffer;
}

FILE *wav::open(const char *fn)
{
	// update the header regarding the size of the data:
	//audiofilename =new std::string(fn);
	//temp_audiofilename =new std::string( );
	//*temp_audiofilename = *audiofilename + ".tmp";

	xl = 0.5;
	xr = 0.5;

	bytesPerSec = samplesPerSec * bitsPerSample * nChannels;
	blockAlign = bitsPerSample * nChannels / 8;
	dataChunkSize = 0;
	numframes = 0;

	audio_file = fopen(fn, "wb+");
	if(audio_file==NULL)
	{
		//std::cerr << "Cannot open " << fn << " for writing\n";
		return NULL;
	}

	fwrite(this, 44, 1, audio_file);

	return audio_file;
}

void wav::writebuffer(float ** audioframe,int samples)
{
	double vl,vr;
	int32_t vall;
	int32_t valr;
	int b;

	numframes = samples/samplesPerFrame;
	for(int i =0; i<samples; i++)
	{
		xl = (audioframe[0][i]) ;
		if (nChannels ==2)
			xr = (audioframe[1][i]) ;
		else
			xr = xl;

		// rescale to unsigned int
		vall=int32_t(xl*(UMAX(bitsPerSample)));
		valr=int32_t(xr*(UMAX(bitsPerSample)));

		for(b=bitsPerSample; b>0; vall >>= 8, b-=8)
			fputc(vall & 0xFF, audio_file);

		if (nChannels ==2)
			for(b=bitsPerSample; b>0; valr >>= 8, b-=8)
				fputc(valr & 0xFF, audio_file);
	}
}
void wav::set_timecode(unsigned int seconds,unsigned int frames)
{

    unsigned long timesamples = seconds * (samplesPerSec);
    timesamples+=  frames*samplesPerFrame;

    TimeReferenceLow=timesamples;
    TimeReferenceHigh=timesamples>>32;

}

void wav::writeframe(float * audioframe,bool dcbias)
{
#define S(x) ((x)*2.0 - 1.0)

#define U(x) (((x)+1.0) / 2.0)

	double vl,vr;
	int32_t vall;
	int32_t valr;
	numframes++;
	int16_t oval;
	long avg=0;

	int b;

	LPF_Beta = 0.8;
	alpha = 0.98;

	float arrayloc=0;

	for(int i =0; i<samplesPerFrame; i++)
	{
		xl = (audioframe[(i*nChannels)]) ;
		if (nChannels ==2)
			xr = (audioframe[(i*nChannels)+1]) ;
		else
			xr=xl;

		yr = U(alpha*S(ymr1) + S(xr) - S(xmr1));

		yl = U(alpha*S(yml1) + S(xl) - S(xml1));

		ymr1 = yr;
		yml1 = yl;
		xml1 = xl;
		xmr1 = xr;

		hpol =hpol - (LPF_Beta * (hpol - yl));
		hpor =hpor - (LPF_Beta * (hpor - yr));


		// rescale to Signed int
		vall=int32_t((xl*UMAX(bitsPerSample))-(UMAX(bitsPerSample)/2));
		valr=int32_t((xr*UMAX(bitsPerSample))-(UMAX(bitsPerSample)/2));

		//fwrite(&vall,2,1,audio_file);
		for(b=bitsPerSample; b>0; vall >>= 8, b-=8)
			fputc(vall & 0xFF, audio_file);

		if (nChannels ==2)
			//fwrite(&valr,2,1,audio_file);
			for(b=bitsPerSample; b>0; valr >>= 8, b-=8)
				fputc(valr & 0xFF, audio_file);
	}
}

void wav::close()
{
	bytesPerSec = samplesPerSec * (bitsPerSample/8) * nChannels;
	blockAlign = bitsPerSample * nChannels / 8;
	dataChunkSize = numframes*samplesPerFrame* blockAlign;

	// riffSize = dataChunkSize + bwfckSize + 134
	// (134 = fmt+list+4*8+4, 4*8 = chunk headers, +4 for the "WAVE")
	// but we'll just calculate that at file close, so as to handle
	// any other chunks that developers may include later.

	if(audio_file==NULL)
	{
		std::cerr << "Error writing " << numframes*samplesPerFrame <<
				" samples\n";
		return;
	}

	// write the bext chunk at the end of the file
	fwrite(&(this->bwfchunkID[0]), 346, 1, audio_file);
	//skip two bytes of padding in the middle of the struct
	fwrite(&(this->TimeReferenceLow), 364, 1, audio_file);

	// update the header regarding the size of the data:
	riffSize = ftell(audio_file)-8;
	fseek ( audio_file , 0 , SEEK_SET );
	fwrite(this, 44, 1, audio_file);

	fclose(audio_file);
}

void wav::write(const char *fn, const std::vector<double> &signal)
{
	FILE *fp;

	int b;

	// update the header regarding the size of the data:
	bytesPerSec = samplesPerSec * (bitsPerSample/8) * nChannels;
	blockAlign = bitsPerSample * nChannels / 8;
	dataChunkSize = signal.size() * blockAlign;
	riffSize = dataChunkSize + 36;

	fp = fopen(fn, "wb");
	if(fp==NULL)
	{
		std::cerr << "Cannot open " << fn << " for writing\n";
		return;
	}

	fwrite(this, sizeof(*this), 1, fp);

	double v;
	uint32_t val;
	std::vector<double>::const_iterator i;
	for(i = signal.begin(); i != signal.end(); ++i)
	{
		// rescale to U16 = [0 , 2^16-1] = [0, 65535]
		v = (*i) * UMAX(bitsPerSample);
		if(v < 0) val = 0;
		else if(v > UMAX(bitsPerSample)) val = UMAX(bitsPerSample);
		else val = uint32_t(v);

		// flip the leading bit to convert from U16 to S16
		val -= UMAX(bitsPerSample)/2;

		//fwrite(&val,2,1,fp);
		for(b=bitsPerSample; b; val >>= 8, b-=8)
			fputc(val & 0xFF, fp);

	}

	fclose(fp);
}

void wav::BeginInfoChunk()
{
	infochunkPosition = ftell(audio_file);
	fwrite("LIST",1,4,audio_file);
	fwrite("\0\0\0\0",1,4,audio_file);
	fwrite("INFO",1,4,audio_file);
}

void wav::AddInfo(const char *id, const char *data)
{
	uint32_t dataSize = strlen(data)+1; // include null
	fwrite(id,1,4,audio_file);
	fwrite(&dataSize,4,1,audio_file);
	fwrite(data,1,dataSize,audio_file);
	if(dataSize%2) fputc(0, audio_file); // pad to even
}

void wav::EndInfoChunk()
{
	long endPos = ftell(audio_file);
	uint32_t infoSize = (endPos - infochunkPosition) - 8;
	fseek(audio_file, infochunkPosition+4, SEEK_SET);
	fwrite(&infoSize,4,1,audio_file);
	fseek(audio_file, 0, SEEK_END);
}

// VERY dumb WAV reader: assumes signed 16 bit samples (WAV standard)
// returns unsigned 16 bit samples
bool wav::read(const char *fn)
{
	audio_file = fopen(fn, "rb");
	if(audio_file == NULL)
	{
		std::cerr << "Cannot open " << fn << " for reading\n";
		perror(NULL);
		return false;
	}

	fread(this,44,1,audio_file);

	if(std::strncmp(this->chunkID,"RIFF",4)!=0)
	{
		std::cerr << "Not a WAV file.\n";
		return false;
	}

	if(this->formatTag != 1)
	{
		std::cerr << "WAV file codec format must be PCM\n";
		return false;
	}

	if(this->nChannels != 1)
	{
		std::cerr << "WAV file must be mono\n";
		return false;
	}

	// skip chunks until we get to the "data" chunk
	while(std::strncmp(this->dataChunkID, "data", 4) != 0)
	{
		// skip over this chunk to the header of next chunk
		fseek(audio_file, this->dataChunkSize, SEEK_CUR);

		//read the next chunk's ID and size
		fread(&(this->dataChunkID), 1, 4, audio_file);
		fread(&(this->dataChunkSize), 4, 1, audio_file);
	}

	bufSize = this->dataChunkSize/(this->bitsPerSample/8);

	int16_t *sbuf = new int16_t[bufSize];
	buffer = new uint16_t[bufSize]; // freed when the wav object is destroyed

	fread(sbuf, this->bitsPerSample/8, bufSize, audio_file);
	fclose(audio_file);

	for(uint32_t i=0; i<bufSize; ++i)
	{
		// lift the bits from signed to unsigned
		buffer[i] = uint16_t(int32_t(sbuf[i])+INT16_MAX+1);
	}
	delete [] sbuf;

	return true;
}

int wav::GetHeight(void) const
{
	return samplesPerSec / 24;
}

int wav::GetScanHeight(void) const
{
	return GetHeight() * 1.1;
}

int wav::GetOverlap(void) const
{
	return GetScanHeight() - GetHeight();
}

double *wav::GetFrame(long frameNum, double *buf) const
{
	int h,w;
	h = w = GetScanHeight();

	if(buf==NULL)
	{
		size_t sz = h*w;
		buf = new double[sz];
		if(buf==NULL)
		{
			std::cerr << "Out of Memory: synth Wav buf\n";
			exit(1);
		}
	}

	uint16_t *bp;

	bp = this->buffer + (frameNum-1) * GetHeight();
	double *p = buf;
	for(int r=0; r<h; r++)
	{
		double val = (double(bp[r]) / double(0x00FFFF));
		for(int c=0; c<w; c++)
		{
			*(p++) = val;
		}
	}
	return buf;
}

// 20% variable density; 10% black 50% variable area 10% black 10% frame marker
unsigned char *wav::GetFrameImage(size_t frameNum, unsigned char *buf,
		int &width, int &height, bool &endian) const
{
	height = GetScanHeight();

	width = height;

	if(buf==NULL)
	{
		size_t sz = height*width*2*4;
		buf = new unsigned char[sz];
		if(buf==NULL)
		{
			std::cerr << "Out of Memory: synth Wav buf\n";
			exit(1);
		}
	}

	uint16_t *bp;

	bp = this->buffer + (frameNum-1) * GetHeight();
	unsigned char *p = buf;
	int c;

	const uint16_t white[] = { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF };
	const uint16_t black[] = { 0x0000, 0x0000, 0x0000, 0x0000 };

	for(int r=0; r<height; r++)
	{
		uint16_t val = bp[r];

		// variable density
		for(c=0; c<4*width*.2; c++)
		{
			memcpy(p,&val,2);
			p+=2;
		}
		// black
		for( ; c<4*width*.3; c++)
		{
			memcpy(p,black,2);
			p+=2;
		}

		// variable area: white then black
		double area = double(val)/double(UINT16_MAX);
		for( ; c<4*width*(.3 + area*.5); c++)
		{
			memcpy(p, white, 2);
			p+=2;
		}

		for( ; c<4*width; c++)
		{
			memcpy(p, black, 2);
			p+=2;
		}

	}

	// mark the frame start and end
	const uint16_t yellow[] = { 0xFFFF, 0xFFFF, 0x0000, 0x0000};
	int overlap = (height - GetHeight())/2;

	int i;

	// top of frame
	for(i=0; i<6;i++)
	{
		p = buf + 2*4*width*(overlap+i) + 2*4*(width/10)*9;
		for(c=0; c<width/10; c++, p+=8) memcpy(p,yellow,8);
	}

	// bottom of frame
	for(i=0; i<6;i++)
	{
		p= buf + 2*4*width*(overlap + GetHeight() + i)+ 2*4*(width/10)*9;
		for(c=0; c<width/10; c++, p+=8) memcpy(p,yellow,8);
	}

	endian = false;
	return buf;
}
