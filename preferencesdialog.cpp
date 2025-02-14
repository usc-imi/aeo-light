//-----------------------------------------------------------------------------
// This file is part of AEO-Light
//
// Copyright (c) 2016-2025 University of South Carolina
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
#include "preferencesdialog.h"
#include "ui_preferencesdialog.h"

#include <QFileDialog>
#include <QStandardPaths>
#include <QSettings>
#include <QDebug>
#include <QMessageBox>

preferencesdialog::preferencesdialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::preferencesdialog)
{
	ui->setupUi(this);

	settings = new QSettings;

	QStringList l;

	l = QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation);

	if(l.size()) sysRead = l.at(0);
	else sysRead = "/";

	sysWrite = QStandardPaths::writableLocation(
			QStandardPaths::DocumentsLocation);

	settings->beginGroup("default-folder");
	ui->sourceText->setText(settings->value("source", sysRead).toString());
	ui->projectText->setText(settings->value("project", sysWrite).toString());
	ui->exportText->setText(settings->value("export", sysWrite).toString());
	settings->endGroup();

	ui->sourceText->setPlaceholderText(sysRead);
	ui->projectText->setPlaceholderText(sysWrite);
	ui->exportText->setPlaceholderText(sysWrite);
}

preferencesdialog::~preferencesdialog()
{
	settings->sync();
	delete settings;

	delete ui;
}

void preferencesdialog::on_browseForSourceButton_clicked()
{
	QString curdir = ui->sourceText->text();
	if(curdir.isEmpty()) curdir = sysRead;

	QString dir = QFileDialog::getExistingDirectory(this,
			"Default Source Scan Folder", curdir);

	if(dir.isEmpty()) return;

	ui->sourceText->setText(dir);
}

void preferencesdialog::on_browseForProjectButton_clicked()
{
	QString curdir = ui->projectText->text();
	if(curdir.isEmpty()) curdir = sysWrite;

	QString dir = QFileDialog::getExistingDirectory(this,
			"Default AEO Project File Folder", curdir);

	if(dir.isEmpty()) return;

	ui->projectText->setText(dir);
}

void preferencesdialog::on_browseForExportButton_clicked()
{
	QString curdir = ui->exportText->text();
	if(curdir.isEmpty()) curdir = sysWrite;

	QString dir = QFileDialog::getExistingDirectory(this,
			"Default Export Folder", curdir);

	if(dir.isEmpty()) return;

	ui->exportText->setText(dir);
}

void ValidateDirectory(QLineEdit *w)
{
	// turn off signals while we handle the message dialogs. This is a
	// workaround for the known QT bug wherein editingFinished is called
	// twice when a messagebox is shown:
	// https://bugreports.qt.io/browse/QTBUG-40
	w->blockSignals(true);

	QString txt = w->text();

	// for preference-file specs, treat all entries as absolute paths
	if(QDir(txt).isRelativePath(txt))
	{
		txt = QDir().rootPath() + txt;
		w->setText(txt);
	}

	if(!QDir(w->text()).exists())
	{
		QMessageBox msgBox;
		msgBox.setWindowTitle("Folder doesn't exist");
		msgBox.setText(QString("The folder '%1' doesn't exist.").arg(txt));
		msgBox.addButton("Create Folder", QMessageBox::AcceptRole);
		msgBox.addButton(QMessageBox::Cancel);
		int ret = msgBox.exec();
		if(ret != QMessageBox::Cancel)
		{
			if(!QDir().mkpath(w->text()))
			{
				QMessageBox::warning(NULL, "Could not create",
						"Could not create directory");
				ret = QMessageBox::Cancel;
			}
		}

		if(ret == QMessageBox::Cancel)
		{
			w->setFocus();
			w->selectAll();
		}
	}

	w->blockSignals(false);
}

void preferencesdialog::on_sourceText_editingFinished()
{
	if(ui->sourceText->text().isEmpty())
		ui->sourceText->setText(sysRead);
	else
		ValidateDirectory(ui->sourceText);
}

void preferencesdialog::on_projectText_editingFinished()
{
	if(ui->projectText->text().isEmpty())
		ui->projectText->setText(sysWrite);
	else
		ValidateDirectory(ui->projectText);
}

void preferencesdialog::on_exportText_editingFinished()
{
	if(ui->exportText->text().isEmpty())
		ui->exportText->setText(sysWrite);
	else
		ValidateDirectory(ui->exportText);
}

void preferencesdialog::on_discardButton_clicked()
{
	reject();
	//done(Rejected);
}

void preferencesdialog::on_saveButton_clicked()
{
	settings->beginGroup("default-folder");
	settings->setValue("source",ui->sourceText->text());
	settings->setValue("project",ui->projectText->text());
	settings->setValue("export",ui->exportText->text());
	settings->endGroup();

	settings->beginGroup("audio-metadata");
	settings->setValue("originator", ui->originatorText->text());
	settings->setValue("archive-location", ui->archiveLocationText->text());
	settings->setValue("copyright", ui->copyrightText->text());
	settings->endGroup();

	accept();
	//done(Accepted);
}
