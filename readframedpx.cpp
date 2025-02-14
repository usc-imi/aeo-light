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

#include <iostream>
#include <sstream>
#include <string>
#include <cmath>
#include <errno.h>

#include "DPX.h"
#include "readframedpx.h"
#include "aeoexception.h"

using namespace dpx;

/* ReadFrameDPX - read a single frame from a DPX file
 * arguments:
 *   dpxfn: the filename of the dpx file
 *   buf:   an existing target buffer of sufficient size, or NULL
 *
 * The DPX image is returned in buf (which is also returned by the function)
 * in row-major order, top-to-bottom, left-to-right
 */
double *ReadFrameDPX(const char *dpxfn, double *buf=NULL)
{
	InStream img;

	// keep this around, since all the frames will be the same size
	static unsigned char *byteBuf;

	if(!img.Open(dpxfn))
	{
		QString msg;
		msg += "ReadFrameDPX: Cannot open ";
		msg += dpxfn;
		msg += "\n";
		if(errno)
			msg += strerror(errno);
		throw AeoException(msg);
	}

	dpx::Reader dpx;
	dpx.SetInStream(&img);
	if(!dpx.ReadHeader())
	{
		QString msg;
		msg += "ReadFrameDPX: Cannot parse DPX header of ";
		msg += dpxfn;
		msg += "\n";
		if(errno)
			msg += strerror(errno);
		img.Close();
		throw AeoException(msg);
	}

	dpx::DataSize size = dpx.header.ComponentDataSize(0);
	int bitDepth = dpx.header.ComponentByteCount(0) * 8;

	int numChannels = dpx.header.ImageElementComponentCount(0);

	if(buf == NULL)
	{
		buf = new double [dpx.header.Width() * dpx.header.Height()];
		if(buf == NULL)
		{
			throw AeoException("Out of Memory: DPX buf");
		}
	}

	if(byteBuf == NULL)
	{
		byteBuf = new unsigned char
			[dpx.header.Width() * dpx.header.Height() *
			numChannels * dpx.header.ComponentByteCount(0)];

		if(byteBuf == NULL)
		{
			throw AeoException("Out of Memory: DPX byteBuf");
		}
	}

	dpx.ReadImage(byteBuf);

	// Convert from uint8 [0-255] or unit16 [0-65535] to double [0-1]
	// Note that the color-to-grayscale isn't weighted to give green
	// more influence: so the picture area may not "look nice" to human
	// perception, but the soundtrack area isn't in color anyway, and the
	// portions of the code that care about the picture area will work
	// just as well with this alternate response scale (equal weight) as
	// with the human perception scale, and the computation is faster.
	long i;
	double scale;

	if(bitDepth == 8)
	{
		scale = 1.0/(numChannels * 0x00FF);
		if(numChannels == 1)
		{
			for(i=0; i< dpx.header.Width() * dpx.header.Height(); i++)
				buf[i] = double(byteBuf[i]) * scale;
		}
		else
		{
			for(i=0; i< dpx.header.Width() * dpx.header.Height(); i++)
				buf[i] =
					(int(byteBuf[3*i]) +
					int(byteBuf[3*i+1]) +
					int(byteBuf[3*i+2]))
					* scale;
		}
	}
	else if(bitDepth == 16)
	{
		scale = 1.0/(numChannels * 0x00FFFF);
		if(numChannels == 1)
		{
			for(i=0; i< dpx.header.Width() * dpx.header.Height(); i++)
				buf[i] = double(byteBuf[2*i] + 0x0100*byteBuf[2*i+1]) * scale;
		}
		else
		{
			int c;
			double rgb[3];
			for(i=0; i< dpx.header.Width() * dpx.header.Height(); i++)
			{
				for(c=0; c<3; c++)
					rgb[c] = byteBuf[6*i+2*c] + 0x0100*byteBuf[6*i+2*c+1];
				buf[i] = (rgb[0] + rgb[1] + rgb[2]) * scale;
			}
		}
	}
	else
	{
		fprintf(stderr, "ERROR: Unsupported bit depth: %d in '%s'\n",
				bitDepth, dpxfn);
		exit(1);
	}

	img.Close();

	return buf;
}
unsigned char* ReadFrameDPX_ImageData(const char *dpxfn, unsigned char *buf,
		int &bufSize, int &width,int &height,bool &endian,
		GLenum &pix_fmt,int &num_components)
{
	InStream img;

	// keep this around, since all the frames will be the same size
	static unsigned char *byteBuf;

	if(!img.Open(dpxfn))
	{
		QString msg;
		msg += "ReadFrameDPX_ImageData: Cannot open ";
		msg += dpxfn;
		msg += "\n";
		if(errno) msg += strerror(errno);
		throw AeoException(msg);
	}

	dpx::Reader dpx;
	dpx.SetInStream(&img);
	if(!dpx.ReadHeader())
	{
		img.Close();
		QString msg;
		msg += "ReadFrameDPX_ImageData: Cannot parse DPX header of  ";
		msg += dpxfn;
		msg += "\n";
		if(errno) msg += strerror(errno);
		throw AeoException(msg);
	}

	dpx::DataSize size = dpx.header.ComponentDataSize(0);
	int bitDepth = dpx.header.ComponentByteCount(0) * 8;

	int numChannels = dpx.header.ImageElementComponentCount(0);
	int pixel_size; //in bytes;
	bool doRawRead(false);
	int remainder(0);

	width=dpx.header.Width();
	height= dpx.header.Height();

	if(dpx.header.BitDepth(0) == 10 && numChannels == 3)
	{
		pix_fmt = GL_UNSIGNED_INT_10_10_10_2;
		bufSize = width * height * 4;
		num_components=4;
		pixel_size=4;
		doRawRead = true;
	}
	else if (dpx.header.BitDepth(0) == 16 && numChannels ==3)
	{
		// XXX: Why 8 instead of pixel_size of 6?
		bufSize = width * height * 8;
		pix_fmt = GL_UNSIGNED_SHORT;
		num_components=3;
		pixel_size = 6;
		doRawRead = true;
	}
	else if(dpx.header.BitDepth(0) == 16 && numChannels == 1) //16bit luma
	{
		bufSize = width * height * 2;
		pix_fmt = GL_UNSIGNED_SHORT;
		num_components=1;
		pixel_size = 2;
		doRawRead = true;
	}
	else
	{
		if(
				dpx.header.ImageElementComponentCount(0) == 1 &&
				dpx.header.BitDepth(0)==10 &&
				dpx.header.ImagePacking(0) == 1 &&
				dpx.header.Width() % 3 != 0 )
		{
			remainder = 3 - (width % 3);
		}

		bufSize = width * height * numChannels * 2;
		num_components = numChannels;
		pix_fmt = GL_UNSIGNED_SHORT;
		pixel_size = 2;
		doRawRead = false;
	}

	if(buf == NULL)
	{
		buf = new unsigned char [bufSize];
		if(buf == NULL)
			throw AeoException("Out of Memory: DPX image buf");
	}

	if(doRawRead)
	{
		endian=dpx.header.RequiresByteSwap();

		dpx.fd->Seek( dpx.header.imageOffset,dpx.fd->kStart);
		dpx.fd->Read(buf,dpx.header.Width() * dpx.header.Height() * pixel_size);
	}
	else
	{
		if(!remainder)
		{
			if(!dpx.ReadImage(buf, kWord, dpx.header.ImageDescriptor(0)))
				throw("This DPX encoding is not supported (e.g., RLE)");
		}
		else
		{
			// remainder non-zero means that the DPX is grayscale with
			// 10-bit samples packed three-to-a-word, but the image width
			// is not divisible by 3
			// opendpx cannot read this properly (it requires the scanlines
			// to break on word boundaries), so we have to read it ourselves
			// and unpack it manually.

			int numWords = ceil(double(width*height)/3.0);
			static uint32_t *wbuf(NULL);
			if(wbuf == NULL)
			{
				// make a buffer to hold the 32-bit words
				wbuf = new uint32_t[numWords];
				if(wbuf == NULL)
					throw AeoException("Out of Memory: DPX word buf");

			}

			// read in the whole array of 32-bit words from the DPX file
			dpx.fd->Seek(dpx.header.imageOffset, dpx.fd->kStart);
			dpx.fd->Read(wbuf, numWords*sizeof(uint32_t));

			// swap the bytes in the words if necessary
			if(dpx.header.RequiresByteSwap())
			{
				for(int w=0; w<numWords; ++w)
				{
					uint32_t *cp = wbuf+w;
					*cp = (*cp >> 16) | (*cp << 16);
					*cp = ((*cp & 0xFF00FF00) >> 8) | ((*cp & 0x00FF00FF) << 8);
				}
			}

			// use a proxy to more easily enter the extracted sample values
			// as 16-bit words in the buf array.
			uint16_t *ubuf;
			ubuf = reinterpret_cast<uint16_t *>(buf);

			uint32_t *word; // pointer into wbuf
			// double scale = 65535.0 / 1023.0; // scale 10-bit to 16-bit

			int nSample = 0; // number of samples remaining in the current word
			word = wbuf; // start at the beginning of the buffer
			for(int i=0; i<width*height; ++i)
			{
				if(nSample==0)
				{
					word++;
					nSample = 3;
					*word >>=2; // remove the fill bits
				}
				// unpack the 10-bit word and rescale it to 16 bits
				// ubuf[i] = uint16_t(((*word) & 0x03FF) * scale);
				// approximate this scaling with bit operations for speed:
				ubuf[i] = *word & 0x03FF;
				ubuf[i] = (ubuf[i] << 6) | (ubuf[i] >> 4);
				// shift the 32-bit word down and update the packed count
				(*word) >>= 10;
				nSample--;
			}
		}

		// either way, we've already swapped the bytes if it was needed.
		endian = false;

	}

	img.Close();

	return buf;

}


