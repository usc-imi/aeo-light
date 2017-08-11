#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <QAbstractButton>
#include <QSettings>
#include <QDialog>

namespace Ui {
class preferencesdialog;
}

class preferencesdialog : public QDialog
{
	Q_OBJECT

public:
	explicit preferencesdialog(QWidget *parent = 0);
	~preferencesdialog();

private slots:
	void on_browseForSourceButton_clicked();

	void on_browseForProjectButton_clicked();

	void on_browseForExportButton_clicked();

	void on_sourceText_editingFinished();

	void on_discardButton_clicked();

	void on_saveButton_clicked();

	void on_projectText_editingFinished();

	void on_exportText_editingFinished();

private:
	Ui::preferencesdialog *ui;
	QString sysRead;
	QString sysWrite;
	QSettings *settings;
};

#endif // PREFERENCESDIALOG_H
