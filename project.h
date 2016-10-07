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

#ifndef PROJECT_H
#define PROJECT_H

#include <iostream>
#include <string>
#include <vector>

#include <QTextStream>

#include "FilmScan.h"
#include "overlap.h"

class FrameRegion {
private:
	unsigned int left;
	unsigned int right;
public:
	FrameRegion() {};
	FrameRegion(int l, int r) : left(l), right(r) {} ;

	int Left() const { return left; }
	int Right() const { return right; }
	int Width() const { return right - left + 1; }
};

typedef std::vector< double > LampMask;

class Project {
public:
	std::string filename; // file name of this project file

	FilmScan inFile;
	unsigned int firstFrameIndex; // = 0
	unsigned int lastFrameIndex; // = 0

	std::vector<FrameRegion> soundBounds;

	unsigned int overlapThreshold; // default = 20



public:
	Project();
	Project(std::string filename);
	~Project();

	void Initialize();

	bool SourceScan(std::string filename, SourceFormat fmt=SOURCE_UNKNOWN);

};


#endif
