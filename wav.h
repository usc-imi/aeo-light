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

#ifndef WAV_H
#define WAV_H

#include <cstdio>
#include <vector>
#include <stdint.h>

class wav
{
public:


	/*      Wave Header   12bytes    */
	char chunkID[4];
	uint32_t riffSize;
	char typeID[4];



	/*      Format Chunk Header    28   */
	char formatChunkID[4];
	uint32_t formatChunkSize;  // == 16
	uint16_t formatTag;
	uint16_t nChannels;
	uint32_t samplesPerSec;
	uint32_t bytesPerSec;
	uint16_t blockAlign;
	uint16_t bitsPerSample;

	/*      Data Chunk    4 + Samples     */
	char dataChunkID[4];
	uint32_t dataChunkSize;
	/*      BWF Chunk Header    710   */

	char bwfchunkID[4]; /* (broadcastextension)ckID=bext. */
	uint32_t bwfckSize; /* size of extension chunk ==702 */
	char Description[256]; /* ASCII : «Description of the sound sequence» */
	char Originator[32]; /* ASCII : «Name of the originator» */
	char OriginatorReference[32];/* ASCII : «Reference of the originator» */
	char OriginationDate[10]; /* ASCII : «yyyy:mm:dd» */
	char OriginationTime[8]; /* ASCII : «hh:mm:ss» */
	uint32_t TimeReferenceLow; /* First sample count since midnight, low word */
	uint32_t TimeReferenceHigh; /* First sample count since midnight, high word */
	uint16_t Version; /* Version of the BWF; unsigned binary number */
	uint8_t UMID [64]; /* Binary byte 0 of SMPTE UMID */

	uint16_t LoudnessValue; /* WORD : «Integrated Loudness Value of the file in LUFS (multiplied by 100) » */
	uint16_t LoudnessRange; /* WORD : «Loudness Range of the file in LU (multiplied by 100) » */
	uint16_t MaxTruePeakLevel; /* WORD : «Maximum True Peak Level of the file expressed as dBTP (multiplied by 100) » */
	uint16_t MaxMomentaryLoudness; /* WORD : «Highest value of the Momentary Loudness Level of the file in LUFS (multiplied by 100) » */
	uint16_t MaxShortTermLoudness; /* WORD : «Highest value of the Short-Term Loudness Level of the file in LUFS (multiplied by 100) » */
	uint8_t Reserved[180]; /* 180 bytes, reserved for future use, set to “NULL” */
	char CodingHistory[100]; /* ASCII : « History coding » */


	FILE * audio_file;
	long infochunkPosition;

	//std::string * audiofilename;
	//std::string  * temp_audiofilename;
	int numframes;
	int avg_inc;
	int16_t val_avg;
	int16_t val_hp;
	int16_t val_lp;
	int16_t val_prev;
	float v_prev;
	uint16_t *buffer;
	uint32_t bufSize;
	uint32_t samplesPerFrame;
	double yr,xr,yl,xl,xmr1,ymr1,xml1,yml1;
	double hpol,hpor;
	double LPF_Beta;
	double alpha;


public:
	wav(unsigned int rate=48000);

	~wav();
	//long timecon ;
	//int16_t* dcrestore;
	void write(const char *fn, const std::vector<double> &signal);
	FILE *open(const char *fn);
	void writeframe(float* audio_frame,bool dcbias);
	void writebuffer(float **audio_buffer, int samples);
	void close();
	void BeginInfoChunk();
	void AddInfo(const char *id, const char *data);
	void EndInfoChunk();
	void set_timecode(unsigned int seconds, unsigned int frames);
	bool read(const char *fn);
	int GetHeight(void) const;
	int GetScanHeight(void) const;
	int GetOverlap(void) const;
	double *GetFrame(long frameNuf, double *buf) const;
	unsigned char *GetFrameImage(size_t frameNum, unsigned char *buf,
			int &width, int &height, bool &endian) const;
};

#endif // WAV_H
