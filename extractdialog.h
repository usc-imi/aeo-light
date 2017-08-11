#ifndef EXTRACTDIALOG_H
#define EXTRACTDIALOG_H

#include <QDialog>
#include "metadata.h"

namespace Ui {
class ExtractDialog;
}

class ExtractDialog : public QDialog
{
	Q_OBJECT

public:
	explicit ExtractDialog(QWidget *parent, MetaData &metadata,
			const QString &dir);
	~ExtractDialog();

private:
	Ui::ExtractDialog *ui;
	uint16_t version;
	MetaData *meta;
	QString defaultDir;
	bool requestRestart;
	bool videoIsRisky;

public:
	void MarkVideoAsRisky(void);
	bool RequestedRestart(void) {return requestRestart; };

	QString GetFilename(void);
	QString GetVideoFilename(void);
	QString GetOriginator(void);
	QString GetOriginatorReference(void);
	QString GetDescription(void);
	uint16_t GetVersion(void);
	QByteArray GetUMID(void);
	QString GetCodingHistory(void);
	QString GetArchiveLocation(void);
	QString GetSubjectInfo(void);
	QString GetCopyrightInfo(void);

	void SetFilename(const QString &filename);
	void SetOriginator(const QString &originator);
	void SetOriginatorReference(const QString &originatorReference);
	void SetDescription(const QString &description);
	void SetOriginationDateStr(const QString &datestr);
	void SetVersion(uint16_t version);
	void SetTimeReferenceStr(const QString &timecode);
	void SetCodingHistory(const QString &codingHistory);
	void SetArchiveLocation(const QString &archiveLocation);
	void SetSubjectInfo(const QString &subjectInfo);
	void SetCopyrightInfo(const QString &copyrightInfo);

private slots:
	void on_cancelButton_clicked();
	void on_okButton_clicked();
	void on_fileBrowseButton_clicked();
	void on_muxVideoCheckbox_clicked(bool checked);
	void on_fileVideoBrowseButton_clicked();
};

#endif // EXTRACTDIALOG_H
