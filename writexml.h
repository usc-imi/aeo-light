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

#ifndef WRITEXML_H
#define WRITEXML_H

#include <cstdio>

#include <QFileInfo>
#include <QDateTime>

#include "frame_view_gl.h"
#include "FilmScan.h"

void write_premis_xml(const char *fn, const Frame_Window &project,
		const FilmScan &inFile, const char *soundFile,
		long firstFrame, long numFrames);

void write_mods_xml(const char *fn, const Frame_Window &project,
		const FilmScan &inFile, const char *soundFile,
		long firstFrame, long numFrames);

#endif // WRITEXML_H
