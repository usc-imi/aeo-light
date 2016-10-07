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

#include <cstdio>

#include <QFileInfo>
#include <QDateTime>
#include <QMediaPlayer>

#include "frame_view_gl.h"
#include "FilmScan.h"

#include "writexml.h"

class FileInfo
{
public:
	std::string filename;
	std::string ext;
	std::string path;
	size_t bytes;
	unsigned long frame_in;
	unsigned long frame_out;
	std::string source;
	std::string bounds;
	std::string pixbounds;

	// extra stuff for MODS format:
	size_t duration;
	size_t bitdepth;
	std::string codec;
	size_t samplerate;

};

void file_premis_xml(FILE *fp, const FileInfo &finfo);
void file_mods_xml(FILE *fp, const FileInfo &finfo);

void write_premis_xml(const char *fn, const Frame_Window &project,
		const FilmScan &inFile, const char *soundFile,
		long firstFrame, long numFrames)
{

	FILE *fp = fopen(fn, "w");
	if(fp == NULL) return;

	// XML header
	fprintf(fp,"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	fprintf(fp,"<premis xmlns=\"info:lc/xmlns/premis-v2\" "
			"xmlns:xsi=\"http://www.w3c.org/2001/XMLSchema-instance\"\n");
	fprintf(fp,
			"\tversion=\"2.0\" xmlns:aeo=\"http://imi.cas.sc.edu/mirc/\">\n");

	// the exported file
	QFileInfo qinfo(soundFile);

	FileInfo f2info;

	f2info.filename = qinfo.fileName().toStdString();
	f2info.ext = qinfo.suffix().toLower().toStdString();
	f2info.path = qinfo.path().toStdString();
	f2info.bytes = qinfo.size();

	//f2audio = audioinfo(file2);

	//f2info.duration = f2audio.Duration;
	//f2info.bitdepth = fmt.audio.bitdepth;
	//f2info.codec = fmt.audio.codec;
	//f2info.samplerate = fmt.audio.sampling_rate;

	f2info.frame_in = firstFrame;
	f2info.frame_out = firstFrame + numFrames - 1;

	size_t w = inFile.Width();
	f2info.bounds = QString("%1 - %2").
			arg(size_t(project.bounds[0]*w)).
			arg(size_t(project.bounds[1]*w)).toStdString();
	f2info.pixbounds = QString("%1 - %2").
			arg(size_t(project.pixbounds[0]*w)).
			arg(size_t(project.pixbounds[1]*w)).toStdString();
	f2info.source = inFile.GetBaseName();

	file_premis_xml(fp, f2info);

	// XML Footer
	fprintf(fp,"</premis>\n");

	fclose(fp);

	return;
}

