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
    ui->importText->setText(settings->value("import", sysRead).toString());
    settings->endGroup();

	ui->sourceText->setPlaceholderText(sysRead);
	ui->projectText->setPlaceholderText(sysWrite);
    ui->exportText->setPlaceholderText(sysWrite);
    ui->importText->setPlaceholderText(sysRead);

    connect(
        ui->browseForSourceButton, &QPushButton::clicked,
        this, [this](){
            BrowseForFolder(
                ui->sourceText,
                "Default Source Scan Folder",
                sysRead);
            }
        );
    connect(
        ui->browseForProjectButton, &QPushButton::clicked,
        this, [this](){
            BrowseForFolder(
                ui->projectText,
                "Default AEO Project File Folder",
                sysWrite);
        }
        );
    connect(
        ui->browseForExportButton, &QPushButton::clicked,
        this, [this](){
            BrowseForFolder(
                ui->exportText,
                "Default Export Folder",
                sysWrite);
        }
        );
    connect(
        ui->browseForImportButton, &QPushButton::clicked,
        this, [this](){
            BrowseForFolder(
                ui->importText,
                "Default Import Folder",
                sysRead);
        }
        );

    connect(
        ui->sourceText, &QLineEdit::editingFinished,
        this, [this](){ ValidateFolder(sysRead); } );
    connect(
        ui->projectText, &QLineEdit::editingFinished,
        this, [this](){ ValidateFolder(sysWrite); } );
    connect(
        ui->exportText, &QLineEdit::editingFinished,
        this, [this](){ ValidateFolder(sysWrite); } );
    connect(
        ui->importText, &QLineEdit::editingFinished,
        this, [this](){ ValidateFolder(sysRead); } );


    connect(
        ui->discardButton, &QPushButton::clicked,
        this, &preferencesdialog::reject);
    connect(
        ui->saveButton, &QPushButton::clicked,
        this, &preferencesdialog::Save);
}

preferencesdialog::~preferencesdialog()
{
	settings->sync();
	delete settings;

	delete ui;
}

void preferencesdialog::BrowseForFolder(
    QLineEdit *lineEdit, QString title, QString dflt)
{
    QString curdir = lineEdit->text();
    if(curdir.isEmpty()) curdir = dflt;

    QString dir = QFileDialog::getExistingDirectory(this, title, curdir);
    if(dir.isEmpty()) return;

    lineEdit->setText(dir);
}

void preferencesdialog::ValidateFolder(QString dflt)
{
    QLineEdit *w = qobject_cast<QLineEdit *>(sender());
    if(!w) return;

    if(w->text().isEmpty())
    {
        w->setText(dflt);
        return;
    }

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


void preferencesdialog::Save()
{
	settings->beginGroup("default-folder");
	settings->setValue("source",ui->sourceText->text());
	settings->setValue("project",ui->projectText->text());
    settings->setValue("export",ui->exportText->text());
    settings->setValue("import",ui->importText->text());
    settings->endGroup();

	settings->beginGroup("audio-metadata");
	settings->setValue("originator", ui->originatorText->text());
	settings->setValue("archive-location", ui->archiveLocationText->text());
	settings->setValue("copyright", ui->copyrightText->text());
	settings->endGroup();

	accept();
	//done(Accepted);
}
