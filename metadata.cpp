#include "metadata.h"

#include <QSettings>

MetaData::MetaData()
{
	QSettings settings;
	settings.beginGroup("audio-metadata");
	this->originator = settings.value("originator").toString();
	this->archivalLocation = settings.value("archive-location").toString();
	this->copyright = settings.value("copyright").toString();
	settings.endGroup();
	version = 2;
}