//=============================================================================
void file_premis_xml(FILE *fp, const FileInfo &finfo)
{

	fprintf(fp, "\t<object xsi:type=\"file\">\n");

	// Filename
	fprintf(fp,"\t\t<objectIdentifier>\n");
	fprintf(fp,"\t\t\t<objectIdentifierType>filename</objectIdentifierType>\n");
	fprintf(fp,"\t\t\t<objectIdentifierValue>%s</objectIdentifierValue>\n",
			finfo.filename.c_str());
	fprintf(fp,"\t\t</objectIdentifier>\n");

	fprintf(fp,"\t\t<objectCharacteristics>\n");
	fprintf(fp,"\t\t\t<compositionLevel>0</compositionLevel>\n");
	fprintf(fp,"\t\t</objectCharacteristics>\n");

	// File size
	fprintf(fp,"\t\t<size>%ld</size>\n", finfo.bytes);

	// File format
	fprintf(fp,"\t\t<format>\n");
	fprintf(fp,"\t\t\t<formatDesignation>\n");
	fprintf(fp,"\t\t\t\t<formatName>audio/%s</formatName>\n",finfo.ext.c_str());
	fprintf(fp,"\t\t\t</formatDesignation>\n");
	fprintf(fp,"\t\t</format>\n");

	// Creator
	fprintf(fp,"\t\t<creatingApplication>\n");
	fprintf(fp,"\t\t\t<!-- AEO-Light extracts sound from video files "
			"containing an image of the soundtrack. -->\n");
	fprintf(fp,"\t\t\t<creatingApplicationName>AEO-Light"
			"</creatingApplicationName>\n");
	fprintf(fp,"\t\t\t<creatingApplicationVersion>2.1"
			"</creatingApplicationVersion>\n");
	fprintf(fp,"\t\t\t<dateCreatedByApplication>%s"
			"</dateCreatedByApplication>\n",
			QDateTime::currentDateTime().toString("yyyy-MM-dd").
			toStdString().c_str());
	fprintf(fp,"\t\t\t<creatingApplicationExtension>\n");
	fprintf(fp,"\t\t\t\t<!-- %s %s\n",
			"Locally defined elements specifying segement of video source",
			"from which this file has been produced.");
	fprintf(fp,"\t\t\t\t-->\n");
	fprintf(fp,"\t\t\t\t<aeo:sourceFrameIn>%lu</aeo:sourceFrameIn>\n",
			finfo.frame_in);
	fprintf(fp,"\t\t\t\t<aeo:sourceFrameOut>%lu</aeo:sourceFrameOut>\n",
			finfo.frame_out);
	fprintf(fp,"\t\t\t\t<aeo:trackBounds aeo:num=\"1\">%s</aeo:trackBounds>\n",
			finfo.bounds.c_str());
	fprintf(fp,"\t\t\t\t<aeo:pixBounds aeo:num=\"1\">%s</aeo:pixBounds>\n",
			finfo.pixbounds.c_str());
	fprintf(fp,"\t\t\t</creatingApplicationExtension>\n");
	fprintf(fp,"\t\t</creatingApplication>\n");

	fprintf(fp,"\t\t<relationship>\n");
	fprintf(fp,"\t\t\t<relationshipType>structual</relationshipType>\n");
	fprintf(fp,"\t\t\t<relationshipSubType>has sibling"
			"</relationshipSubType>\n");
	fprintf(fp,"\t\t\t<relatedObjectIdentification>\n");
	fprintf(fp,"\t\t\t\t<relatedObjectIdentifierType>source filename"
			"</relatedObjectIdentifierType>\n");
	fprintf(fp,"\t\t\t\t<relatedObjectIdentifierValue>%s"
			"</relatedObjectIdentifierValue>\n",
			finfo.source.c_str());
	fprintf(fp,"\t\t\t</relatedObjectIdentification>\n");
	fprintf(fp,"\t\t\t<relatedEventIdentifier>\n");
	fprintf(fp,"\t\t\t\t<relatedEventIdentifierType>AEO-Light"
			"</relatedEventIdentifierType>\n");
	fprintf(fp,"\t\t\t\t<relatedEventIdentifierValue>1"
			"</relatedEventIdentifierValue>\n");
	fprintf(fp,"\t\t\t</relatedEventIdentifier>\n");
	fprintf(fp,"\t\t</relationship>\n");

	fprintf(fp, "\t</object>\n");

	fprintf(fp, "\t<event>\n");
	fprintf(fp, "\t\t<eventIdentifier>\n");
	fprintf(fp, "\t\t\t<eventIdentifierType>AEO-Light</eventIdentifierType>\n");
	fprintf(fp, "\t\t\t<eventIdentifierValue>1</eventIdentifierValue>\n");
	fprintf(fp, "\t\t</eventIdentifier>\n");
	fprintf(fp, "\t\t<eventType>creation</eventType>\n");
	fprintf(fp, "\t\t<eventDateTime>%s</eventDateTime>\n",
			QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss").
			toStdString().c_str());
	fprintf(fp, "\t</event>\n");

}

//=============================================================================

