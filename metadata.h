#ifndef METADATA_H
#define METADATA_H

#include <QString>

class MetaData
{
public:
	MetaData();

public:
	QString originator;
	QString originatorReference;
	QString description;
	uint16_t version;
	uint64_t timeReference;
	QString codingHistory; // multi-line OK
	QString archivalLocation;
	QString title;
	QString comment;
	QString copyright;
};

#endif // METADATA_H