#ifdef USEBOOST
/* ReadFrameDPX - read a single frame from a DPX file
 * arguments:
 *   dpxfn: the filename of the dpx file
 *   buf:   an existing target buffer of sufficient size, or NULL
 *
 * The DPX image is returned in buf (which is also returned by the function)
 * in row-major order, top-to-bottom, left-to-right
 */
boost::numeric::ublas::matrix<double> ReadFrameDPX(const char *dpxfn)
{
	InStream img;
	boost::numeric::ublas::matrix<double> buf;

	// keep this around, since all the frames will be the same size
	static unsigned char *byteBuf;

	if(!img.Open(dpxfn))
	{
		QString msg;
		msg += "ReadFrameDPX: Cannot open ";
		msg += dpxfn;
		msg += "\n";
		if(errno) msg += strerror(errno);
		throw AeoException(msg);
	}

	dpx::Reader dpx;
	dpx.SetInStream(&img);
	if(!dpx.ReadHeader())
	{
		img.Close();
		QString msg;
		msg += "ReadFrameDPX: Cannot parse DPX header of ";
		msg += dpxfn;
		msg += "\n";
		if(errno) msg += strerror(errno);
		throw AeoException(msg);
	}

	dpx::DataSize size = dpx.header.ComponentDataSize(0);
	int bitDepth = dpx.header.ComponentByteCount(0) * 8;

	int numChannels = dpx.header.ImageElementComponentCount(0);

	buf.resize(dpx.header.Height(), dpx.header.Width(), false);

	if(byteBuf == NULL)
	{
		byteBuf = new unsigned char
			[dpx.header.Width() * dpx.header.Height() *
			numChannels * dpx.header.ComponentByteCount(0)];

		if(byteBuf == NULL)
		{
			throw AeoException("Out of Memory: DPX byteBuf");
		}
	}

	dpx.ReadImage(byteBuf);

	// Convert from uint8 [0-255] or unit16 [0-65535] to double [0-1]
	// Note that the color-to-grayscale isn't weighted to give green
	// more influence: so the picture area may not "look nice" to human
	// perception, but the soundtrack area isn't in color anyway, and the
	// portions of the code that care about the picture area will work
	// just as well with this alternate response scale (equal weight) as
	// with the human perception scale, and the computation is faster.
	long i,r,c;
	double scale;

	if(bitDepth == 8)
	{
		scale = 1.0/(numChannels * 0x00FF);
		if(numChannels == 1)
		{
			i = 0;
			for(r=0; r<dpx.header.Height(); r++)
			for(c=0; c<dpx.header.Width(); c++)
			{
				buf(r,c) = double(byteBuf[i++]) * scale;
			}
		}
		else
		{
			i = 0;
			for(r=0; r<dpx.header.Height(); r++)
			for(c=0; c<dpx.header.Width(); c++)
			{
				buf(r,c) =
					(int(byteBuf[3*i]) +
					int(byteBuf[3*i+1]) +
					int(byteBuf[3*i+2]))
					* scale;
				i++;
			}
		}
	}
	else if(bitDepth == 16)
	{
		scale = 1.0/(numChannels * 0x00FFFF);
		if(numChannels == 1)
		{
			i = 0;
			for(r=0; r<dpx.header.Height(); r++)
			for(c=0; c<dpx.header.Width(); c++)
			{
				buf(r,c) = double(byteBuf[2*i] + 0x0100*byteBuf[2*i+1]) * scale;
				i++;
			}
		}
		else
		{
			int ch;
			double rgb[3];

			i = 0;
			for(r=0; r<dpx.header.Height(); r++)
			for(c=0; c<dpx.header.Width(); c++)
			{
				for(ch=0; ch<3; ch++)
					rgb[ch] = byteBuf[6*i+2*ch] + 0x0100*byteBuf[6*i+2*ch+1];
				buf(r,c)= (rgb[0] + rgb[1] + rgb[2]) * scale;
				i++;
			}
		}
	}
	else
	{
		fprintf(stderr, "ERROR: Unsupported bit depth: %d in '%s'\n",
				bitDepth, dpxfn);
		exit(1);

	}
	img.Close();
	return buf;
}
#endif