void write_mods_xml(const char *fn, const Frame_Window &project,
		const FilmScan &inFile, const char *soundFile,
		long firstFrame, long numFrames)
{
	FILE *fp = fopen(fn, "w");

	// XML header
	fprintf(fp,"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	fprintf(fp,"<modsCollection xmlns:xlink=\"http://www.w3.org/1999/xlink\" "
			"xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
			"xmlns=\"http://www.loc.gov/mods/v3\" "
			"xsi:schemaLocation=\"http://www.loc.gov/mods/v3 "
			"http://www.loc.gov/standards/mods/v3/mods-3-3.xsd\">\n");
	fprintf(fp,"<mods version=\"3.3\">\n");

	// the exported file
	QFileInfo qinfo(soundFile);

	FileInfo f2info;

	f2info.filename = qinfo.fileName().toStdString();
	f2info.ext = qinfo.suffix().toLower().toStdString();
	f2info.path = qinfo.path().toStdString();
	f2info.bytes = qinfo.size();

	f2info.duration = project.duration;
	f2info.bitdepth = project.bit_depth;
	switch(f2info.bitdepth)
	{
	case 8:	f2info.codec = "pcm_u8"; break;
	case 16: f2info.codec = "pcm_s16le"; break;
	case 24: f2info.codec = "pcm_s24le"; break;
	default: f2info.codec = "pcm";
	}

	f2info.samplerate = project.sampling_rate;

	file_mods_xml(fp,f2info);

	// XML Footer
	fprintf(fp,"</mods>\n");
	fprintf(fp,"</modsCollection>\n");

	fclose(fp);

}

//=============================================================================
void file_mods_xml(FILE *fp, const FileInfo &finfo)
{
	// Date created
	fprintf(fp,"\t<originInfo>\n");
	fprintf(fp,"\t\t<dateCreated>%s</dateCreated>\n",
			QDateTime::currentDateTime().toString("yyyyMMdd").
			toStdString().c_str());
	fprintf(fp,"\t</originInfo>\n");

	// File size
	fprintf(fp,"\t<physicalDescription>\n");
	fprintf(fp,"\t\t<extent unit=\"bytes\">%lu</extent>\n", finfo.bytes);
	fprintf(fp,"\t</physicalDescription>\n");

	// Duration
	fprintf(fp,"\t<physicalDescription>\n");
	fprintf(fp,"\t\t<extent unit=\"seconds\">%f</extent>\n",
			double(finfo.duration)/100.0);
	fprintf(fp,"\t</physicalDescription>\n");

	// Type of media (sound)
	fprintf(fp,"\t<typeOfResource>sound recording</typeOfResource>\n");

	// File format
	fprintf(fp,"\t<physicalDescription>\n");
	fprintf(fp,"\t\t<internetMediaType>audio/%s</internetMediaType>\n",
			finfo.ext.c_str());
	fprintf(fp,"\t</physicalDescription>\n");

	// Bit depth
	fprintf(fp,"\t<physicalDescription>\n");
	fprintf(fp,"\t\t<note displayLabel=\"bit depth\">%lu</note>\n",
			finfo.bitdepth);
	fprintf(fp,"\t</physicalDescription>\n");

	fprintf(fp,"\t<physicalDescription>\n");
	fprintf(fp,"\t\t<note displayLabel=\"codec\">%s</note>\n",
			finfo.codec.c_str());
	fprintf(fp,"\t</physicalDescription>\n");

	// Sampling rate
	fprintf(fp,"\t<physicalDescription>\n");
	fprintf(fp,"\t\t<note displayLabel=\"sampling rate\">%lu</note>\n",
			finfo.samplerate);
	fprintf(fp,"\t</physicalDescription>\n");

	// Location
	fprintf(fp,"\t<location displayLabel=\"file path\">%s/%s</location>\n",
			finfo.path.c_str(), finfo.filename.c_str());
}
