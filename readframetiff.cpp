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

#include "readframetiff.h"

#include <tiffio.h>
#include <errno.h>

#include "aeoexception.h"

double *ReadFrameTIFF(const char *fn, double *buf)
{
	throw AeoException("Write ReadFrameTIFF() function.");
	return buf;
}

unsigned char *ReadFrameTIFF_ImageData(const char *fn, unsigned char *buf,
		int &width, int &height, bool &endian,
		GLenum &pix_fmt, int &num_components)
{
	TIFF* tif = TIFFOpen(fn, "r");

	if(tif == NULL)
	{
		QString msg;
		msg += "ReadFrameTIFF: Cannot open ";
		msg += fn;
		msg += "\n";
		if(errno)
			msg += strerror(errno);
		throw AeoException(msg);
	}

	if(TIFFIsTiled(tif))
	{
		TIFFClose(tif);
		throw AeoException("Tiled TIFF not supported.");
	}

	uint16 bitsPerSample[3];
	uint16 bitDepth;
	uint16 numChannels;
	uint32 imageHeight;
	uint32 imageWidth;
	uint16 planarConfig;
	uint16 sampleFormat(0);
	uint16 photometric;

	if(TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &numChannels) != 1)
	{
		TIFFClose(tif);
		throw AeoException("Invalid TIFF: no SAMPLESPERPIXEL tag");
	}

	if(TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &photometric) != 1)
		photometric = (numChannels==1)?PHOTOMETRIC_MINISBLACK:PHOTOMETRIC_RGB;
	else if(!(numChannels==1 ||
			(numChannels==3 && photometric == PHOTOMETRIC_RGB)))
	{
		TIFFClose(tif);
		throw AeoException("TIF must be either RGB or grayscale.");
	}

	if(TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &imageWidth) != 1 ||
			TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &imageHeight) != 1)
	{
		TIFFClose(tif);
		throw AeoException("Invalid TIFF image size");
	}

	// libtiff docs say this returns 1 value, but other sources say it
	// returns <numChannels> values, so just to be safe, we allow it to
	// return up to 3 values and just use the first one.
	if(TIFFGetFieldDefaulted(tif, TIFFTAG_BITSPERSAMPLE, bitsPerSample) != 1)
	{
		TIFFClose(tif);
		throw AeoException("Invalid TIFF: no bitsPerSample tag");
	}

	bitDepth = bitsPerSample[0];

	if(bitDepth != 8 && bitDepth != 16)
	{
		TIFFClose(tif);
		throw AeoException("TIFF must be 8-bit or 16-bit unsigned int.");
	}

	if(TIFFGetFieldDefaulted(tif, TIFFTAG_PLANARCONFIG, &planarConfig) != 1)
	{
		TIFFClose(tif);
		throw AeoException("Invalid TIFF: no planarConfig tag");
	}

	if(TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLEFORMAT, &sampleFormat) != 1)
	{
		TIFFClose(tif);
		throw AeoException("Invalid TIFF: no sampleFormat (e.g., uint) tag.");
	}

	if(sampleFormat != SAMPLEFORMAT_UINT)
	{
		TIFFClose(tif);
		throw AeoException("TIFF pixel datatype is not unsigned int.");
	}

	tdata_t tbuf;
	uint32 row;

	tbuf = _TIFFmalloc(TIFFScanlineSize(tif));
	if((unsigned char *)tbuf == NULL)
	{
		TIFFClose(tif);
		throw AeoException("Out of memory: TIFF scanline buffer.");
	}

	if(buf == NULL)
	{
		buf = new unsigned char[
				imageWidth * imageHeight * numChannels * (bitDepth/8u)];
		if(buf==NULL)
		{
			_TIFFfree(tbuf);
			TIFFClose(tif);
			throw AeoException("Out of memory: TIFF image buffer.");
		}
	}

	switch(planarConfig)
	{
	case PLANARCONFIG_CONTIG:
		for (row = 0; row < imageHeight; row++)
		{
			if(TIFFReadScanline(tif, tbuf, row) != 1)
			{
				_TIFFfree(tbuf);
				TIFFClose(tif);
				throw AeoException("TIFF I/O Error.");
			}
			memcpy(buf+row*imageWidth*numChannels*(bitDepth/8u), tbuf,
					imageWidth*numChannels*(bitDepth/8u));
		}
		break;
	case PLANARCONFIG_SEPARATE:
		uint16 s, col;

		for (s = 0; s < numChannels; s++)
		{
			for (row = 0; row < imageHeight; row++)
			{
				if(TIFFReadScanline(tif, tbuf, row, s) != 1)
				{
					_TIFFfree(tbuf);
					TIFFClose(tif);
					throw AeoException("TIFF I/O Error.");
				}
				for(col = 0; col < imageWidth; col++)
				{
					memcpy(buf+(bitDepth/8u)*(s+numChannels*(col+imageWidth)),
							tbuf, (bitDepth/8u));
				}
			}
		}
		break;
	default:
		_TIFFfree(tbuf);
		TIFFClose(tif);
		throw AeoException("Invalid Planar Config in TIFF.");
	}

	_TIFFfree(tbuf);
	TIFFClose(tif);

	width = imageWidth;
	height = imageHeight;
	num_components = numChannels;

	if(bitDepth==8) pix_fmt = GL_UNSIGNED_BYTE;
	else pix_fmt = GL_UNSIGNED_SHORT;

	endian = false; // libtiff automatically converts to native endianness

	return buf;
}
