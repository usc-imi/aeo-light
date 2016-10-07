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

#ifndef READFRAMETIFF_H
#define READFRAMETIFF_H

#include <QOpenGLTexture>

double *ReadFrameTIFF(const char *fn, double *buf);
unsigned char *ReadFrameTIFF_ImageData(const char *fn, unsigned char *buf,
		int &width, int &height, bool &endian,
		GLenum &pix_fmt, int &num_components);

#endif // READFRAMETIFF_H
