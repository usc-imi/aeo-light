#include "extractdialog.h"
#include "ui_extractdialog.h"

#include <QDateTime>
#include <QFileDialog>
#include <QDebug>
#include <QMessageBox>

ExtractDialog::ExtractDialog(QWidget *parent, MetaData &metadata,
			const QString &dir) :
	QDialog(parent),
	ui(new Ui::ExtractDialog)
{
	ui->setupUi(this);

	meta = &metadata;
	defaultDir = dir;
	requestRestart = false;
	videoIsRisky = false;

	SetOriginator(meta->originator);
	SetArchiveLocation(meta->archivalLocation);
	SetCopyrightInfo(meta->copyright);
	SetVersion(meta->version);
	SetCodingHistory(meta->codingHistory);
	SetTimeReferenceStr(QString("%L1").arg(meta->timeReference));

	ui->datetimeDisplay->setText(
			QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));

}

ExtractDialog::~ExtractDialog()
{
	delete ui;
}

void ExtractDialog::MarkVideoAsRisky(void)
{
	ui->muxVideoCheckbox->setChecked(false);
	on_muxVideoCheckbox_clicked(false);
	videoIsRisky = true;
}

QString ExtractDialog::GetFilename(void)
{
	return ui->filenameText->text();
}

QString ExtractDialog::GetVideoFilename(void)
{
	if(ui->muxVideoCheckbox->isChecked())
		return ui->filenameVideoText->text();
	else
		return QString();
}

QString ExtractDialog::GetOriginator(void)
{
	return ui->originatorDisplay->text();
}

QString ExtractDialog::GetOriginatorReference(void)
{
	return ui->originatorReferenceText->text();
}

QString ExtractDialog::GetDescription(void)
{
	return ui->descriptionText->text();
}

uint16_t ExtractDialog::GetVersion(void)
{
	return version;
}

QString ExtractDialog::GetCodingHistory(void)
{
	return ui->codingHistoryDisplay->text();
}

QString ExtractDialog::GetArchiveLocation(void)
{
	return ui->archiveLocationDisplay->text();
}

QString ExtractDialog::GetSubjectInfo(void)
{
	return ui->subjectText->text();
}

QString ExtractDialog::GetCopyrightInfo(void)
{
	return ui->copyrightText->text();
}

void ExtractDialog::SetFilename(const QString &filename)
{
	ui->filenameText->setText(filename);
}

void ExtractDialog::SetOriginator(const QString &originator)
{
	ui->originatorDisplay->setText(originator);
}

void ExtractDialog::SetOriginatorReference(const QString &originatorReference)
{
	ui->originatorReferenceText->setText(originatorReference);
}

void ExtractDialog::SetDescription(const QString &description)
{
	ui->descriptionText->setText(description);
}

void ExtractDialog::SetOriginationDateStr(const QString &datestr)
{
	ui->datetimeDisplay->setText(datestr);
}

void ExtractDialog::SetVersion(uint16_t version)
{
	ui->versionDisplay->setText(QString("%1h").arg(version, 4, 16, QChar('0')));
}

void ExtractDialog::SetTimeReferenceStr(const QString &timecode)
{
	ui->timecodeDisplay->setText(timecode);
}

void ExtractDialog::SetCodingHistory(const QString &codingHistory)
{
	ui->codingHistoryDisplay->setText(codingHistory);
}

void ExtractDialog::SetArchiveLocation(const QString &archiveLocation)
{
	ui->archiveLocationDisplay->setText(archiveLocation);
}

void ExtractDialog::SetSubjectInfo(const QString &subjectInfo)
{
	ui->subjectText->setText(subjectInfo);
}

void ExtractDialog::SetCopyrightInfo(const QString &copyrightInfo)
{
	ui->copyrightText->setText(copyrightInfo);
}

void ExtractDialog::on_cancelButton_clicked()
{
	reject();
}

void ExtractDialog::on_okButton_clicked()
{
	meta->originator = ui->originatorDisplay->text();
	meta->originatorReference = ui->originatorReferenceText->text();
	meta->comment = ui->subjectText->text();
	meta->copyright = ui->copyrightText->text();

	accept();
}

static QString SaveFile(QWidget *parent, QString label, QString defaultPath,
		QString current, QString ext)
{
	QString sel;

	if(!current.isEmpty())
	{
		if(QFileInfo(current).isRelative())
			sel = defaultPath + "/" + current;
		else
			sel = current;
	}
	else
	{
		sel = defaultPath;
	}

	return QFileDialog::getSaveFileName(parent, label, sel, ext);
}

void ExtractDialog::on_fileBrowseButton_clicked()
{
	QString filename = SaveFile(
			this, tr("Export audio to"), defaultDir,
			ui->filenameText->text(), "*.wav");

	if(filename.isEmpty()) return;

	ui->filenameText->setText(filename);

	if(ui->filenameVideoText->text().isEmpty())
	{
		QFileInfo fi(filename);
		ui->filenameVideoText->setText(
				fi.dir().path() + "/" + fi.completeBaseName() + ".mp4");
	}
}

void ExtractDialog::on_muxVideoCheckbox_clicked(bool checked)
{
	if(checked && videoIsRisky)
	{
		QMessageBox w(QMessageBox::Warning,
				tr("Video Instability Warning"),
				tr("The video creation code is unstable and may crash "
				   "the program when creating a video a second time.\n\n"
				   "Restart AEO-Light to avoid this."),
				QMessageBox::Cancel | QMessageBox::Ignore | QMessageBox::Abort,
				this);
		int answer;

		w.button(QMessageBox::Abort)->setText("Restart");
		w.setDefaultButton(QMessageBox::Cancel);

		answer = w.exec();

		if(answer == QMessageBox::Abort)
		{
			this->requestRestart = true;
			this->done(this->Rejected);
			return;
		}
		else if(answer == QMessageBox::Cancel)
		{
			ui->muxVideoCheckbox->setChecked(false);
			return;
		}
	}
	ui->filenameVideoText->setEnabled(checked);
	ui->fileVideoBrowseButton->setEnabled(checked);
}

void ExtractDialog::on_fileVideoBrowseButton_clicked()
{
	QString filename = SaveFile(this, tr("Save video as"), defaultDir,
			ui->filenameVideoText->text(), "*.mp4");

	if(filename.isEmpty()) return;

	ui->filenameVideoText->setText(filename);
}
