#include <OpenEXR/ImfRgbaFile.h>
#include <OpenEXR/ImfArray.h>

#include "aeoexception.h"
#include "readframeexr.h"

double *ReadFrameEXR(const char *fn, double *buf=NULL)
{
	Imf::RgbaInputFile exr(fn);
	Imath::Box2i dw = exr.dataWindow();
	int width = dw.max.x - dw.min.x + 1;
	int height = dw.max.y - dw.min.y + 1;

	// keep this around, since all the frames will be the same size
	static Imf::Array2D<Imf::Rgba> pixels;

	if(buf == NULL)
	{
		buf = new double [width*height];
		if(buf == NULL)
		{
			throw AeoException("Out of Memory: EXR buf");
		}
	}

	pixels.resizeErase(height, width);
	exr.setFrameBuffer(&pixels[0][0] - dw.min.x - dw.min.y * width, 1, width);
	exr.readPixels(dw.min.y, dw.max.y);

	int i = 0;

	// "convert" from half BGRA to double Y
	for(int x=0; x<height; ++x)
		for(int y=0; y<width; ++y, ++i)
		{
			buf[i] = (
					double(pixels[x][y].r) +
					double(pixels[x][y].g) +
					double(pixels[x][y].b) ) / 3.0;
		}

	return buf;
}

unsigned char *ReadFrameEXR_ImageData(const char *exrfn, unsigned char *buf,
		int &width, int &height, bool &endian)
{
	Imf::RgbaInputFile exr(exrfn);
	Imath::Box2i dw = exr.dataWindow();
	width = dw.max.x - dw.min.x + 1;
	height = dw.max.y - dw.min.y + 1;
	endian = false;

	// keep this around, since all the frames will be the same size
	static Imf::Array2D<Imf::Rgba> pixels;

	if(buf == NULL)
	{
		buf = new unsigned char [width*height* 4];//8 or 10 bit
		if(buf == NULL)
		{
			throw AeoException("Out of Memory: EXR buf");
		}
	}

	exr.setFrameBuffer((Imf::Rgba *)buf,1,width);
	exr.readPixels(dw.min.y, dw.max.y);

	return buf;
}
