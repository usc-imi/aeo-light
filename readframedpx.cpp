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
        int &width,int &height,bool &endian, GLenum &pix_fmt,int &num_components)
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


        if(dpx.header.BitDepth(0) == 10)
        {
            pix_fmt = GL_UNSIGNED_INT_10_10_10_2;
            buf = new unsigned char [dpx.header.Width() * dpx.header.Height() * 4];
            num_components=4;
            pixel_size=4;

        }
        else if (dpx.header.BitDepth(0) == 16 && numChannels ==3)
        {
            buf = new unsigned char [dpx.header.Width() * dpx.header.Height() * 8];
            pix_fmt = GL_UNSIGNED_SHORT;
            num_components=3;
            pixel_size = 6;
        }
        else //16bit luma
        {

            buf = new unsigned char [dpx.header.Width() * dpx.header.Height() * 2];
            pix_fmt = GL_UNSIGNED_SHORT;
            num_components=1;
            pixel_size = 2;

        }
		if(buf == NULL)
		{
			throw AeoException("Out of Memory: DPX buf");
		}


	dpx.fd->Seek( dpx.header.imageOffset,dpx.fd->kStart);
	width=dpx.header.Width();
	height= dpx.header.Height();

	endian=dpx.header.RequiresByteSwap();
    dpx.fd->Read(buf,dpx.header.Width() * dpx.header.Height() * pixel_size);
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
