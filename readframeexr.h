#ifndef READFRAMEEXR_H
#define READFRAMEEXR_H

double *ReadFrameEXR(const char *dpxfn, double *buf);
unsigned char *ReadFrameEXR_ImageData(const char *exrfn, unsigned char *buf,
		int &width, int &height, bool &endian);

#endif // READFRAMEEXR_H

