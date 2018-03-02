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

#include <algorithm>

#include <QApplication>
#include <QByteArray>
#include <QProcess>
#include <QDebug>
#include <QSettings>
#include <QVersionNumber>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTimer>
#include <QElapsedTimer>
#include <QMessageBox>
#include <QDesktopServices>
#include <QInputDialog>
#include <QSound>
#include <QFile>
#include <QMouseEvent>
#include <QDateTime>
#include <QTemporaryFile>
#include <QScrollArea>
#include <QProgressDialog>

#include <cstdio>
#include <exception>

#include <fcntl.h>
#include <stdlib.h>

#include <csetjmp>
#include <csignal>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "wav.h"
#include "writexml.h"
#include "project.h"
#include "aeoexception.h"

#include "mainwindow.h"
#include "savesampledialog.h"
#include "preferencesdialog.h"
#include "extractdialog.h"

#ifdef USE_MUX_HACK
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
extern "C"
{
#include <libavutil/avassert.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

// The av_err2str(n) macro uses the C99 compound literal construct, which
// MSVC does not understand.
// Ref: https://ffmpeg.zeranoe.com/forum/viewtopic.php?t=4220
// Ref: https://stackoverflow.com/questions/3869963/compound-literals-in-msvc
#ifdef _WIN32
#define aeo_av_err2str(n) "MSVC Cannot Show This Error"
#else
//#define aeo_av_err2str(n) av_err2str(n)
#define aeo_av_err2str(errnum) \
	av_make_error_string((char*)__builtin_alloca(AV_ERROR_MAX_STRING_SIZE), \
	AV_ERROR_MAX_STRING_SIZE, errnum)
#endif


#endif // ifdef USE_MUX_HACK

extern "C" void SegvHandler(int param);
static jmp_buf segvJumpEnv;
static long lastFrameLoad = 0;
static const char *traceCurrentOperation = NULL;
static const char *traceSubroutineOperation = NULL;

#ifndef UMAX
	#define UMAX(b) ((1ull<<(b))-1)
#endif
#ifndef SMAX
	#define SMAX(b) (UMAX((b)-1))
#endif

//----------------------------------------------------------------------------
ExtractedSound::ExtractedSound() :
	frameIn(0),
	frameOut(0),
	gamma(100),
	gain (100),
	sCurve(0),
	overlap(20),
	lift(0),
	blur(0),
	fpsType(FPS_24),
	useBounds(true),
	usePixBounds(false),
	useSCurve(false),
	makeNegative(false),
	makeGray(false),
	sound(NULL),
	err(0)
{
	bounds[0] = bounds[1] = 0;
	pixBounds[0] = pixBounds[1] = 0;
	framePitch[0] = framePitch[1] = 0;
}

ExtractedSound::~ExtractedSound()
{
	//let the deque handle this: too many copies being made to
	// handle here. We'd need to switch to a smart pointer
#if 0
	if(sound != NULL)
	{
		sound->stop();
		QFile::remove(sound->fileName());
	}
#endif
}

bool ExtractedSound::operator ==(const ExtractedSound &ref) const
{
	if(frameIn != ref.frameIn) return false;
	if(frameOut != ref.frameOut) return false;
	if(framePitch[0] != ref.framePitch[0]) return false;
	if(framePitch[1] != ref.framePitch[1]) return false;
	if(overlap != ref.overlap) return false;

	if(useBounds != ref.useBounds) return false;
	if(usePixBounds != ref.usePixBounds) return false;
	if(useBounds &&
			((bounds[0] != ref.bounds[0]) || (bounds[1] != ref.bounds[1])))
		return false;
	if(usePixBounds &&
			((pixBounds[0] != ref.pixBounds[0]) ||
			(pixBounds[1] != ref.pixBounds[1])))
		return false;

	if(gamma != ref.gamma) return false;
	if(gain != ref.gain) return false;
	if(lift != ref.lift) return false;
	if(blur != ref.blur) return false;

	if(useSCurve != ref.useSCurve) return false;
	if(useSCurve && (sCurve != ref.sCurve)) return false;

	if(fpsType != ref.fpsType) return false;

	if(makeNegative != ref.makeNegative) return false;
	if(makeGray != ref.makeGray) return false;

	return true;
}

//----------------------------------------------------------------------------

#define TRANSSLIDER_VALUE 200


QString MainWindow::Version()
{
	// Version number is set in the .pro file
	return QCoreApplication::applicationName() + QString(" v") +
			QCoreApplication::applicationVersion() /* + " (" __DATE__ ")"*/;
}

void RecursivelyEnable(QLayout *l, bool enable=true)
{
	int i;
	QLayoutItem *item;

	for(i=0; i<l->count(); ++i)
	{
		item = l->itemAt(i);
		if(item)
		{
			if(item->widget()) item->widget()->setEnabled(enable);
			else if(item->layout()) RecursivelyEnable(item->layout(), enable);
		}
	}
}

// -------------------------------------------------------------------
// Constructor
// -------------------------------------------------------------------

MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent),
	ui(new Ui::MainWindow)
{
	ui->setupUi(this);

	#ifdef _WIN32
	QString fontsize = "18pt";
	#else
	QString fontsize = "24pt";
	#endif

	ui->actionAbout->setMenuRole(QAction::AboutRole);
	ui->actionQuit->setMenuRole(QAction::QuitRole);
	ui->actionPreferences->setMenuRole(QAction::PreferencesRole);

	#ifdef AVAILABLE_MAC_OS_X_VERSION_10_11_AND_LATER
	// As of 10.11 (El Capitan), OSX automagically adds "Enter Full Screen"
	// to any menu named "View", so we have to name it something else in
	// order to avoid this undesired behavior.
	// Here we append a zero-width space character.
	ui->menuView->setTitle("View\u200C");
	#endif

	ui->appNameLabel->setText(
			QString("<html><head/><body><p><span style=\" font-size:"
			+ fontsize + ";\">")
			+ Version() + QString("</span></p></body></html>"));

	ui->FramePitchendSlider->setValue(this->scan.overlapThreshold);

	ui->maxFrequencyFrame->setVisible(false);
	ui->tabWidget->setCurrentIndex(0);
	//ui->soundtrackSettingGrid->setRowStretch(0,1);
	frame_window = NULL;
	paramCopyLock = false;
	samplesPlayed.resize(4);
	currentMeta = NULL;
	currentFrameTexture = NULL;
	outputFrameTexture = NULL;

	// turn off stuff that can't be used until a project is loaded
	ui->saveprojectButton->setEnabled(false);
	ui->actionSave_Settings->setEnabled(false);
	ui->actionShow_Overlap->setEnabled(false);
	ui->actionShow_Soundtrack_Only->setEnabled(false);
	ui->actionWaveform_Zoom->setEnabled(false);
	ui->loadSettingsButton->setEnabled(false);
	ui->tabWidget->setEnabled(false);
	RecursivelyEnable(ui->viewOptionsLayout, false);
	RecursivelyEnable(ui->frameNumberLayout, false);
	RecursivelyEnable(ui->sampleLayout, false);

	// skip license agreement if the user has already agreed to this version
	QSettings settings;
	QString ag = settings.value("license","0.1").toString();
	QVersionNumber agv = QVersionNumber::fromString(ag);
	QVersionNumber thisv = QVersionNumber::fromString(APP_VERSION);

	Log() << "Starting project: " << startingProjectFilename << "\n";

	if(QVersionNumber::compare(agv, thisv) < 0)
		QTimer::singleShot(0, this, SLOT(LicenseAgreement()));
	else
		QTimer::singleShot(0, this, SLOT(OpenStartingProject()));

	prevProjectDir = "";

	// start log with timestamp
	Log() << QDateTime::currentDateTime().toString() << "\n";

	isVideoMuxingRisky = false;

	#ifdef USE_MUX_HACK
	encStartFrame = 0;
	encNumFrames = 0;
	encCurFrame = 0;

	encVideoBufSize = 0;

	encRGBFrame = NULL;
	encS16Frame = NULL;

	encAudioLen = 0;
	encAudioNextPts = 0;
	#endif
}

MainWindow::~MainWindow()
{
	DeleteTempSoundFile();
	delete ui;
}

void MainWindow::SaveDefaultsSoundtrack()
{
	QSettings settings;

	if(!scan.inFile.IsReady()) return;

	double w = scan.inFile.Width();
	double h = scan.inFile.Height();

	settings.beginGroup("soundtrack");
	settings.setValue("bounds/left", ui->leftSpinBox->value()/w);
	settings.setValue("bounds/right", ui->rightSpinBox->value()/w);
	settings.setValue("bounds/use", ui->OverlapSoundtrackCheckBox->isChecked());
	settings.setValue("pixbounds/left", ui->leftPixSpinBox->value()/w);
	settings.setValue("pixbounds/right", ui->rightPixSpinBox->value()/w);
	settings.setValue("pixbounds/use", ui->OverlapPixCheckBox->isChecked());
	settings.setValue("framepitch/start", ui->framepitchstartSlider->value()/h);
	settings.setValue("framepitch/end", ui->FramePitchendSlider->value()/h);
	settings.setValue("overlap/radius", ui->overlapSlider->value());
	settings.setValue("overlap/lock", ui->HeightCalculateBtn->isChecked());
	settings.setValue("isstereo", ui->monostereo_PD->currentIndex());
	settings.endGroup();
}

void MainWindow::SaveDefaultsImage()
{
	QSettings settings;

	settings.beginGroup("image");
	settings.setValue("lift", ui->liftSlider->value());
	settings.setValue("gamma", ui->gammaSlider->value());
	settings.setValue("gain", ui->gainSlider->value());
	settings.setValue("s-curve", ui->thresholdSlider->value());
	settings.setValue("s-curve-on", ui->threshBox->isChecked());
	settings.setValue("blur-sharp", ui->blurSlider->value());
	settings.setValue("negative", ui->negBox->isChecked());
	settings.setValue("desaturate",ui->desatBox->isChecked());
	settings.endGroup();
}

void MainWindow::SaveDefaultsAudio()
{
	QSettings settings;

	settings.beginGroup("extraction");
	settings.setValue("sampling-rate", ui->filerate_PD->currentText());
	settings.setValue("bit-depth", ui->bitDepthComboBox->currentText());
	settings.setValue("time-code-advance", ui->advance_CB->currentText());
	settings.setValue("sidecar", ui->xmlSidecarComboBox->currentText());
	settings.endGroup();
}

void MainWindow::LoadDefaults()
{
	QSettings settings;
	int intv;
	double doublev;

	if(!scan.inFile.IsReady()) return;

	int w = scan.inFile.Width();
	int h = scan.inFile.Height();

	// Soundtrack Settings
	settings.beginGroup("soundtrack");

	intv = int(settings.value("bounds/left",0).toDouble() * w + 0.5);
	ui->leftSpinBox->setValue(intv);
	ui->leftSlider->setValue(intv);
	intv = int(settings.value("bounds/right",0).toDouble() * w + 0.5);
	ui->rightSpinBox->setValue(intv);
	ui->rightSlider->setValue(intv);
	ui->OverlapSoundtrackCheckBox->setChecked(
			settings.value("bounds/use",true).toBool());

	intv = int(settings.value("pixbounds/left",0.475).toDouble() * w + 0.5);
	ui->leftPixSpinBox->setValue(intv);
	ui->leftPixSlider->setValue(intv);
	intv = int(settings.value("pixbounds/right",0.525).toDouble() * w + 0.5);
	ui->rightPixSpinBox->setValue(intv);
	ui->rightPixSlider->setValue(intv);
	ui->OverlapPixCheckBox->setChecked(
			settings.value("pixbounds/use", false).toBool());
	on_OverlapPixCheckBox_clicked(ui->OverlapPixCheckBox->isChecked());

	intv = int(settings.value("framepitch/start", 0.1).toDouble() * h + 0.5);
	ui->framepitchstartSlider->setValue(intv);
	ui->framepitchstartLabel->setText(QString::number(intv/1000.f,'f',2));
	intv = int(settings.value("framepitch/end", 0.1).toDouble() * h + 0.5);
	ui->FramePitchendSlider->setValue(intv);
	ui->framePitchLabel->setText(QString::number(intv/1000.f,'f',2));

	intv = settings.value("overlap/radius",5).toInt();
	ui->overlapSlider->setValue(intv);
	ui->overlap_label->setText(QString::number(intv/100.f,'f',2));

	ui->HeightCalculateBtn->setChecked(settings.value("overlap/lock").toBool());
	ui->monostereo_PD->setCurrentIndex(settings.value("isstereo",0).toInt());

	settings.endGroup();

	// image processing settings
	settings.beginGroup("image");

	intv = settings.value("lift",0).toInt();
	ui->liftSlider->setValue(intv);
	ui->liftLabel->setText(QString::number(intv/100.f,'f',2));

	intv = settings.value("gamma",100).toInt();
	ui->gammaSlider->setValue(intv);
	ui->gammaLabel->setText(QString::number(intv/100.f,'f',2));

	intv = settings.value("gain", 100).toInt();
	ui->gainSlider->setValue(intv);
	ui->gainLabel->setText(QString::number(intv/100.f,'f',2));

	intv = settings.value("s-curve",300).toInt();
	ui->thresholdSlider->setValue(intv);
	ui->threshLabel->setText(QString::number(intv/100.f,'f',2));

	ui->threshBox->setChecked(settings.value("s-curve-on",false).toBool());

	intv = settings.value("blur-sharp", 0).toInt();
	ui->blurSlider->setValue(intv);
	ui->blurLabel->setText(QString::number(intv/100.f,'f',2));

	ui->negBox->setChecked(settings.value("negative",false).toBool());
	ui->desatBox->setChecked(settings.value("desaturate",false).toBool());
	settings.endGroup();

	// extraction/export parameters

	settings.beginGroup("extraction");
	QString txt;

	txt = settings.value("sampling-rate").toString();
	intv = ui->filerate_PD->findText(txt, Qt::MatchFixedString);
	if(intv != -1) ui->filerate_PD->setCurrentIndex(intv);

	txt = settings.value("bit-depth").toString();
	intv = ui->bitDepthComboBox->findText(txt, Qt::MatchFixedString);
	if(intv != -1) ui->bitDepthComboBox->setCurrentIndex(intv);

	txt = settings.value("time-code-advance").toString();
	intv = ui->advance_CB->findText(txt, Qt::MatchFixedString);
	if(intv != -1) ui->advance_CB->setCurrentIndex(intv);

	txt = settings.value("sidecar").toString();
	intv = ui->xmlSidecarComboBox->findText(txt, Qt::MatchFixedString);
	if(intv != -1) ui->xmlSidecarComboBox->setCurrentIndex(intv);

	settings.endGroup();

}

void MainWindow::LicenseAgreement()
{
	QMessageBox msg(this);
	QString lic;
	QFile licFile(":/LICENSE.txt");

	msg.setText("License Agreement");
	msg.setInformativeText(
			"-------------------------------------------------------------------------------------------------\n"
			"Copyright (c) 2015-2018 South Carolina Research Foundation\n"
			"All Rights Reserved\n"
			"See [Details] below for the full license agreement.\n");

	if(!licFile.open(QIODevice::ReadOnly))
	{
		exit(1);
	}

	QTextStream licStream(&licFile);
	lic = licStream.readAll();
	licFile.close();

	msg.setDetailedText(lic);

	msg.addButton("Accept Licence", QMessageBox::AcceptRole);
	msg.addButton(QMessageBox::Cancel);
	int ret = msg.exec();

	if(ret == QMessageBox::Cancel)
		exit(0);

	QSettings settings;
	settings.setValue("license",APP_VERSION);

	return;
}

void MainWindow::Render_Frame()
{
	if (frame_window!=NULL)
	{
		frame_window->renderNow();
	}
}

ExtractedSound MainWindow::ExtractionParamsFromGUI()
{
	ExtractedSound params;

	params.useBounds = ui->OverlapSoundtrackCheckBox->isChecked();
	params.usePixBounds = ui->OverlapPixCheckBox->isChecked();
	if(!params.usePixBounds) params.useBounds = true;

	if(params.useBounds)
	{
		params.bounds[0] = ui->leftSpinBox->value();
		params.bounds[1] = ui->rightSpinBox->value();
	}
	if(params.usePixBounds)
	{
		params.pixBounds[0] = ui->leftPixSpinBox->value();
		params.pixBounds[1] = ui->rightPixSpinBox->value();
	}

	params.framePitch[0] = ui->framepitchstartSlider->value();
	params.framePitch[1] = ui->FramePitchendSlider->value();

	params.frameIn = ui->frameInSpinBox->value();
	params.frameOut = ui->frameOutSpinBox->value();

	params.gamma = ui->gammaSlider->value();
	params.gain = ui->gainSlider->value();

	params.useSCurve = ui->threshBox->isChecked();
	if(params.useSCurve) params.sCurve = ui->thresholdSlider->value();

	params.overlap = ui->overlapSlider->value();
	params.lift = ui->liftSlider->value();
	params.blur = ui->blurSlider->value();

	switch(ui->filmrate_PD->currentIndex())
	{
	case 0: params.fpsType = FPS_NTSC; break;
	case 1: params.fpsType = FPS_24; break;
	case 2: params.fpsType = FPS_25; break;
	}

	params.makeNegative = ui->negBox->isChecked();
	params.makeGray = ui->desatBox->isChecked();

	return params;
}

//-----------------------------------------------------------------------------
// ExtractionParametersToGUI
// Copy values from the ExtractedSound params to the GUI controls
void MainWindow::ExtractionParametersToGUI(const ExtractedSound &params)
{
	ui->OverlapSoundtrackCheckBox->setChecked(params.useBounds);
	ui->OverlapPixCheckBox->setChecked(params.usePixBounds);
	on_OverlapPixCheckBox_clicked(ui->OverlapPixCheckBox->isChecked());

	if(params.useBounds)
	{
		ui->leftSpinBox->setValue(params.bounds[0]);
		ui->leftSlider->setValue(params.bounds[0]);
		ui->rightSpinBox->setValue(params.bounds[1]);
		ui->rightSlider->setValue(params.bounds[1]);
	}

	if(params.usePixBounds)
	{
		ui->leftPixSpinBox->setValue(params.pixBounds[0]);
		ui->leftPixSlider->setValue(params.pixBounds[0]);
		ui->rightPixSpinBox->setValue(params.pixBounds[1]);
		ui->rightPixSlider->setValue(params.pixBounds[1]);
	}

	ui->FramePitchendSlider->setValue(params.framePitch[1]);
	ui->framePitchLabel->setText(
				QString::number(params.framePitch[1]/1e3,'f',2));
	ui->framepitchstartSlider->setValue(params.framePitch[0]);
	ui->framepitchstartLabel->setText(
				QString::number(params.framePitch[0]/1e3,'f',2));

	ui->maxFrequencyLabel->setText(
				QString("%1").arg(float(this->MaxFrequency())/1000.0));

	ui->frameInSpinBox->setValue(params.frameIn);
	ui->frameOutSpinBox->setValue(params.frameOut);

	ui->gammaSlider->setValue(params.gamma);
	ui->gainSlider->setValue(params.gain);

	ui->threshBox->setChecked(params.useSCurve);
	if(params.useSCurve)
		ui->thresholdSlider->setValue(params.sCurve);

	ui->overlapSlider->setValue(params.overlap);
	ui->liftSlider->setValue(params.lift);
	ui->blurSlider->setValue(params.blur);

	switch(params.fpsType)
	{
	case FPS_NTSC: ui->filmrate_PD->setCurrentIndex(0); break;
	case FPS_24: ui->filmrate_PD->setCurrentIndex(1); break;
	case FPS_25: ui->filmrate_PD->setCurrentIndex(2); break;
	}

	ui->negBox->setChecked(params.makeNegative);
	ui->desatBox->setChecked(params.makeGray);

	GPU_Params_Update(1);
}

//-----------------------------------------------------------------------------
// GUI_Params_Update
// Copy values from the GPU settings in frame_window to the GUI controls
void MainWindow::GUI_Params_Update()
{
	// prevent a loop-back from GPU_Params_Update
	if(paramCopyLock) return;
	paramCopyLock = true;

	// If the OpenGL window isn't ready with a scan, skip the update
	if (frame_window!=NULL && this->scan.inFile.IsReady())
	{
		ui->leftSpinBox->setValue(
				frame_window->bounds[0] * this->scan.inFile.Width());
		ui->leftSlider->setValue(
				frame_window->bounds[0] * this->scan.inFile.Width());
		ui->rightSpinBox->setValue(
				frame_window->bounds[1] * this->scan.inFile.Width());
		ui->rightSlider->setValue(
				frame_window->bounds[1] * this->scan.inFile.Width());

		ui->leftPixSpinBox->setValue(
				frame_window->pixbounds[0] * this->scan.inFile.Width());
		ui->leftPixSlider->setValue(
				frame_window->pixbounds[0] * this->scan.inFile.Width());
		ui->rightPixSpinBox->setValue(
				frame_window->pixbounds[1] * this->scan.inFile.Width());
		ui->rightPixSlider->setValue(
				frame_window->pixbounds[1] * this->scan.inFile.Width());

		ui->FramePitchendSlider->setValue(
				frame_window->overlap[2] * 1000.0f);
		ui->framePitchLabel->setText(
				QString::number(frame_window->overlap[2],'f',2));
		ui->framepitchstartSlider->setValue(frame_window->overlap[3]*1000.0f);
		ui->framepitchstartLabel->setText(
				QString::number(frame_window->overlap[3],'f',2));
		ui->maxFrequencyLabel->setText(
				QString("%1").arg(float(this->MaxFrequency())/1000.0));
	}

	// release the lock
	paramCopyLock = false;
}

//-----------------------------------------------------------------------------
// GPU_Params_Update
// Copy values from the GUI controls to the GPU settings in frame_window
void MainWindow::GPU_Params_Update(bool renderyes)
{
	// prevent a loop-back from GUI_Params_Update
	if(paramCopyLock) return;
	paramCopyLock = true;

	// If the OpenGL window isn't ready with a scan, skip the update
	if (frame_window!=NULL && this->scan.inFile.IsReady())
	{
		frame_window->bounds[0] =
				ui->leftSpinBox->value()/float(this->scan.inFile.Width());
		frame_window->bounds[1] =
				ui->rightSpinBox->value()/float(this->scan.inFile.Width());

		frame_window->pixbounds[0] =
				ui->leftPixSpinBox->value()/float(this->scan.inFile.Width());
		frame_window->pixbounds[1] =
				ui->rightPixSpinBox->value()/float(this->scan.inFile.Width());

		frame_window->overlap[0] = TRANSSLIDER_VALUE/10000.0f;
		frame_window->overlap[1] = ui->overlapSlider->value()/100.0f;
		frame_window->overlap[2] = ui->FramePitchendSlider->value()/1000.0f;
		frame_window->overlap[3] = ui->framepitchstartSlider->value()/1000.0f;
		ui->maxFrequencyLabel->setText(
				QString("%1").arg(float(this->MaxFrequency())/1000.0));

		frame_window->gamma = ui->gammaSlider->value()/100.0f;
		frame_window->lift = ui->liftSlider->value()/100.0f;
		frame_window->gain = ui->gainSlider->value()/100.0f;
		frame_window->blur = ui->blurSlider->value()/100.0f;
		frame_window->threshold = ui->thresholdSlider->value()/100.0f;
		frame_window->thresh = ui->threshBox->checkState();
		frame_window->trackonly = ui->actionShow_Soundtrack_Only->isChecked();
		frame_window->negative = ui->negBox->checkState();
		frame_window->desaturate = ui->desatBox->checkState();
		frame_window->overlapshow = ui->actionShow_Overlap->isChecked();

		// Note: if none are checked, we still use the soundtrack (target=1)
		if(ui->OverlapPixCheckBox->isChecked())
		{
			if(ui->OverlapSoundtrackCheckBox->isChecked())
				frame_window->overlap_target = 2.0;
			else
				frame_window->overlap_target = 1.0;
		}
		else
			frame_window->overlap_target = 0.0;

		if (ui->actionWaveform_Zoom->isChecked())
			frame_window->WFMzoom=10.0f;
		else
			frame_window->WFMzoom=1.0f;

		if (renderyes)
			frame_window->renderNow();
	}

	// release the lock
	paramCopyLock = false;
}

//-----------------------------------------------------------------------------
bool MainWindow::Load_Frame_Texture(int frame_num)
{
	if (frame_window==NULL) return false;

	if(!frame_window->isExposed())
	{
		Log() << "Frame window not exposed. Attempting to raise.\n";

		frame_window->raise();
		frame_window->showNormal();
		frame_window->setWindowState(Qt::WindowActive);
		//qApp->setActiveWindow(frame_window);

		qApp->processEvents();

		while (!frame_window->isExposed())
		{
			Log() << "Frame window still not exposed. Asking for user help.\n";
			int raise = QMessageBox::question(this, "Select image Window",
					"The image window is obscured. Please move it to the front.",
					QMessageBox::Ok | QMessageBox::Cancel,
					QMessageBox::Ok);

			if(raise == QMessageBox::Cancel)
			{
				Log() << "User cancelled (could not raise window?)\n";
				LogClose();
				return false;
			}
		}
	}

	traceCurrentOperation = "Retrieving scan image";
	currentFrameTexture = this->scan.inFile.GetFrameImage(
			this->scan.inFile.FirstFrame()+frame_num, currentFrameTexture);
	traceCurrentOperation = "Loading scan into texture";
	frame_window->load_frame_texture(currentFrameTexture);

	/*
	traceCurrentOperation = "Freeing texture buffer";
	if (frameTex)
		delete frameTex;
	*/

	traceCurrentOperation = "GL render";
	frame_window->renderNow();
	traceCurrentOperation = "";

	// record frame in static variable for debugging/restarting from error:
	lastFrameLoad = frame_num;

	return true;
}

int MainWindow::MaxFrequency() const
{
	if(!this->frame_window) return -1;
	if(!this->scan.inFile.IsReady()) return -1;

	float fps;
	switch(this->ui->filmrate_PD->currentIndex())
	{
	case 0: fps = 23.976; break;
	case 1: fps = 24.000; break;
	case 2: fps = 25.000; break;
	}

	int nPitchLines = this->scan.inFile.Height() *
			(1 - frame_window->overlap[3] - frame_window->overlap[2]);

	return int(nPitchLines * fps / 2);
}

void MainWindow::on_changeButton_clicked()
{
	frame_window->close();
	delete frame_window;
}


void MainWindow::on_sourceButton_clicked()
{
	// Ask for input source

	static QString prevDir = "";
	QString srcDir;

	const struct {
		SourceFormat fileType;
		const char *filter;
	} fileFilterArr[] = {
		{ SOURCE_DPX,       "DPX frames (*.dpx)" },
		{ SOURCE_LIBAV,     "Video files (*.mp4 *.mov *.avi)" },
		{ SOURCE_WAV,       "Synthetic (*.wav)" },
		{ SOURCE_TIFF,      "TIFF frames (*.tif *.tiff)" },
		{ SOURCE_UNKNOWN,   "" }
	};

	int ft;

	QStringList fileTypeFilters;
	for(ft = 0; fileFilterArr[ft].fileType != SOURCE_UNKNOWN; ++ft)
		fileTypeFilters += tr(fileFilterArr[ft].filter);

	if(prevDir.isEmpty())
	{
		QSettings settings;
		settings.beginGroup("default-folder");
		srcDir = settings.value("source").toString();
		if(srcDir.isEmpty())
		{
			QStringList l;
			l = QStandardPaths::standardLocations(
						QStandardPaths::DocumentsLocation);
			if(l.size()) srcDir = l.at(0);
			else srcDir = "/";
		}
		settings.endGroup();
	}
	else
	{
		srcDir = prevDir;
	}

	QString selectedType = "";
	QString filename = QFileDialog::getOpenFileName(this,
			tr("Film Source"), srcDir,
			fileTypeFilters.join(";;"),
			&selectedType /* , QFileDialog::DontUseNativeDialog */ );

	if(filename.isEmpty()) return;

	prevDir = QFileInfo(filename).absolutePath();

	ft = fileTypeFilters.indexOf(selectedType);
	if(ft < 0) return;

	this->NewSource(filename, fileFilterArr[ft].fileType);
}

bool MainWindow::NewSource(QString filename, SourceFormat ft)
{
	//Set up longjmp to catch SIGSEGV and report what was going on at the
	// time of the segment violation.
	traceCurrentOperation = "";
	traceSubroutineOperation = "";
	int jmp = setjmp(segvJumpEnv);
	if(jmp!=0)
	{
		QMessageBox::critical(NULL,"Critical Failure",
				QString("Critical failure opening new source.\n%1\n%2").
						arg(traceCurrentOperation).
						arg(traceSubroutineOperation));

		std::exit(1);
	}

	void (*prevSegvHandler)(int);
	prevSegvHandler = std::signal(SIGSEGV, SegvHandler);

	try
	{
		traceCurrentOperation = "Opening Source";
		this->scan.SourceScan(filename.toStdString(), ft);
		traceCurrentOperation = "Verifying scan is ready";
		if(this->scan.inFile.IsReady())
		{
			if(frame_window)
			{
				traceCurrentOperation="Closing previous frame window";
				frame_window->currentOperation = &traceSubroutineOperation;
				frame_window->close();
				traceCurrentOperation="Deleting previous frame window";
				delete frame_window;
			}

			traceCurrentOperation = "Creating new frame window";
			frame_window = new Frame_Window(
						this->scan.inFile.Width(),this->scan.inFile.Height());

			frame_window->currentOperation = &traceSubroutineOperation;
			frame_window->setTitle(filename);
			frame_window->ParamUpdateCallback(&(this->GUI_Params_Update_Static),this);

			Log() << "New frame window\n";
			frame_window->logger = &Log();

			traceCurrentOperation = "Resizing window to 640x640";
			frame_window->resize(640, 640);

			traceCurrentOperation = "Showing frame window";
			frame_window->show();
			qApp->processEvents();

			traceCurrentOperation = "Updating GUI controls for new source";
			ui->frameInSpinBox->setValue(this->scan.inFile.FirstFrame());
			ui->frameInTimeCodeLabel->setText(Compute_Timecode_String(0));
			ui->frameOutSpinBox->setValue(this->scan.inFile.LastFrame());
			ui->frameOutTimeCodeLabel->setText(
						Compute_Timecode_String(this->scan.inFile.NumFrames()-1));
			ui->frameNumberTimeCodeLabel->setText(Compute_Timecode_String(0));

			ui->rightSpinBox->setMaximum(this->scan.inFile.Width()-1);
			ui->leftSpinBox->setMaximum(this->scan.inFile.Width()-1);
			ui->rightSlider->setMaximum(this->scan.inFile.Width()-1);
			ui->leftSlider->setMaximum(this->scan.inFile.Width()-1);

			ui->rightPixSpinBox->setMaximum(this->scan.inFile.Width()-1);
			ui->leftPixSpinBox->setMaximum(this->scan.inFile.Width()-1);
			ui->rightPixSlider->setMaximum(this->scan.inFile.Width()-1);
			ui->leftPixSlider->setMaximum(this->scan.inFile.Width()-1);

			ui->leftPixSlider->setValue(int(this->scan.inFile.Width() * 0.475));
			ui->leftPixSpinBox->setValue(int(this->scan.inFile.Width() * 0.475));
			ui->rightPixSlider->setValue(int(this->scan.inFile.Width() * 0.525));
			ui->rightPixSpinBox->setValue(int(this->scan.inFile.Width() * 0.525));

			ui->playSlider->setMaximum(this->scan.inFile.NumFrames()-1);

			// finalize UI with user preferences
			LoadDefaults();

			// enable the rest of the UI that was waiting until a project loaded
			ui->actionSave_Settings->setEnabled(true);
			ui->actionShow_Overlap->setEnabled(true);
			ui->actionShow_Soundtrack_Only->setEnabled(true);
			ui->actionWaveform_Zoom->setEnabled(true);
			ui->menuView->setEnabled(true);
			ui->saveprojectButton->setEnabled(true);
			ui->loadSettingsButton->setEnabled(true);
			ui->tabWidget->setEnabled(true);
			RecursivelyEnable(ui->viewOptionsLayout, true);
			RecursivelyEnable(ui->frameNumberLayout, true);
			//RecursivelyEnable(ui->sampleLayout, false);
			ui->playSampleButton->setEnabled(true);
			ui->autoLoadSettingsCheckBox->setEnabled(true);

			traceCurrentOperation = "Updating GPU params";
			GPU_Params_Update(0);
			traceCurrentOperation = "Displaying first frame";

			Load_Frame_Texture(0);
		}

	}
	catch(std::exception &e)
	{
		QMessageBox w;
		QString msg("Error opening source: \n");
		int answer;

		msg += QString(e.what());

		w.setText(msg);
		w.setStandardButtons(QMessageBox::Abort | QMessageBox::Ok);
		w.setDefaultButton(QMessageBox::Ok);
		answer = w.exec();

		if(answer == QMessageBox::Abort) exit(1);
		return false;
	}

	traceCurrentOperation = "";
	traceSubroutineOperation = "";
	this->frame_window->logger = NULL;
	this->frame_window->currentOperation = NULL;

	// restore the previous SEGV handler
	if(prevSegvHandler != SIG_ERR)
		std::signal(SIGSEGV, prevSegvHandler);

	return true;
}

//********************* Project Load and Save *********************************
bool MainWindow::saveproject(QString fn)
{
	QFile file(fn);
	file.open(QIODevice::WriteOnly);
	QTextStream out(&file);   // we will serialize the data into the file

	saveproject(out);

	file.close();

	return true;
}

bool MainWindow::saveproject(QTextStream &out)
{
	out<<"AEO-Light Project Settings\n";

	// Source Data
	out<<"Source Scan = "<<this->scan.inFile.GetFileName().c_str()<<"\n";
	out<<"Source Format = "<<this->scan.inFile.GetFormatStr()<<"\n";
	out<<"Frame Rate = "<<ui->filmrate_PD->currentText()<<"\n";

	// Sondtrack Settings
	out<<"Use Soundtrack = "<<ui->OverlapSoundtrackCheckBox->isChecked()<<"\n";
	out<<"Left Boundary = "<<ui->leftSpinBox->value()<<"\n";
	out<<"Right Boundary = "<<ui->rightSpinBox->value()<<"\n";
	out<<"Soundtrack Type = "<<ui->monostereo_PD->currentText()<<"\n";
	out<<"Use Pix Track = "<<ui->OverlapPixCheckBox->isChecked()<<"\n";
	out<<"Left Pix Boundary = "<<ui->leftPixSpinBox->value()<<"\n";
	out<<"Right Pix Boundary = "<<ui->rightPixSpinBox->value()<<"\n";
	out<<"Frame Pitch Start = "<<ui->framepitchstartSlider->value()<<"\n";
	out<<"Frame Pitch End = "<<ui->FramePitchendSlider->value()<<"\n";
	out<<"Overlap Search Size = "<<ui->overlapSlider->value()<<"\n";
	out<<"Frame Translation = "<< TRANSSLIDER_VALUE <<"\n";

	// Image Processing Settings
	out<<"Lift = "<<ui->liftSlider->value()<<"\n";
	out<<"Gamma = "<<ui->gammaSlider->value()<<"\n";
	out<<"Gain = "<<ui->gainSlider->value()<<"\n";
	out<<"S-Curve Value = "<<ui->thresholdSlider->value()<<"\n";
	out<<"S-Curve On = "<<ui->threshBox->checkState()<<"\n";
	out<<"Blur = "<<ui->blurSlider->value()<<"\n";
	out<<"Negative = "<<ui->negBox->checkState()<<"\n";
	out<<"Desaturate = "<<ui->desatBox->checkState()<<"\n";
	out<<"Calibrate = "<<ui->CalEnableCB->checkState()<<"\n";

	#ifdef SAVE_CALIBRATION_MASK_IN_PROJECT
	int numFloats = frame_window->cal_points;
	float *mask = frame_window->GetCalibrationMask();
	uchar *cmask = reinterpret_cast<uchar *>(mask);
	QByteArray bytes = qCompress(cmask, numFloats*sizeof(float), 9);
	out<<"Calibration Mask = "<<(bytes.toBase64())<<"\n";

	cmask=NULL;
	delete [] mask;
	mask = NULL;
	#endif

	// Extraction Settings
	out<<"Export Frame In = "<<ui->frameInSpinBox->value()<<"\n";
	out<<"Export Frame Out = "<<ui->frameOutSpinBox->value()<<"\n";
	out<<"Export Sampling Rate = "<<ui->filerate_PD->currentText()<<"\n";
	out<<"Export Bit Depth = "<<ui->bitDepthComboBox->currentText()<<"\n";
	// This is probably more of a source setting than an export setting:
	out<<"Timecode Advance = "<<ui->advance_CB->currentIndex()<<"\n";
	out<<"Export XML Sidecar = "<<ui->xmlSidecarComboBox->currentText()<<"\n";

	// View Settings
	out<<"View Zoom = "<<ui->waveformZoomCheckBox->isChecked()<<"\n";
	out<<"View Overlap = "<<ui->showOverlapCheckBox->isChecked()<<"\n";
	out<<"View Soundtrack Only = "<<
			ui->showSoundtrackOnlyCheckBox->isChecked()<<"\n";
	out<<"View Frame = "<<ui->frame_numberSpinBox->value()<<"\n";

	return true;
}

bool MainWindow::OpenProject(QString fn)
{
	if(LoadProjectSource(fn))
		return LoadProjectSettings(fn);
	return false;
}

void MainWindow::OpenStartingProject()
{
	Log() << "Opening starting project\n";
	if(!this->startingProjectFilename.isEmpty())
		this->OpenProject(this->startingProjectFilename);
}

bool MainWindow::LoadProjectSource(QString fn)
{
	QFile file(fn);
	file.open(QIODevice::ReadOnly);
	QTextStream in(&file);   // we will serialize the data into the file
	bool foundSource = false;
	SourceFormat f = SOURCE_UNKNOWN;
	QString srcfn;

	while(!in.atEnd() && !foundSource) {
		QString line = in.readLine();
		QStringList fields = line.split(QRegExp("\\s*=\\s*"));

		if((fields[0]).contains("Source Scan"))
		{
			foundSource = true;
			srcfn = fields[1];
		}
		if((fields[0]).contains("Source Format"))
		{
			f = FilmScan::StrToSourceFormat(fields[1].toStdString().c_str());
		}
	}
	file.close();

	if(!foundSource)
	{
		(void)QMessageBox::warning(this, tr("AEO-Light"),
				tr("This .aeo file doesn't contain the name of a source scan.\n"
				"(It was created by the former [save settings] action)."));
		return false;
	}
	this->NewSource(srcfn);

	return true;
}

bool MainWindow::LoadProjectSettings(QString fn)
{
	QFile file(fn);
	file.open(QIODevice::ReadOnly);
	QTextStream in(&file);   // we will serialize the data into the file

	bool deleteMe = false;
	bool needMask = false;
	bool gotMask = false;

	while(!in.atEnd()) {
		QString line = in.readLine();
		QStringList fields = line.split(QRegExp("\\s*=\\s*"));

		if ((fields[0]).contains("Frame Rate"))
		{
			int idx = ui->filmrate_PD->findText(fields[1],Qt::MatchExactly);
			if(idx != -1) ui->filmrate_PD->setCurrentIndex(idx);
		}
		if ((fields[0]).contains("Use Soundtrack"))
			ui->OverlapSoundtrackCheckBox->setChecked(fields[1].toInt());
		if ((fields[0]).contains("Left Bound"))
			ui->leftSpinBox->setValue(fields[1].toInt());
		if ((fields[0]).contains("Right Bound"))
			ui->rightSpinBox->setValue(fields[1].toInt());
		if ((fields[0]).contains("Soundtrack Type"))
		{
			int idx = ui->monostereo_PD->findText(fields[1],Qt::MatchExactly);
			if(idx != -1) ui->monostereo_PD->setCurrentIndex(idx);
		}
		if ((fields[0]).contains("Use Pix Track"))
			ui->OverlapPixCheckBox->setChecked(fields[1].toInt());
		if ((fields[0]).contains("Left Pix Bound"))
			ui->leftPixSpinBox->setValue(fields[1].toInt());
		if ((fields[0]).contains("Right Pix Bound"))
			ui->rightPixSpinBox->setValue(fields[1].toInt());
		if ((fields[0]).contains("Frame Pitch Start"))
		{
			ui->framepitchstartSlider->setValue(fields[1].toInt());
			ui->framepitchstartLabel->setText(
					QString::number(fields[1].toInt()/1000.f,'f',2));
		}
		if ((fields[0]).contains("Frame Pitch End"))
			ui->FramePitchendSlider->setValue(fields[1].toInt());
		if ((fields[0]).contains("Overlap Search Size"))
			ui->overlapSlider->setValue(fields[1].toInt());
		if ((fields[0]).contains("Lift"))
			ui->liftSlider->setValue(fields[1].toInt());
		if ((fields[0]).contains("Gamma"))
			ui->gammaSlider->setValue(fields[1].toInt());
		if ((fields[0]).contains("Gain"))
			ui->gainSlider->setValue(fields[1].toInt());
		if ((fields[0]).contains("S-Curve Value"))
			ui->thresholdSlider->setValue(fields[1].toInt());
		if ((fields[0]).contains("S-Curve On"))
			ui->threshBox->setChecked(fields[1].toInt());
		if ((fields[0]).contains("Blur"))
			ui->blurSlider->setValue(fields[1].toInt());
		if ((fields[0]).contains("Negative"))
			ui->negBox->setChecked(fields[1].toInt());
		if ((fields[0]).contains("Desaturate"))
			ui->desatBox->setChecked(fields[1].toInt());

		if ((fields[0]).contains("Calibrate"))
		{
			ui->CalEnableCB->setChecked(fields[1].toInt());
			needMask = true;
		}

		#ifdef SAVE_CALIBRATION_MASK_IN_PROJECT
		if((fields[0]).contains("Calibration Mask"))
		{
			QByteArray bytes = qUncompress(fields[1].toUtf8());
			const char *cmask = bytes.constData();
			const float *mask = reinterpret_cast<const float *>(cmask);
			frame_window->SetCalibrationMask(mask);
			gotMask = true;
		}
		#endif

		// Extraction Settings
		if ((fields[0]).contains("Export Frame In"))
			ui->frameInSpinBox->setValue(fields[1].toInt());
		if ((fields[0]).contains("Export Frame Out"))
			ui->frameOutSpinBox->setValue(fields[1].toInt());
		if ((fields[0]).contains("Export Sampling Rate"))
		{
			int idx = ui->filerate_PD->findText(fields[1],Qt::MatchExactly);
			if(idx != -1) ui->filerate_PD->setCurrentIndex(idx);
		}
		if ((fields[0]).contains("Export Bit Depth"))
		{
			int idx = ui->bitDepthComboBox->
					findText(fields[1],Qt::MatchExactly);
			if(idx != -1) ui->bitDepthComboBox->setCurrentIndex(idx);
		}
		// This is probably more of a source setting than an export setting:
		if ((fields[0]).contains("Timecode Advance"))
			ui->advance_CB->setCurrentIndex(fields[1].toInt());

		if ((fields[0]).contains("Export XML Sidecar"))
		{
			int idx = ui->xmlSidecarComboBox->
					findText(fields[1],Qt::MatchExactly);
			if(idx != -1) ui->xmlSidecarComboBox->setCurrentIndex(idx);
		}

		// View Settings
		if ((fields[0]).contains("View Zoom"))
			ui->waveformZoomCheckBox->setChecked(fields[1].toInt());
		if ((fields[0]).contains("View Overlap"))
			ui->showOverlapCheckBox->setChecked(fields[1].toInt());
		if ((fields[0]).contains("View Soundtrack Only"))
			ui->showSoundtrackOnlyCheckBox->setChecked(fields[1].toInt());
		if ((fields[0]).contains("View Frame"))
			ui->frame_numberSpinBox->setValue(fields[1].toInt());

		if((fields[0]).contains("DeleteMe"))
			deleteMe = (fields[1].contains("true"));
	}

	file.close();
	if(deleteMe) file.remove();

	if(gotMask)
		ui->CalEnableCB->setEnabled(true);
	else if(needMask)
		QTimer::singleShot(20, this, SLOT(on_CalBtn_clicked()));

	on_OverlapPixCheckBox_clicked(ui->OverlapPixCheckBox->isChecked());

	return true;
}


void MainWindow::on_saveprojectButton_clicked()
{
	QString savDir;

	if(this->prevProjectDir.isEmpty())
	{
		QSettings settings;
		settings.beginGroup("default-folder");
		savDir = settings.value("project").toString();
		if(savDir.isEmpty())
		{
			savDir = QStandardPaths::writableLocation(
						QStandardPaths::DocumentsLocation);
		}
		settings.endGroup();
	}
	else
	{
		savDir = this->prevProjectDir;
	}

	QString fileName = QFileDialog::getSaveFileName(this,
			tr("Save AEO Light Project"), savDir,
			tr("AEO Settings Files (*.aeo)"));

	if(fileName.isEmpty()) return;

	saveproject(fileName);
	this->prevProjectDir = QFileInfo(fileName).absolutePath();
}

void MainWindow::on_loadprojectButton_clicked()
{
	QString prjDir;

	if(this->prevProjectDir.isEmpty())
	{
		QSettings settings;
		settings.beginGroup("default-folder");
		prjDir = settings.value("project").toString();
		if(prjDir.isEmpty())
		{
			prjDir = QStandardPaths::writableLocation(
						QStandardPaths::DocumentsLocation);
		}
		settings.endGroup();
	}
	else
	{
		prjDir = this->prevProjectDir;
	}

	QString fileName = QFileDialog::getOpenFileName(this,
			tr("Open AEO Light Project"), prjDir,
			tr("AEO Settings Files (*.aeo)"));

	if(fileName.isEmpty()) return;

	this->OpenProject(fileName);
	this->prevProjectDir = QFileInfo(fileName).absolutePath();
}

void MainWindow::on_loadSettingsButton_clicked()
{
	QString prjDir;

	if(this->prevProjectDir.isEmpty())
	{
		QSettings settings;
		settings.beginGroup("default-folder");
		prjDir = settings.value("project").toString();
		if(prjDir.isEmpty())
		{
			prjDir = QStandardPaths::writableLocation(
						QStandardPaths::DocumentsLocation);
		}
		settings.endGroup();
	}
	else
	{
		prjDir = this->prevProjectDir;
	}

	QString fileName = QFileDialog::getOpenFileName(this,
			tr("Open AEO Light Project"), prjDir,
			tr("AEO Settings Files (*.aeo)"));

	if(fileName.isEmpty()) return;

	this->LoadProjectSettings(fileName);
	this->prevProjectDir = QFileInfo(fileName).absolutePath();
}


//*********************Sequence & Track Selection UI **************************
void MainWindow::on_leftPixSlider_sliderMoved(int position)
{
	ui->leftPixSpinBox->setValue(position);
}

void MainWindow::on_rightPixSlider_sliderMoved(int position)
{
	ui->rightPixSpinBox->setValue(position);
}

void MainWindow::on_leftPixSpinBox_valueChanged(int arg1)
{
	ui->leftPixSlider->setValue(arg1);
	GPU_Params_Update(1);
}

void MainWindow::on_rightPixSpinBox_valueChanged(int arg1)
{
	ui->rightPixSlider->setValue(arg1);
	GPU_Params_Update(1);
}

void MainWindow::on_leftSpinBox_valueChanged(int arg1)
{
	ui->leftSlider->setValue(arg1);
	GPU_Params_Update(1);
}

void MainWindow::on_rightSpinBox_valueChanged(int arg1)
{
	ui->rightSlider->setValue(arg1);
	GPU_Params_Update(1);
}
void MainWindow::on_leftSlider_sliderMoved(int position)
{
	ui->leftSpinBox->setValue(position);
}
void MainWindow::on_rightSlider_sliderMoved(int position)
{
	ui->rightSpinBox->setValue(position);
}
void MainWindow::on_playSlider_sliderMoved(int position)
{
	FrameTexture *ft;

	if(position > ui->frame_numberSpinBox->minimum())
	{
		Load_Frame_Texture(position-1);
	}
	Load_Frame_Texture(position);
	ui->frame_numberSpinBox->setValue(position);
	ui->frameNumberTimeCodeLabel->setText( Compute_Timecode_String(position));

}
void MainWindow::on_markinButton_clicked()
{
	ui->frameInSpinBox->setValue(
			this->scan.inFile.FirstFrame()+ui->playSlider->value());
}

void MainWindow::on_markoutButton_clicked()
{
	ui->frameOutSpinBox->setValue(
			this->scan.inFile.FirstFrame()+ui->playSlider->value());

}

void MainWindow::on_framepitchstartSlider_valueChanged(int value)
{
	ui->framepitchstartLabel->setText(QString::number(value/1000.f,'f',2));
	GPU_Params_Update(1);
}

void MainWindow::on_overlapSlider_valueChanged(int value)
{
	ui->overlap_label->setText(QString::number(value/100.f,'f',2));
	GPU_Params_Update(1);
}

void MainWindow::on_frame_numberSpinBox_valueChanged(int arg1)
{
	// load the previous frame so that overlap can be computed

	if(arg1 > ui->frame_numberSpinBox->minimum())
		Load_Frame_Texture(arg1-1);


	Load_Frame_Texture(arg1);
    ui->playSlider->setValue(  arg1);
     ui->frameNumberTimeCodeLabel->setText( Compute_Timecode_String(arg1));

}


void MainWindow::on_HeightCalculateBtn_clicked()
{
	if (frame_window!=NULL && frame_window->is_calc==false)
	{
		frame_window->is_calc=false;

		frame_window->bestloc=frame_window->bestmatch.postion;

		this->ui->FramePitchendSlider->setValue(
				int( (1.0-(1.0+(frame_window->overlap[3] -
						frame_window->overlap[0]))) * 1000.0));

		this->ui->overlapSlider->setValue(3);
		this->ui->overlap_label->setText(QString::number(3./100.f,'f',2));
		frame_window->currmatch = frame_window->bestmatch;
		frame_window->currstart = frame_window->overlap[3];
		frame_window->is_calc=true;

		//this->ui->FramePitchendSlider->setEnabled(false);

		this->ui->HeightCalculateBtn->setText("Unlock Height");
	}
	else
	{
		frame_window->is_calc=false;

		this->ui->FramePitchendSlider->setEnabled(true);

		this->ui->HeightCalculateBtn->setText("Lock Height");
	}
}


void MainWindow::on_extractGLButton_clicked()
{
	if(QApplication::keyboardModifiers() & Qt::ShiftModifier)
		extractGL(false);
	else
		extractGL(true);
}

bool MainWindow::extractGL(QString filename=QString(), bool doTimer=false)
{
	bool batch = true;
	MetaData meta;
	QString videoFilename;

	long firstFrame =
			ui->frameInSpinBox->value() - this->scan.inFile.FirstFrame();
	long numFrames =
			ui->frameOutSpinBox->value() - ui->frameInSpinBox->value() + 1;

	int samplingRate = 48000;
	if(ui->filerate_PD->currentIndex() == 1) samplingRate = 96000;
	meta.timeReference = ComputeTimeReference(firstFrame, samplingRate);

	int bitDepth=ui->bitDepthComboBox->currentText().split(" ")[0].toInt();

	QString stereo = (ui->monostereo_PD->currentIndex()==0)?"dual-mono":"stereo";

	meta.codingHistory = QString("A=PCM,F=%1,W=%2,M=%3,T=AEO-Light").
			arg(samplingRate).arg(bitDepth).arg(stereo);

	// Ask for output filename
	if(filename.isEmpty())
	{
		QString expDir;

		if(this->prevExportDir.isEmpty())
		{
			QSettings settings;
			settings.beginGroup("default-folder");
			expDir = settings.value("export").toString();
			if(expDir.isEmpty())
			{
				expDir = QStandardPaths::writableLocation(
							QStandardPaths::DocumentsLocation);
			}
			settings.endGroup();
		}
		else
		{
			expDir = this->prevExportDir;
		}

		ExtractDialog dialog(this, meta, expDir);
		dialog.setWindowTitle("Extract to Audio File");
		if(this->isVideoMuxingRisky) dialog.MarkVideoAsRisky();

		if(dialog.exec() == ExtractDialog::Rejected)
		{
			if(dialog.RequestedRestart())
			{
				// restart
				QTemporaryFile savefile;
				savefile.setAutoRemove(false);
				savefile.open();
				QTextStream stream(&savefile);
				this->saveproject(stream);
				stream << "DeleteMe = true\n";
				savefile.close();
				qApp->quit();
				QStringList args; //qApp->arguments());
				args << savefile.fileName();
				QProcess::startDetached(qApp->arguments()[0], args);
			}
			return false;
		}

		//filename = QFileDialog::getSaveFileName(
		//			this,tr("Export audio to"),expDir,"*.wav");
		filename = dialog.GetFilename();
		if(filename.isEmpty()) return false;

		this->prevExportDir = QFileInfo(filename).absolutePath();

		videoFilename = dialog.GetVideoFilename();

		this->currentMeta = &meta;
		batch = false;
	}

	QElapsedTimer timer;
	if(doTimer) timer.start();

	av_log(NULL, AV_LOG_INFO, "Extract()\n");
	av_log(NULL, AV_LOG_INFO, "WriteAudio: %s\n", filename.toStdString().c_str());
	if(!videoFilename.isEmpty())
		av_log(NULL, AV_LOG_INFO, "WriteVideo: %s\n", videoFilename.toStdString().c_str());
	else
		av_log(NULL, AV_LOG_INFO, "WriteVideo: -no-\n");

	bool success = Extract(filename, videoFilename, firstFrame, numFrames,
			EXTRACT_LOG);

	QString processMsg;
	QString timingMsg;

	if(success)
	{
		processMsg = tr("Sound Extraction Complete");
		if(doTimer)
		{
			qint64 ms = timer.elapsed();
			double sec = ms / 1.0e3;
			double fps = numFrames / sec;

			timingMsg = QString(tr("Elapsed time: %1 seconds. (%2 fps)")).
					arg(sec).arg(fps);
		}
		else
		{
			timingMsg = tr("Sound extraction completed successfully.");
		}

		if(ui->xmlSidecarComboBox->currentText() == "Premis")
		{
			write_premis_xml(
					(filename + QString(".xml")).toStdString().c_str(),
					*frame_window, this->scan.inFile,
					filename.toStdString().c_str(),
					firstFrame, numFrames
			);
		} else if(ui->xmlSidecarComboBox->currentText() == "MODS")
		{
			write_mods_xml(
					(filename + QString(".xml")).toStdString().c_str(),
					*frame_window, this->scan.inFile,
					filename.toStdString().c_str(),
					firstFrame, numFrames
			);
		}
	}
	else
	{
		processMsg = tr("Sound Extraction Canceled");
		timingMsg = tr("Sound extraction canceled before finishing.");
	}

	Log() << timingMsg << "\n";
	Log() << QDateTime::currentDateTime().toString() << "\n";

	if(!batch)
	{
		this->LogClose();
		this->currentMeta = NULL;

		QMessageBox msg;
		msg.setText(processMsg);
		msg.setInformativeText(timingMsg);
		QPushButton *viewButton =
				msg.addButton("View Log", QMessageBox::HelpRole);
		msg.addButton(QMessageBox::Ok);

		msg.exec();

		if(msg.clickedButton() == viewButton)
		{
			#ifdef _WIN32
			system(QString("start %1/%2").arg(QDir::homePath(),"AEO-log.txt")
					.toStdString().c_str());
			#else
			system("open ~/.aeolight.log.txt");
			#endif
		}
	}

	return success;
}

//----------------------------------------------------------------------------
void MainWindow::PlaySample(int index)
{
	ExtractedSound sample;

	if(samplesPlayed.size() <= index) return;

	sample = samplesPlayed[index];

	if(sample.sound == NULL) return;

	if(ui->autoLoadSettingsCheckBox->isChecked())
	{
		ExtractionParametersToGUI(sample);
		GPU_Params_Update(true);
	}

	QSound *sound = sample.sound;
	if(sound->isFinished())
		sound->play();

	return;
}

//----------------------------------------------------------------------------
void MainWindow::on_playSampleButton_clicked()
{
	ExtractedSound sample;

	// create a new sample:
	QTemporaryFile qtmp(QDir::tempPath()+"/aeoXXXXXX.wav");
	// open and close it to ensure the name is fully resolved
	qtmp.setAutoRemove(false);
	qtmp.open();
	qtmp.close();

	long firstFrame = ui->frameInSpinBox->value() -
			this->scan.inFile.FirstFrame();
	long numFrames = 24 * 5;
	if(numFrames >
			this->scan.inFile.LastFrame() -
			ui->frameInSpinBox->value())
	{
		numFrames = this->scan.inFile.LastFrame() -
				ui->frameInSpinBox->value();
	}

	this->currentMeta = NULL;

	sample = Extract(qtmp.fileName(), QString(), firstFrame, numFrames, EXTRACT_LOG);

	static long nSample = 0;

	if(sample.err == 0)
	{
		sample.sound = new QSound(qtmp.fileName(), NULL);
		sample.sound->play();

		SaveSampleDialog *save = new SaveSampleDialog(this);
		save->setWindowTitle("Save Sample Audio");
		save->exec();

		if(save->result() == QDialog::Accepted)
		{
			switch(save->SelectedSlot())
			{
			case 1:
				if(samplesPlayed[0].sound != NULL)
					delete samplesPlayed[0].sound;

				samplesPlayed[0] = sample;
				ui->playSample1Button->setEnabled(true);
				ui->loadSample1Button->setEnabled(true);
				break;
			case 2:
				if(samplesPlayed[1].sound != NULL)
					delete samplesPlayed[1].sound;

				samplesPlayed[1] = sample;
				ui->playSample2Button->setEnabled(true);
				ui->loadSample2Button->setEnabled(true);
				break;
			case 3:
				if(samplesPlayed[2].sound != NULL)
					delete samplesPlayed[2].sound;

				samplesPlayed[2] = sample;
				ui->playSample3Button->setEnabled(true);
				ui->loadSample3Button->setEnabled(true);
				break;
			case 4:
				if(samplesPlayed[3].sound != NULL)
					delete samplesPlayed[3].sound;

				samplesPlayed[3] = sample;
				ui->playSample4Button->setEnabled(true);
				ui->loadSample4Button->setEnabled(true);
				break;
			}
		}
	}

	return;
}

void MainWindow::on_playSample1Button_clicked()
{
	PlaySample(0);
}

void MainWindow::on_playSample2Button_clicked()
{
	PlaySample(1);
}

void MainWindow::on_playSample3Button_clicked()
{
	PlaySample(2);
}

void MainWindow::on_playSample4Button_clicked()
{
	PlaySample(3);
}

void MainWindow::on_loadSample1Button_clicked()
{
	ExtractionParametersToGUI(samplesPlayed[0]);
	GPU_Params_Update(true);
}

void MainWindow::on_loadSample2Button_clicked()
{
	ExtractionParametersToGUI(samplesPlayed[1]);
	GPU_Params_Update(true);
}

void MainWindow::on_loadSample3Button_clicked()
{
	ExtractionParametersToGUI(samplesPlayed[2]);
	GPU_Params_Update(true);
}

void MainWindow::on_loadSample4Button_clicked()
{
	ExtractionParametersToGUI(samplesPlayed[3]);
	GPU_Params_Update(true);
}

//----------------------------------------------------------------------------

ExtractedSound MainWindow::Extract(QString filename, QString videoFilename,
		long firstFrame, long numFrames, uint8_t flags)
{
	ExtractedSound ret;
	bool success;

	av_log(NULL, AV_LOG_INFO, "Extract() called:\n");
	av_log(NULL, AV_LOG_INFO, "WriteAudio: %s\n", filename.toStdString().c_str());
	if(!videoFilename.isEmpty())
		av_log(NULL, AV_LOG_INFO, "WriteVideo: %s\n", videoFilename.toStdString().c_str());
	else
		av_log(NULL, AV_LOG_INFO, "WriteVideo: -no-\n");

	// pull requested fps from GUI
	switch(ui->filmrate_PD->currentIndex())
	{
	case 0: this->frame_window->fps = 23.976; break;
	case 1: this->frame_window->fps = 24.000; break;
	case 2: this->frame_window->fps = 25.000; break;
	}

	// duration in milliseconds
	this->frame_window->duration = numFrames * this->frame_window->fps * 100.0;

	// pull requested bit depth from GUI
	this->frame_window->bit_depth =
			ui->bitDepthComboBox->currentText().split(" ")[0].toInt();

	switch(ui->filerate_PD->currentIndex())
	{
	case 0: this->frame_window->sampling_rate = 48000; break;
	case 1: this->frame_window->sampling_rate = 96000; break;
	default: this->frame_window->sampling_rate = 0;
	}

	try
	{
		#if 0
		if(this->scan.inFile.IsSynth())
		{
			int ans = QMessageBox::question(this, "Cheat?",
					"Use exact synthetic overlap? (i.e., cheat)",
					QMessageBox::Yes | QMessageBox::No,
					QMessageBox::No);

			if(ans == QMessageBox::Yes)
				this->scan.cheatOverlap = true;
		}
		#endif

		if(flags & EXTRACT_LOG)
		{
			Log() << QDateTime::currentDateTime().toString() << "\n";
			Log() << Version() << "\n";
			Log() << "Source: " <<
					 QString(scan.inFile.GetFileName().c_str()) << "\n";
			Log() << "Output: " << filename << "\n";

			Log() << "Size: " <<
					 this->scan.inFile.Width() << "x" <<
					 this->scan.inFile.Height() << "\n";

			this->LogSettings();
			this->frame_window->PrintGLVersion(Log());
		}

		if(!this->frame_window->isExposed())
		{
			if(flags & EXTRACT_LOG) Log() << "Frame window not exposed.\n";

			this->frame_window->raise();
			this->frame_window->showNormal();
			this->frame_window->setWindowState(Qt::WindowActive);
			//qApp->setActiveWindow(this->frame_window);

			qApp->processEvents();
		}

		while (!this->frame_window->isExposed())
		{
			if(flags & EXTRACT_LOG)
				Log() << "Frame window still not exposed. "
						"Asking for user help.\n";
			int raise = QMessageBox::question(this, "Select image Window",
					"The image window is obscured. "
					"Please move it to the front.",
					QMessageBox::Ok | QMessageBox::Cancel,
					QMessageBox::Ok);

			if(raise == QMessageBox::Cancel)
			{
				if(flags & EXTRACT_LOG)
					Log() << "User cancelled (could not raise window?)\n";
				ret.err = 1;
				return ret;
			}
		}

		if(flags & EXTRACT_LOG) Log() << "Frame window is exposed.\n";

		void (*prevSegvHandler)(int);
		prevSegvHandler = std::signal(SIGSEGV, SegvHandler);

		traceCurrentOperation = "";
		traceSubroutineOperation = "";
		int jmp = setjmp(segvJumpEnv);
		if(jmp!=0)
		{
			QMessageBox::critical(NULL,"Critical Failure",
					QString("Critical Failure around frame %1.\n%2\n%3").
							arg(lastFrameLoad).arg(traceCurrentOperation).
							arg(traceSubroutineOperation));

			std::exit(1);
		}

		if(flags & EXTRACT_LOG) this->frame_window->logger = &Log();
		this->frame_window->currentOperation = &traceSubroutineOperation;

		success = this->WriteAudioToFile(filename.toStdString().c_str(),
				videoFilename.toStdString().c_str(),
				firstFrame, numFrames);
		this->frame_window->logger = NULL;
		this->frame_window->currentOperation = NULL;

		// restore the previous SEGV handler
		if(prevSegvHandler != SIG_ERR)
			std::signal(SIGSEGV, prevSegvHandler);


	}
	catch(std::exception &e)
	{
		QMessageBox w;
		QString msg("Error extracting sound: \n");
		int answer;

		msg += QString(e.what());

		w.setText(msg);
		w.setStandardButtons(QMessageBox::Abort | QMessageBox::Ok);
		w.setDefaultButton(QMessageBox::Ok);
		answer = w.exec();

		if(answer == QMessageBox::Abort) exit(1);
		ret.err = 2;
		return ret;
	}

	ret = ExtractionParamsFromGUI();

	if(success)
		ret.err = 0;
	else if(this->requestCancel)
		ret.err = 1;
	else
	{
		QMessageBox w;
		QString msg("Error writing wav file.\n");
		int answer;

		w.setText(msg);
		w.setStandardButtons(QMessageBox::Abort | QMessageBox::Ok);
		w.setDefaultButton(QMessageBox::Ok);
		answer = w.exec();

		if(answer == QMessageBox::Abort) exit(1);
		ret.err = 3;
	}

	return ret;
}

#ifdef USE_MUX_HACK
//----------------------------------------------------------------------------
//---------------------- MUX.C -----------------------------------------------
//----------------------------------------------------------------------------
/*
 * Copyright (c) 2003 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * @file
 * libavformat API example.
 *
 * Output a media file in any supported libavformat format. The default
 * codecs are used.
 * @example muxing.c
 */

#define STREAM_DURATION   10.0
#define STREAM_PIX_FMT    AV_PIX_FMT_YUV420P /* default pix_fmt */

#define RGB_PIX_FMT       AV_PIX_FMT_RGBA

#define SCALE_FLAGS SWS_BICUBIC

bool MainWindow::EnqueueNextFrame()
{
	if(this->encCurFrame > this->encNumFrames) return false;

	traceCurrentOperation = "Load Texture";

	av_log(NULL, AV_LOG_INFO, "encStartFrame = %ld\n", this->encStartFrame);
	av_log(NULL, AV_LOG_INFO, "encCurFrame = %ld\n", this->encCurFrame);

	long framenum = this->encStartFrame + this->encCurFrame;

	av_log(NULL, AV_LOG_INFO, "framenum = %ld\n", framenum);

	if(framenum > this->scan.inFile.NumFrames()-1)
	{
		// TODO: correct the sound array allocation steps above
		// so that we don't have to load the final frame twice
		av_log(NULL, AV_LOG_INFO, "Load_Frame_Texture(%ld)\n", framenum-1);
		if(!Load_Frame_Texture(framenum - 1)) return false;
	}
	else
	{
		av_log(NULL, AV_LOG_INFO, "Load_Frame_Texture(%ld)\n", framenum);
		if(!Load_Frame_Texture(framenum)) return false;
	}

	// XXX: Warning: this reads to vo.videobuffer instead -- do not change
	// the argument expecting it to work.
	av_log(NULL, AV_LOG_INFO, "this->frame_window->read_frame_texture(this->outputFrameTexture)\n");
	this->frame_window->read_frame_texture(this->outputFrameTexture);

	av_log(NULL, AV_LOG_INFO, "p = new uint8_t[%ld]\n", this->encVideoBufSize);
	uint8_t *p = new uint8_t[this->encVideoBufSize];
	av_log(NULL, AV_LOG_INFO, "memcpy(p, this->frame_window->vo.videobuffer, this->encVideoBufSize)\n");
	memcpy(p, this->frame_window->vo.videobuffer, this->encVideoBufSize);
	av_log(NULL, AV_LOG_INFO, "enqueue p = %p into this->encVideoQueue\n", p);
	this->encVideoQueue.push(p);

	// update audio signal render buffer length
	this->encAudioLen += this->frame_window->samplesperframe_file;
	av_log(NULL, AV_LOG_INFO, "audio render len = %lu\n", (unsigned long)(this->encAudioLen));

	this->encCurFrame++;

	return true;
}

uint8_t *MainWindow::GetVideoFromQueue()
{
	uint8_t *p;

	av_log(NULL, AV_LOG_INFO, "encVideoQueue.empty() = %c\n", encVideoQueue.empty()?'T':'F');
	av_log(NULL, AV_LOG_INFO, "encVideoQueue.size() = %lu\n", encVideoQueue.size());

	if(this->encVideoQueue.empty())
	{
		if(!EnqueueNextFrame()) return NULL;
	}
	p = this->encVideoQueue.front();
	this->encVideoQueue.pop();

	av_log(NULL, AV_LOG_INFO, "return p = %p\n", p);
	return p;
}

AVFrame *MainWindow::GetAudioFromQueue()
{
	const int nbits=16;

	while(this->encS16Frame->pts + this->encS16Frame->nb_samples > this->encAudioLen)
	{
		if(!EnqueueNextFrame()) return NULL;
	}

	int64_t offset = this->encAudioNextPts - this->encAudioPad;
	av_log(NULL, AV_LOG_INFO, "audio render offset = %lld\n", offset);
	av_log(NULL, AV_LOG_INFO, "audio render len = %lld\n", this->encAudioLen);

	if(this->encS16Frame->channels != 2)
		throw AeoException("sound must be stereo");

	int16_t *samples = (int16_t *)(this->encS16Frame->data[0]);
	int s = 0;

	int16_t v;

	if(offset < 0)
	{
		for(int i = 0; i<this->encS16Frame->nb_samples; ++i)
		{
			for(int c = 0; c < this->encS16Frame->channels; ++c)
			{
				samples[s++] = 0;
			}
		}
		av_log(NULL, AV_LOG_INFO, "Silence written nb_samples = %d x%d\n",
				this->encS16Frame->nb_samples, this->encS16Frame->channels);
	}
	else
	{
		// translate the floating point values to S16:
		float **audio = this->frame_window->FileRealBuffer;
		av_log(NULL, AV_LOG_INFO, "audio = FileRealBuffer = [%p,%p]\n",
				audio[0], audio[1]);
		av_log(NULL, AV_LOG_INFO, "Audio copy nb_samples = %d x%d\n",
				this->encS16Frame->nb_samples, this->encS16Frame->channels);
		for(int i = 0; i<this->encS16Frame->nb_samples; ++i)
		{
			for(int c = 0; c < this->encS16Frame->channels; ++c)
			{
				//av_log(NULL, AV_LOG_INFO, "reading audio[%d][%lld]\n", c, offset+i);
				v = int32_t(
						(audio[c][offset+i]*UMAX(nbits))-(UMAX(nbits)/2));
				//av_log(NULL, AV_LOG_INFO, "writing samples[%d]\n", s);
				samples[s++] = v;
			}
		}
		av_log(NULL, AV_LOG_INFO, "Audio copied nb_samples = %d x%d\n",
				this->encS16Frame->nb_samples, this->encS16Frame->channels);
	}

	this->encS16Frame->pts = this->encAudioNextPts;
	this->encAudioNextPts += this->encS16Frame->nb_samples;

	return this->encS16Frame;
}

static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt)
{
#if 0
	AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;

	printf("pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
		   av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
		   av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
		   av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
		   pkt->stream_index);
#endif
}

static int write_frame(AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt)
{
	/* rescale output packet timestamp values from codec to stream timebase */
	av_packet_rescale_ts(pkt, *time_base, st->time_base);
	pkt->stream_index = st->index;

	/* Write the compressed frame to the media file. */
	log_packet(fmt_ctx, pkt);
	return av_interleaved_write_frame(fmt_ctx, pkt);
}

/* Add an output stream. */
static void add_stream(OutputStream *ost, AVFormatContext *oc,
					   AVCodec **codec,
					   enum AVCodecID codec_id)
{
	AVCodecContext *c;
	int i;

	/* find the encoder */
	*codec = avcodec_find_encoder(codec_id);
	if (!(*codec)) {
		fprintf(stderr, "Could not find encoder for '%s'\n",
				avcodec_get_name(codec_id));
		exit(1);
	}

	// allocid: MainWindowStream01
	ost->st = avformat_new_stream(oc, *codec);
	if (!ost->st) {
		fprintf(stderr, "Could not allocate stream\n");
		exit(1);
	}
	av_log(NULL, AV_LOG_INFO,
			"ALLOC new: MainWindowStream01 ost->st->codec = %p\n",
			ost->st->codec);
	ost->st->id = oc->nb_streams-1;
	c = ost->st->codec;

	switch ((*codec)->type) {
	case AVMEDIA_TYPE_AUDIO:
		c->sample_fmt  = (*codec)->sample_fmts ?
			(*codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
		c->bit_rate    = 64000;

		if(ost->requestedSamplingRate > 0)
			c->sample_rate = ost->requestedSamplingRate;
		else
			c->sample_rate = 44100;

		/* Ugh. Why would you override your setting as soon as you set it?
		 *
		if ((*codec)->supported_samplerates) {
			c->sample_rate = (*codec)->supported_samplerates[0];
			for (i = 0; (*codec)->supported_samplerates[i]; i++) {
				if ((*codec)->supported_samplerates[i] == 44100)
					c->sample_rate = 44100;
			}
		}
		*/

		if ((*codec)->channel_layouts) {
			c->channel_layout = (*codec)->channel_layouts[0];
			for (i = 0; (*codec)->channel_layouts[i]; i++) {
				if ((*codec)->channel_layouts[i] == AV_CH_LAYOUT_STEREO)
					c->channel_layout = AV_CH_LAYOUT_STEREO;
			}
		}
		else
			c->channel_layout = AV_CH_LAYOUT_STEREO;

		c->channels = av_get_channel_layout_nb_channels(c->channel_layout);

		ost->st->time_base.num = 1;
		ost->st->time_base.den = c->sample_rate;
		break;

	case AVMEDIA_TYPE_VIDEO:
		c->codec_id = codec_id;

		c->bit_rate = 400000;
		/* Resolution must be a multiple of two. */
		if(ost->requestedWidth > 0)
		{
			c->width = ost->requestedWidth;
			c->height = ost->requestedHeight;
		}
		else
		{
			c->width    = 352;
			c->height   = 288;
		}

		c->bit_rate = c->width * c->height * 4;

		/* timebase: This is the fundamental unit of time (in seconds) in terms
		 * of which frame timestamps are represented. For fixed-fps content,
		 * timebase should be 1/framerate and timestamp increments should be
		 * identical to 1. */
		ost->st->time_base.num = 1;
		ost->st->time_base.den = ost->requestedTimeBase;

		c->time_base       = ost->st->time_base;

		c->gop_size      = 12; /* emit one intra frame every twelve frames at most */
		c->pix_fmt       = STREAM_PIX_FMT;
		if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
			/* just for testing, we also add B frames */
			c->max_b_frames = 2;
		}
		if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
			/* Needed to avoid using macroblocks in which some coeffs overflow.
			 * This does not happen with normal video, it just happens here as
			 * the motion of the chroma plane does not match the luma plane. */
			c->mb_decision = 2;
		}
	break;

	default:
		break;
	}

#ifndef AV_CODEC_FLAG_GLOBAL_HEADER
#define AV_CODEC_FLAG_GLOBAL_HEADER CODEC_FLAG_GLOBAL_HEADER
#endif

	/* Some formats want stream headers to be separate. */
	if (oc->oformat->flags & AVFMT_GLOBALHEADER)
		c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
}

/**************************************************************/
/* audio output */

static AVFrame *alloc_audio_frame(enum AVSampleFormat sample_fmt,
								  uint64_t channel_layout,
								  int sample_rate, int nb_samples)
{
	AVFrame *frame = av_frame_alloc();
	int ret;

	if (!frame) {
		fprintf(stderr, "Error allocating an audio frame\n");
		exit(1);
	}

	frame->format = sample_fmt;
	frame->channel_layout = channel_layout;
	frame->sample_rate = sample_rate;
	frame->nb_samples = nb_samples;

	if (nb_samples) {
		ret = av_frame_get_buffer(frame, 0);
		if (ret < 0) {
			fprintf(stderr, "Error allocating an audio buffer\n");
			exit(1);
		}
	}

	return frame;
}

static void open_audio(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg)
{
	AVCodecContext *c;
	int nb_samples;
	int ret;
	AVDictionary *opt = NULL;

	c = ost->st->codec;

	/* open it */
	av_dict_copy(&opt, opt_arg, 0);
	ret = avcodec_open2(c, codec, &opt);
	av_dict_free(&opt);
	if (ret < 0) {
		fprintf(stderr, "Could not open audio codec: %s\n", aeo_av_err2str(ret));
		exit(1);
	}

	/* init signal generator */
	ost->t     = 0;
	ost->tincr = 2 * M_PI * 110.0 / c->sample_rate;
	/* increment frequency by 110 Hz per second */
	ost->tincr2 = 2 * M_PI * 110.0 / c->sample_rate / c->sample_rate;

#ifndef AV_CODEC_CAP_VARIABLE_FRAME_SIZE
#define AV_CODEC_CAP_VARIABLE_FRAME_SIZE CODEC_CAP_VARIABLE_FRAME_SIZE
#endif

	if(c->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
		nb_samples = 10000;
	else
		nb_samples = c->frame_size;

	// allocid: MainWindowFrame01
	ost->frame     = alloc_audio_frame(c->sample_fmt, c->channel_layout,
									   c->sample_rate, nb_samples);
	av_log(NULL, AV_LOG_INFO, "ALLOC new: MainWindowFrame01 ost->frame = %p\n",
			ost->frame);
	// allocid: MainWindowFrame02
	ost->tmp_frame = alloc_audio_frame(AV_SAMPLE_FMT_S16, c->channel_layout,
									   c->sample_rate, nb_samples);
	av_log(NULL, AV_LOG_INFO,
			"ALLOC new: MainWindowFrame02 ost->tmp_frame = %p\n",
			ost->frame);

	/* create resampler context */
	// allocid: MainWindowSWR01
	ost->swr_ctx = swr_alloc();
	if (!ost->swr_ctx) {
		fprintf(stderr, "Could not allocate resampler context\n");
		exit(1);
	}
	av_log(NULL, AV_LOG_INFO, "ALLOC new: MainWindowSWR01 ost->swr_ctx = %p\n",
			ost->swr_ctx);

	/* set options */
	av_opt_set_int       (ost->swr_ctx, "in_channel_count",   c->channels,       0);
	av_opt_set_int       (ost->swr_ctx, "in_sample_rate",     c->sample_rate,    0);
	av_opt_set_sample_fmt(ost->swr_ctx, "in_sample_fmt",      AV_SAMPLE_FMT_S16, 0);
	av_opt_set_int       (ost->swr_ctx, "out_channel_count",  c->channels,       0);
	av_opt_set_int       (ost->swr_ctx, "out_sample_rate",    c->sample_rate,    0);
	av_opt_set_sample_fmt(ost->swr_ctx, "out_sample_fmt",     c->sample_fmt,     0);

	/* initialize the resampling context */
	if ((ret = swr_init(ost->swr_ctx)) < 0) {
		fprintf(stderr, "Failed to initialize the resampling context\n");
		exit(1);
	}
}

/* Prepare a 16 bit dummy audio frame of 'frame_size' samples and
 * 'nb_channels' channels. */
AVFrame *MainWindow::get_audio_frame(OutputStream *ost)
{
	AVFrame *frame = ost->tmp_frame;
	int j, i, v;
	int16_t *q = (int16_t*)frame->data[0];
	AVRational avr_one;
	avr_one.num = avr_one.den = 1;

	/* check if we want to generate more frames */
	if (av_compare_ts(ost->next_pts, ost->st->codec->time_base,
					  STREAM_DURATION, avr_one) >= 0)
		return NULL;

	av_log(NULL, AV_LOG_INFO, "Audio generate nb_samples = %d x%d\n",
			frame->nb_samples, ost->st->codec->channels);
	for (j = 0; j <frame->nb_samples; j++) {
		v = (int)(sin(ost->t) * 10000);
		for (i = 0; i < ost->st->codec->channels; i++)
			*q++ = v;
		ost->t     += ost->tincr;
		ost->tincr += ost->tincr2;
	}

	frame->pts = ost->next_pts;
	ost->next_pts  += frame->nb_samples;

	return frame;
}

/*
 * encode one audio frame and send it to the muxer
 * return 1 when encoding is finished, 0 otherwise
 */
int MainWindow::write_audio_frame(AVFormatContext *oc, OutputStream *ost)
{
	AVCodecContext *c;
	AVPacket pkt; // data and size must be 0;
	pkt.data = NULL;
	pkt.size = 0;
	AVFrame *frame;
	int ret;
	int got_packet = 0;
	int dst_nb_samples;

	av_init_packet(&pkt);
	c = ost->st->codec;

#if 0
	frame = get_audio_frame(ost);
#else
	av_log(NULL, AV_LOG_INFO, "frame = this->GetAudioFromQueue()\n");
	frame = this->GetAudioFromQueue();
	av_log(NULL, AV_LOG_INFO, "frame = %p\n", frame);
#endif

	if (frame) {
		/* convert samples from native format to destination codec format, using the resampler */
		/* compute destination number of samples */
		av_log(NULL, AV_LOG_INFO, "dst_nb_samples = av_rescale_rnd(...)\n");
		dst_nb_samples = av_rescale_rnd(
				swr_get_delay(ost->swr_ctx, c->sample_rate) + frame->nb_samples,
				c->sample_rate, c->sample_rate, AV_ROUND_UP);
		av_assert0(dst_nb_samples == frame->nb_samples);

		/* when we pass a frame to the encoder, it may keep a reference to it
		 * internally;
		 * make sure we do not overwrite it here
		 */
		av_log(NULL, AV_LOG_INFO, "av_frame_make_writable(ost->frame)\n");
		ret = av_frame_make_writable(ost->frame);
		if (ret < 0)
			exit(1);

		/* convert to destination format */
		ret = swr_convert(ost->swr_ctx,
				ost->frame->data, dst_nb_samples,
				(const uint8_t **)frame->data, frame->nb_samples);
		if (ret < 0) {
			fprintf(stderr, "Error while converting\n");
			exit(1);
		}
		frame = ost->frame;

		AVRational r;
		r.num = 1;
		r.den = c->sample_rate;

		frame->pts = av_rescale_q(ost->samples_count, r, c->time_base);
		ost->samples_count += dst_nb_samples;

		ret = avcodec_encode_audio2(c, &pkt, frame, &got_packet);
		if (ret < 0) {
			fprintf(stderr, "Error encoding audio frame: %s\n", aeo_av_err2str(ret));
			exit(1);
		}

		if (got_packet) {
			ret = write_frame(oc, &c->time_base, ost->st, &pkt);
			if (ret < 0) {
				fprintf(stderr, "Error while writing audio frame: %s\n",
						aeo_av_err2str(ret));
				exit(1);
			}
		}
	}

	return (frame || got_packet) ? 0 : 1;
}

/**************************************************************/
/* video output */

static AVFrame *alloc_picture(enum AVPixelFormat pix_fmt, int width, int height)
{
	AVFrame *picture;
	int ret;

	picture = av_frame_alloc();
	if (!picture)
		return NULL;

	picture->format = pix_fmt;
	picture->width  = width;
	picture->height = height;

	/* allocate the buffers for the frame data */
	ret = av_frame_get_buffer(picture, 32);
	if (ret < 0) {
		fprintf(stderr, "Could not allocate frame data.\n");
		exit(1);
	}

	return picture;
}

static void open_video(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg)
{
	int ret;
	AVCodecContext *c = ost->st->codec;
	AVDictionary *opt = NULL;

	av_log(NULL, AV_LOG_INFO, "av_dict_copy(&opt, opt_arg, 0)\n");
	av_dict_copy(&opt, opt_arg, 0);

	/* open the codec */
	av_log(NULL, AV_LOG_INFO, "ret = avcodec_open2(c, codec, &opt)\n");
	ret = avcodec_open2(c, codec, &opt);
	av_dict_free(&opt);
	if (ret < 0) {
		fprintf(stderr, "Could not open video codec: %s\n", aeo_av_err2str(ret));
		exit(1);
	}

	/* allocate and init a re-usable frame */
	av_log(NULL, AV_LOG_INFO, "ost->frame = alloc_picture(c->pix_fmt, %d, %d)\n",c->width, c->height);
	// allocid: MainWindowFrame03
	ost->frame = alloc_picture(c->pix_fmt, c->width, c->height);
	if (!ost->frame) {
		fprintf(stderr, "Could not allocate video frame\n");
		exit(1);
	}
	av_log(NULL, AV_LOG_INFO, "ALLOC new: MainWindowFrame03 ost->frame = %p\n",
			ost->frame);

	/* If the output format is not RGBA, then a temporary RGBA
	 * picture is needed too. It is then converted to the required
	 * output format. */
	ost->tmp_frame = NULL;
	if(1 || c->pix_fmt != AV_PIX_FMT_RGBA)
	{
		av_log(NULL, AV_LOG_INFO, "ost->tmp_frame = alloc_picture(AV_PIX_FMT_YUV420P, %d, %d)\n",c->width, c->height);
		// allocid: MainWindowFrame04
		ost->tmp_frame = alloc_picture(AV_PIX_FMT_YUV420P, c->width, c->height);
		if (!ost->tmp_frame) {
			fprintf(stderr, "Could not allocate temporary picture\n");
			exit(1);
		}
		av_log(NULL, AV_LOG_INFO,
				"ALLOC new: MainWindowFrame04 ost->tmp_frame = %p\n",
				ost->tmp_frame);
	}
}

/* Prepare a dummy image. */
static void fill_yuv_image(AVFrame *pict, int frame_index,
						   int width, int height)
{
	int x, y, i, ret;

	/* when we pass a frame to the encoder, it may keep a reference to it
	 * internally;
	 * make sure we do not overwrite it here
	 */
	ret = av_frame_make_writable(pict);
	if (ret < 0)
		exit(1);

	i = frame_index;

	/* Y */
	for (y = 0; y < height; y++)
		for (x = 0; x < width; x++)
			pict->data[0][y * pict->linesize[0] + x] = x + y + i * 3;

	/* Cb and Cr */
	for (y = 0; y < height / 2; y++) {
		for (x = 0; x < width / 2; x++) {
			pict->data[1][y * pict->linesize[1] + x] = 128 + y + i * 2;
			pict->data[2][y * pict->linesize[2] + x] = 64 + x + i * 5;
		}
	}
}

/* Prepare a dummy image. */
static void fill_rgba_image(AVFrame *pict, int frame_index,
						   int width, int height)
{
	int x, y, i, ret;

	/* when we pass a frame to the encoder, it may keep a reference to it
	 * internally;
	 * make sure we do not overwrite it here
	 */
	av_log(NULL, AV_LOG_INFO, "av_frame_make_writable(pict)\n");
	ret = av_frame_make_writable(pict);
	if (ret < 0)
		exit(1);

	i = frame_index;

	av_log(NULL, AV_LOG_INFO, "pict->channels: %d\n", pict->channels);
	av_log(NULL, AV_LOG_INFO, "pict->data[0]: %p\n", pict->data[0]);
	av_log(NULL, AV_LOG_INFO, "pict->data[1]: %p\n", pict->data[1]);
	av_log(NULL, AV_LOG_INFO, "pict->data[2]: %p\n", pict->data[2]);
	av_log(NULL, AV_LOG_INFO, "pict->data[3]: %p\n", pict->data[3]);

	av_log(NULL, AV_LOG_INFO, "fill RGBA\n");
	uint8_t *p;
	for (y = 0; y < height; y++)
	{
		for (x = 0; x < width; x++)
		{
			p = pict->data[0] + (y * pict->linesize[0] + x*4);
			p[0] = x + y + i * 3;
			p[1] = 255;
			p[2] = 0;
			p[3] = 255;
		}
	}
}


AVFrame *MainWindow::get_video_frame(OutputStream *ost)
{
	AVCodecContext *c = ost->st->codec;

#if 0
	/* check if we want to generate more frames */
	if (av_compare_ts(ost->next_pts, ost->st->codec->time_base,
					  STREAM_DURATION, (AVRational){ 1, 1 }) >= 0)
		return NULL;

	if (1 || c->pix_fmt != AV_PIX_FMT_YUV420P) {
		/* as we only generate a YUV420P picture, we must convert it
		 * to the codec pixel format if needed */
		if (!ost->sws_ctx) {
			ost->sws_ctx = sws_getContext(c->width, c->height,
										  AV_PIX_FMT_YUV420P,
										  c->width, c->height,
										  c->pix_fmt,
										  SCALE_FLAGS, NULL, NULL, NULL);
			if (!ost->sws_ctx) {
				fprintf(stderr,
						"Could not initialize the conversion context\n");
				exit(1);
			}
		}
		fill_yuv_image(ost->tmp_frame, ost->next_pts, c->width, c->height);
		sws_scale(ost->sws_ctx,
				  (const uint8_t * const *)ost->tmp_frame->data, ost->tmp_frame->linesize,
				  0, c->height, ost->frame->data, ost->frame->linesize);
	} else {
		fill_yuv_image(ost->frame, ost->next_pts, c->width, c->height);
	}
	//#elif 0
	/* check if we want to generate more frames */
	if (av_compare_ts(ost->next_pts, ost->st->codec->time_base,
					  STREAM_DURATION, (AVRational){ 1, 1 }) >= 0)
		return NULL;

	if (1 || c->pix_fmt != AV_PIX_FMT_RGBA) {
		/* as we only generate a RGBA picture, we must convert it
		 * to the codec pixel format if needed */
		if (!ost->sws_ctx) {
			av_log(NULL, AV_LOG_INFO, "sws_getContext(...)\n");
			ost->sws_ctx = sws_getContext(c->width, c->height,
										  RGB_PIX_FMT,
										  c->width, c->height,
										  c->pix_fmt,
										  SCALE_FLAGS, NULL, NULL, NULL);
			if (!ost->sws_ctx) {
				fprintf(stderr,
						"Could not initialize the conversion context\n");
				exit(1);
			}
		}
		av_log(NULL, AV_LOG_INFO, "fill_rgba_image(encRGBFrame, ost->next_pts, c->width, c->height);\n");
		fill_rgba_image(encRGBFrame, ost->next_pts, c->width, c->height);
		av_log(NULL, AV_LOG_INFO, "sws_scale(...)\n");
		sws_scale(ost->sws_ctx,
				  (const uint8_t * const *)encRGBFrame->data, encRGBFrame->linesize,
				  0, c->height, ost->frame->data, ost->frame->linesize);
	} else {
		fill_rgba_image(ost->frame, ost->next_pts, c->width, c->height);
	}
	#else
	/* check if we have more frames */
	uint8_t *buf;

	// discard the first few frames of video in order to sync to audio
	while(this->encVideoSkip >0)
	{
		av_log(NULL, AV_LOG_INFO, "videoSkip: buf = GetVideoFromQueue()\n");
		buf = GetVideoFromQueue();
		if(buf==NULL) return NULL;
		delete [] buf;
		this->encVideoSkip--;
	}

	av_log(NULL, AV_LOG_INFO, "buf = GetVideoFromQueue()\n");
	buf = GetVideoFromQueue();
	if(buf==NULL) return NULL;

	if(!ost->sws_ctx)
	{
		av_log(NULL, AV_LOG_INFO, "sws_getContext(...)\n");
		// allocid: MainWindowSWS01
		ost->sws_ctx = sws_getContext(c->width, c->height,
				RGB_PIX_FMT,
				c->width, c->height,
				c->pix_fmt,
				SCALE_FLAGS, NULL, NULL, NULL);
		if(!ost->sws_ctx)
			throw AeoException("Could not initialize sws_ctx");
		av_log(NULL, AV_LOG_INFO,
				"ALLOC new: MainWindowSWS01 ost->sws_ctx = %p\n",
				ost->sws_ctx);
	}

	uint8_t **bufHandle = &buf;

	av_frame_make_writable(ost->frame);
	av_log(NULL, AV_LOG_INFO, "sws_scale(...)\n");
	sws_scale(ost->sws_ctx,
			  (const uint8_t * const *)bufHandle, encRGBFrame->linesize,
			  0, c->height, ost->frame->data, ost->frame->linesize);

	delete [] buf;

	#endif

	ost->frame->pts = ost->next_pts++;

	return ost->frame;
}

/*
 * encode one video frame and send it to the muxer
 * return 1 when encoding is finished, 0 otherwise
 */
int MainWindow::write_video_frame(AVFormatContext *oc, OutputStream *ost)
{
	int ret;
	AVCodecContext *c;
	AVFrame *frame;
	int got_packet = 0;
	AVPacket pkt;
	pkt.data = NULL;
	pkt.size = 0;

	c = ost->st->codec;

	av_log(NULL, AV_LOG_INFO, "frame = get_video_frame(ost)\n");
	frame = get_video_frame(ost);

	av_log(NULL, AV_LOG_INFO, "av_init_packet(&pkt)\n");
	av_init_packet(&pkt);

	/* encode the image */
	av_log(NULL, AV_LOG_INFO, "ret = avcodec_encode_video2(c, &pkt, frame, &got_packet)\n");
	ret = avcodec_encode_video2(c, &pkt, frame, &got_packet);
	if (ret < 0) {
		fprintf(stderr, "Error encoding video frame: %s\n", aeo_av_err2str(ret));
		exit(1);
	}

	if (got_packet) {
		av_log(NULL, AV_LOG_INFO, "ret = write_frame(oc, &c->time_base, ost->st, &pkt)\n");
		ret = write_frame(oc, &c->time_base, ost->st, &pkt);
	} else {
		ret = 0;
	}

	if (ret < 0) {
		fprintf(stderr, "Error while writing video frame: %s\n", aeo_av_err2str(ret));
		exit(1);
	}

	return (frame || got_packet) ? 0 : 1;
}

static void close_stream(AVFormatContext *oc, OutputStream *ost)
{
	// allocid: MainWindowStream01
	av_log(NULL, AV_LOG_INFO,
			"ALLOC del: MainWindowStream01 ost->st->codec = %p\n",
			ost->st->codec);
	avcodec_close(ost->st->codec);

	// allocid: MainWindowFrame01
	// allocid: MainWindowFrame03
	av_log(NULL, AV_LOG_INFO, "ALLOC del: MainWindowFrame ost->frame = %p\n",
			ost->frame);
	av_frame_free(&ost->frame);

	// allocid: MainWindowFrame02
	// allocid: MainWindowFrame04
	av_log(NULL, AV_LOG_INFO,
			"ALLOC del: MainWindowFrame ost->tmp_frame = %p\n",
			ost->tmp_frame);
	av_frame_free(&ost->tmp_frame);

	// allocid: MainWindowSWS01
	if(ost->sws_ctx)
	{
		av_log(NULL, AV_LOG_INFO, "ALLOC del: MainWindowSWS01 ost->sws_ctx = %p\n",
				ost->sws_ctx);
		sws_freeContext(ost->sws_ctx);
		ost->sws_ctx = NULL;
	}

	// allocid: MainWindowSWR01
	if(ost->swr_ctx)
	{
		av_log(NULL, AV_LOG_INFO, "ALLOC del: MainWindowSWR01 ost->swr_ctx = %p\n",
				ost->swr_ctx);
		swr_free(&ost->swr_ctx);
	}
}

/**************************************************************/
/* media file output */

int MainWindow::MuxMain(const char *fn_arg, long startFrame, long numFrames,
		long vidFrameOffset, QProgressDialog &progress)
{
	int argc = 2;
	const char *argv[] = { "mux_main", fn_arg };
	OutputStream video_st, audio_st;
	memset(&video_st, 0, sizeof(OutputStream));
	memset(&audio_st, 0, sizeof(OutputStream));
	const char *filename = NULL;
	AVOutputFormat *fmt = NULL;
	AVFormatContext *oc = NULL;
	AVCodec *audio_codec = NULL;
	AVCodec *video_codec = NULL;
	int ret;
	int have_video = 0, have_audio = 0;
	int encode_video = 0, encode_audio = 0;
	AVDictionary *opt = NULL;

	// can only do MuxMain once without risking a crash, so mark it now.
	this->isVideoMuxingRisky = true;

	av_log(NULL, AV_LOG_INFO, "MuxMain(\"%s\", %ld, %ld, %ld, progress)\n",
			fn_arg, startFrame, numFrames, vidFrameOffset);


	if (argc < 2) {
		throw AeoException("Invalid call to MuxMain");
	}

	// Call only once (I think libav can handle additional calls all right,
	// but just in case it doesn't...)
	static bool needRegisterAll = true;
	if(needRegisterAll)
	{
		/* Initialize libavcodec, and register all codecs and formats. */
		av_log(NULL, AV_LOG_INFO, "av_register_all()\n");

		av_register_all();
		needRegisterAll = false;
	}

	filename = argv[1];
	/*
	if (argc > 3 && !strcmp(argv[2], "-flags")) {
		av_dict_set(&opt, argv[2]+1, argv[3], 0);
	}
	*/

	/* allocate the output media context */
	av_log(NULL, AV_LOG_INFO, "avformat_alloc_output_context2(&oc, NULL, NULL, filename)\n");
	// allocid: MainWindowOutput01
	avformat_alloc_output_context2(&oc, NULL, NULL, filename);
	if (!oc) {
		printf("Could not deduce output format from file extension: using MPEG.\n");
		avformat_alloc_output_context2(&oc, NULL, "mpeg", filename);
	}
	av_log(NULL, AV_LOG_INFO, "ALLOC new: MainWindowOutput01 oc = %p\n", oc);
	if (!oc)
		return 1;

	fmt = oc->oformat;

	/* Add the audio and video streams using the default format codecs
	 * and initialize the codecs. */
	if (fmt->video_codec != AV_CODEC_ID_NONE) {
		int fps_timbase =24;
		if (ui->filmrate_PD->currentIndex()>1) fps_timbase=25;

		video_st.requestedTimeBase = fps_timbase;

		//video_st.requestedWidth = this->scan.inFile.Width();
		//video_st.requestedHeight = this->scan.inFile.Height();
		video_st.requestedWidth = 640;
		video_st.requestedHeight = 480;

		av_log(NULL, AV_LOG_INFO, "add_stream(&video_st, oc, &video_codec, fmt->video_codec)\n");
		add_stream(&video_st, oc, &video_codec, fmt->video_codec);
		have_video = 1;
		encode_video = 1;
	}
	if (fmt->audio_codec != AV_CODEC_ID_NONE) {
		int samplerate = (this->ui->filerate_PD->currentIndex()+1)*48000;

		int frameratesamples ;
		int fps_timbase =24;

		if (ui->filmrate_PD->currentIndex()==0)
			frameratesamples = (int) (samplerate/23.976);
		else if (ui->filmrate_PD->currentIndex()==1)
			frameratesamples = (int) (samplerate/24.0);
		else
		{
			frameratesamples =  (int) (samplerate/25.0);
			fps_timbase=25;
		}

		av_log(NULL, AV_LOG_INFO, "Requested sample rate: %d\n", samplerate);
		audio_st.requestedSamplingRate = samplerate;
		audio_st.requestedSamplesPerFrame = frameratesamples;
		audio_st.requestedTimeBase = fps_timbase;

		av_log(NULL, AV_LOG_INFO, "add_stream(&audio_st, oc, &audio_codec, fmt->audio_codec)\n");
		add_stream(&audio_st, oc, &audio_codec, fmt->audio_codec);
		have_audio = 1;
		encode_audio = 1;
	}

	/* Now that all the parameters are set, we can open the audio and
	 * video codecs and allocate the necessary encode buffers. */
	if (have_video)
		open_video(oc, video_codec, &video_st, opt);

	/*
	av_log(NULL, AV_LOG_INFO, "this->encRGBFrame = alloc_picture(AV_PIX_FMT_RGBA,%d,%d)\n",
			this->scan.inFile.Width(), this->scan.inFile.Height());
	this->encRGBFrame = alloc_picture(RGB_PIX_FMT,
			this->scan.inFile.Width(), this->scan.inFile.Height());
	*/
	this->encRGBFrame = av_frame_alloc();
	if (!this->encRGBFrame) {
		av_log(NULL, AV_LOG_INFO, "Could not allocate RGBFrame picture\n");
		throw AeoException("Could not allocate RGBFrame picture");
	}
	this->encRGBFrame->format = RGB_PIX_FMT;
	this->encRGBFrame->width = 640;
	this->encRGBFrame->height = 480;
	if(av_frame_get_buffer(this->encRGBFrame, 32) < 0)
		throw AeoException("Could not allocate encRGBFrame buffer");

	if (have_audio)
		open_audio(oc, audio_codec, &audio_st, opt);

	av_dump_format(oc, 0, filename, 1);

	av_log(NULL, AV_LOG_INFO, "encS16Frame = audio_st.tmp_frame = %p\n", audio_st.tmp_frame);
	this->encS16Frame = audio_st.tmp_frame;

	/* open the output file, if needed */
	if (!(fmt->flags & AVFMT_NOFILE)) {
		ret = avio_open(&oc->pb, filename, AVIO_FLAG_WRITE);
		if (ret < 0) {
			fprintf(stderr, "Could not open '%s': %s\n", filename,
					aeo_av_err2str(ret));
			return 1;
		}
	}

	/* Write the stream header, if any. */
	ret = avformat_write_header(oc, &opt);
	if (ret < 0) {
		fprintf(stderr, "Error occurred when opening output file: %s\n",
				aeo_av_err2str(ret));
		return 1;
	}

	this->encCurFrame = 2;
	this->encStartFrame = startFrame;
	this->encNumFrames = numFrames;

	if(vidFrameOffset > 0)
	{
		this->encVideoSkip = vidFrameOffset;
		this->encAudioSkip = 0;
		this->encVideoPad = 0;
		this->encAudioPad = 0;
	}
	else
	{
		this->encVideoSkip = 0;
		this->encAudioSkip = -vidFrameOffset * this->frame_window->samplesperframe_file;
		av_log(NULL, AV_LOG_INFO, "encAudioSkip = %ld\n", this->encAudioSkip);
		this->encVideoPad = 0;
		this->encAudioPad = 0;
	}

	// this had been set to vid.VideoFrameIn->data[0] in the working code
	// via calls to VideoEncoder::ExampleVideoFrame, which set
	// outputFrameTexture->buf to videoFrameIn->data[0], and
	// Frame_Window::PrepareVideoOutput, which set
	// vo.videobuffer to outputFrameTexture->buf

	this->frame_window->vo.videobuffer = this->encRGBFrame->data[0];

	this->encVideoBufSize = 640 * 480 * 4;

	// queue up some black frames to sync the video:
	uint8_t *blackFrame;
	for(int i=0; i<this->encVideoPad; ++i)
	{
		const uint8_t black[4] = { 0, 0, 0, 0xFF };
		blackFrame = new uint8_t[this->encVideoBufSize];
		for(long p = 0; p<this->encVideoBufSize; p+=4)
			memcpy(blackFrame+p, black, 4);
		this->encVideoQueue.push(blackFrame);
	}

	while (encode_video || encode_audio) {
		/* select the stream to encode */
		if (encode_video &&
			(!encode_audio || av_compare_ts(video_st.next_pts, video_st.st->codec->time_base,
											audio_st.next_pts, audio_st.st->codec->time_base) <= 0)) {
			av_log(NULL, AV_LOG_INFO, "encode_video = !write_video_frame(oc, &video_st)\n");
			encode_video = !write_video_frame(oc, &video_st);
		} else {
			av_log(NULL, AV_LOG_INFO, "encode_audio = !write_audio_frame(oc, &audio_st)\n");
			encode_audio = !write_audio_frame(oc, &audio_st);
		}
		progress.setValue(this->encCurFrame);
		if(progress.wasCanceled())
		{
			this->requestCancel = true;
			throw 2;
		}
	}

	/* Write the trailer, if any. The trailer must be written before you
	 * close the CodecContexts open when you wrote the header; otherwise
	 * av_write_trailer() may try to use memory that was freed on
	 * av_codec_close(). */
	av_write_trailer(oc);

	/* Close each codec. */
	if(have_video)
		close_stream(oc, &video_st);

	this->frame_window->vo.videobuffer = NULL;
	av_frame_free(&this->encRGBFrame);

	if(have_audio)
		close_stream(oc, &audio_st);

	if (!(fmt->flags & AVFMT_NOFILE))
		/* Close the output file. */
		avio_closep(&oc->pb);

	/* free the streams */
	// allocid: MainWindowOutput01
	av_log(NULL, AV_LOG_INFO, "ALLOC del: MainWindowOutput01 oc = %p\n", oc);
	avformat_free_context(oc);
	oc = NULL;

	return 0;
}

#undef STREAM_DURATION
#undef STREAM_PIX_FMT
#undef SCALE_FLAGS

//----------------------------------------------------------------------------
//---------------------- END MUX.C -------------------------------------------
//----------------------------------------------------------------------------
#endif

extern "C" void SegvHandler(int param)
{
	longjmp(segvJumpEnv, 1);
}

bool MainWindow::WriteAudioToFile(const char *fn, const char *videoFn,
		long firstFrame, long numFrames)
{
	bool ret;

	// changing this requires many changes to underlying structures
	const int numChannels = 2;

	ret = true;

	if(videoFn && videoFn[0] == '\0') videoFn = NULL;

	av_log(NULL, AV_LOG_INFO, "WriteAudio: %s\n", fn);
	if(videoFn)
		av_log(NULL, AV_LOG_INFO, "WriteVideo: %s\n", videoFn);
	else
		av_log(NULL, AV_LOG_INFO, "WriteVideo: -no-\n");

	QProgressDialog progress("Extracting Audio...","Cancel",0,numFrames);
	progress.setWindowModality(Qt::WindowModal);
	progress.setWindowTitle("Extracting Audio...");
	progress.setMinimumDuration(0);

	this->requestCancel = false;

	progress.setLabelText("Audio Rendering...");
	progress.setValue(0);

	//qApp->processEvents();

	frame_window->overrideOverlap = 0;

	int samplerate = (ui->filerate_PD->currentIndex()+1)*48000;

	int frameratesamples ;
	int fps_timbase =24;

	if (ui->filmrate_PD->currentIndex()==0)
		frameratesamples = (int) (samplerate/23.976);
	else if (ui->filmrate_PD->currentIndex()==1)
		frameratesamples = (int) (samplerate/24.0);
	else
	{
		frameratesamples =  (int) (samplerate/25.0);
		fps_timbase=25;
	}

	AudioFromTexture audio(numChannels, samplerate, frameratesamples);

	outputFrameTexture= new FrameTexture();
	outputFrameTexture->width=640;
	outputFrameTexture->height=480;

	// See Reference Point frame_view_gl.cpp:LSJ-20170519-1322
	outputFrameTexture->format=GL_UNSIGNED_INT_8_8_8_8_REV;
	outputFrameTexture->bufSize = 640*480*4;
	outputFrameTexture->isNonNativeEndianess = false;
	outputFrameTexture->nComponents = 4;

	#ifndef USE_MUX_HACK
		VideoEncoder vid("a.mov");
	#endif

	if(videoFn)
	{
		#ifndef USE_MUX_HACK
			// set the incoming size and pixel format
			vid.ExampleVideoFrame(this->outputFrameTexture);
			vid.ExampleAudioFrame(&audio);
		#endif

		frame_window->PrepareVideoOutput(this->outputFrameTexture);
		frame_window->is_videooutput = true;
	}

	wav wout(samplerate);
	wout.nChannels = numChannels;

	wout.samplesPerFrame = frameratesamples;
	wout.bitsPerSample = frame_window->bit_depth;

	if(this->currentMeta)
	{
		memset(wout.Originator, 0, 32);
		strncpy(wout.Originator, qPrintable(this->currentMeta->originator), 32);

		memset(wout.OriginatorReference, 0, 32);
		strncpy(wout.OriginatorReference,
				qPrintable(this->currentMeta->originatorReference), 32);

		memset(wout.Description, 0, 256);
		strncpy(wout.Description,
				qPrintable(this->currentMeta->description), 256);

		wout.Version = this->currentMeta->version;

		wout.TimeReferenceHigh = this->currentMeta->timeReference >> 32;
		wout.TimeReferenceLow = this->currentMeta->timeReference;

		memset(wout.CodingHistory, 0, 100);
		strncpy(wout.CodingHistory,
				qPrintable(this->currentMeta->codingHistory), 100);

		// INFO block code is below. It executes after writing data.
	}

	strncpy(wout.OriginationDate,
			qPrintable(QDateTime::currentDateTime().toString("yyyy-MM-dd")),
			10);
	strncpy(wout.OriginationTime,
			qPrintable(QDateTime::currentDateTime().toString("hh:mm:ss")),8);

	frame_window->samplesperframe_file =frameratesamples;
	frame_window->PrepareRecording(numFrames * frameratesamples);

	try
	{
		traceCurrentOperation = "Opening Output File";
		if(wout.open(fn) == NULL) throw 1;

		#ifndef USE_MUX_HACK
		if(videoFn)
		{
			traceCurrentOperation = "Opening video mux output file";
			vid.Open();
		}
		#endif

		traceCurrentOperation = "Load Base Texture";
		if(!Load_Frame_Texture(firstFrame + 0)) throw 1;
		frame_window->is_rendering=true;

		unsigned int sec;
		unsigned int frames;
		QStringList TCL = this->scan.inFile.TimeCode.split(
					QRegExp("[:]"),QString::SkipEmptyParts);
		sec =(((TCL[0].toInt() * 3600 )+ (TCL[1].toInt()* 60)+TCL[2].toInt()));
		frames = TCL[3].toInt() +firstFrame +( sec * fps_timbase);
		frames += ui->advance_CB->currentIndex();
		sec=frames/fps_timbase;
		frames= frames%fps_timbase;
		wout.set_timecode(sec,frames);

		traceCurrentOperation = "Load Texture";
		if(!Load_Frame_Texture(firstFrame + 1)) throw 1;

		#ifdef USE_MUX_HACK
		if(videoFn)
		{
			traceCurrentOperation = "Mux";
			MuxMain(videoFn, firstFrame, numFrames,
					/* frame skip =  */ui->advance_CB->currentIndex(),
					progress);
		}
		else
		#endif
		{
			for (long a = 2; a<= numFrames; a++)
			{
				traceCurrentOperation = "Load Texture";\
				if (a + firstFrame > this->scan.inFile.NumFrames()-1 )
				{
					// TODO: correct the sound array allocation steps above
					// so that we don't have to load the final frame twice
					if(!Load_Frame_Texture(firstFrame + a - 1)) break;
				}
				else
				{
					if(!Load_Frame_Texture(firstFrame + a)) break;
				}

				#ifndef USE_MUX_HACK
				frame_window->read_frame_texture(this->outputFrameTexture);
				vid.WriteVideoFrame(this->outputFrameTexture);
				#endif

				audio.buf[0] = frame_window->FileRealBuffer[0] +
						(a-2)*frameratesamples;
				if(audio.nChannels == 2)
				{
					audio.buf[1] = frame_window->FileRealBuffer[1] +
							(a-2)*frameratesamples;
				}

				av_log(NULL, AV_LOG_INFO, "audio.buf from FileRealBuffer = [%p,%p]\n",
						frame_window->FileRealBuffer[0],
						frame_window->FileRealBuffer[1]);
				av_log(NULL, AV_LOG_INFO, "offset of %ld*%d yeilds buf = [%p,%p]\n",
						(a-2),frameratesamples, audio.buf[0], audio.buf[1]);

				#ifndef USE_MUX_HACK
				vid.WriteAudioFrame(&audio);
				#endif

				traceCurrentOperation = "Update Progress Bar";

				progress.setValue(a);
				traceCurrentOperation = "Process GUI events";

				if(progress.wasCanceled())
				{
					this->requestCancel = true;
					throw 2;
				}

				//if(this->requestCancel) throw 2;
			}
		}

		frame_window->is_rendering =false;
		traceCurrentOperation = "Process Recording";
		frame_window->ProcessRecording(numFrames * frameratesamples);

		traceCurrentOperation = "Writing wav file";
		wout.writebuffer(frame_window->FileRealBuffer,
				numFrames * frameratesamples);

		// INFO chunk
		traceCurrentOperation = "Writing Info block";
		wout.BeginInfoChunk();
		wout.AddInfo("ICRD",
				qPrintable(QDate::currentDate().toString("yyyy-MM-dd")));

		if(this->currentMeta)
		{
			wout.AddInfo("IARL",
					qPrintable(this->currentMeta->archivalLocation));
			wout.AddInfo("ICMT",
					qPrintable(this->currentMeta->comment));
			wout.AddInfo("ICOP",
					qPrintable(this->currentMeta->copyright));
		}

		wout.EndInfoChunk();

		traceCurrentOperation = "Closing wav file";
		wout.close();
		traceCurrentOperation = "";

		#ifndef USE_MUX_HACK
		traceCurrentOperation = "closing video mux output file";
		vid.Close();
		#endif
	}
	catch(int e)
	{
		// silently absorb the exception
		if(e==1) ret = false;
	}
	catch(...)
	{
		throw;
	}

	// clean up:

	frame_window->is_rendering =false;
	frame_window->DestroyRecording();

	if(this->requestCancel)
		ret = false;

	return ret;
}

//*********************IMAGE PROCESSING UI ************************************
QString MainWindow::Compute_Timecode_String(int position)
{
	unsigned int sec;
	unsigned int frames;
	int fps_timebase=24;
	int h,m,s,f;
	if(ui->filmrate_PD->currentIndex()>1)
		fps_timebase=25;

	QStringList TCL =  this->scan.inFile.TimeCode.split(QRegExp("[:]"),QString::SkipEmptyParts);
	sec =(((TCL[0].toInt() * 3600 )+ (TCL[1].toInt()* 60)+TCL[2].toInt()));
	frames = TCL[3].toInt() +position;
	sec+=frames/fps_timebase;
	frames= frames%fps_timebase;

	h = (sec/3600);
	sec -= h*3600;
	m = sec/60;
	sec -=m*60;
	s = sec +  (frames/fps_timebase);
	f= frames%fps_timebase;

	//return  return QString::number(h,)+":"+QString::number(m)+":" +QString::number(s)+":"+QString::number(f);

	return  QString("%1").arg(h, 2, 10, QChar('0'))+":"+
			QString("%1").arg(m, 2, 10, QChar('0'))+":"+
			QString("%1").arg(s, 2, 10, QChar('0'))+":"+
			QString("%1").arg(f, 2, 10, QChar('0'));
}

uint64_t MainWindow::ComputeTimeReference(int position, int samplingRate)
{
	unsigned int sec;
	unsigned int frames;
	int fps_timebase=24;
	int h,m,s,f;
	if(ui->filmrate_PD->currentIndex()>1)
		fps_timebase=25;

	uint64_t reference;

	QStringList TCL =  this->scan.inFile.TimeCode.split(
			QRegExp("[:]"),QString::SkipEmptyParts);
	sec =(((TCL[0].toInt() * 3600 )+ (TCL[1].toInt()* 60)+TCL[2].toInt()));
	frames = TCL[3].toInt() +position;

	sec+=frames/fps_timebase;
	frames= frames%fps_timebase;

	reference = sec;
	reference *= samplingRate;
	reference += uint64_t(
			double(samplingRate)*double(frames)/double(fps_timebase));

	return reference;
}

void MainWindow::on_gammaSlider_valueChanged(int value)
{
	ui->gammaLabel->setText(QString::number(value/100.f,'f',2));
	GPU_Params_Update(1);
}

void MainWindow::on_liftSlider_valueChanged(int value)
{
	ui->liftLabel->setText(QString::number(value/100.f,'f',2));
	GPU_Params_Update(1);
}
void MainWindow::on_gainSlider_valueChanged(int value)
{
	ui->gainLabel->setText(QString::number(value/100.f,'f',2));
	GPU_Params_Update(1);
}

void MainWindow::on_thresholdSlider_valueChanged(int value)
{
	ui->threshLabel->setText(QString::number(value/100.f,'f',2));
	GPU_Params_Update(1);
}
void MainWindow::on_blurSlider_valueChanged(int value)
{
	ui->blurLabel->setText(QString::number(value/100.f,'f',2));
	GPU_Params_Update(1);
}

void MainWindow::on_FramePitchendSlider_sliderMoved(int position)
{

}

void MainWindow::on_leftPixSlider_valueChanged(int value)
{
	GPU_Params_Update(1);
}

void MainWindow::on_rightPixSlider_valueChanged(int value)
{
	GPU_Params_Update(1);
}

void MainWindow::on_smoothingMethodComboBox_2_currentIndexChanged(int index)
{
	GPU_Params_Update(1);
}

void MainWindow::on_negBox_clicked()
{
	GPU_Params_Update(1);
}

void MainWindow::on_threshBox_clicked()
{
	GPU_Params_Update(1);
}

void MainWindow::on_FramePitchendSlider_valueChanged(int value)
{
	ui->framePitchLabel->setText(QString::number(value/1000.f,'f',2));
	GPU_Params_Update(1);
}

void MainWindow::on_lift_resetButton_clicked()
{
	ui->liftSlider->setValue(0);
}

void MainWindow::on_gamma_resetButton_clicked()
{
	ui->gammaSlider->setValue(100);
}

void MainWindow::on_gain_resetButton_clicked()
{
	ui->gainSlider->setValue(100);
}

void MainWindow::on_thresh_resetButton_clicked()
{
	ui->thresholdSlider->setValue(300);
}

void MainWindow::on_blur_resetButton_clicked()
{
	ui->blurSlider->setValue(0);
}

void MainWindow::on_desatBox_clicked()
{
	GPU_Params_Update(1);
}

void MainWindow::on_OverlapSoundtrackCheckBox_stateChanged(int arg1)
{
	GPU_Params_Update(1);
}

void MainWindow::on_OverlapPixCheckBox_stateChanged(int arg1)
{
	GPU_Params_Update(1);
}

void MainWindow::on_MainWindow_destroyed()
{
	if(frame_window)
	{
		frame_window->close();
		delete frame_window;
	}
}

void MainWindow::on_CalBtn_clicked()
{
	if(this->frame_window!=NULL)
	{
		this->ui->CalEnableCB->setChecked(false);
		this->frame_window->cal_enabled=false;
		this->frame_window->is_caling=true;

		QProgressDialog progress("Calibrating...", "Cancel", 0, 50);
		progress.setWindowTitle("Calibrating");
		progress.setMinimumDuration(0);
		bool canceled = false;
		progress.setWindowModality(Qt::WindowModal);
		progress.setValue(0);

		this->frame_window->clear_cal=true;
		GPU_Params_Update(1);

		int rnd;
		int floor = 0;
		int ceiling = this->scan.lastFrameIndex-this->scan.firstFrameIndex;
		int range = (ceiling - floor);

		while(this->frame_window->calval<0.5)
		{
			//srand((unsigned)time(0));

			rnd = floor+int(rand()%range);
			Load_Frame_Texture((rnd));

			progress.setValue(100*this->frame_window->calval);
			if(progress.wasCanceled())
			{
				canceled = true;
				break;
			}
		}
		progress.setValue(50); // auto hides the window

		this->frame_window->is_caling=false;
		if(!canceled)
		{
			this->ui->CalEnableCB->setEnabled(true);
			this->ui->CalEnableCB->setChecked(true);
			this->frame_window->cal_enabled=true;
		}
		GPU_Params_Update(1);
	}
}

void MainWindow::on_CalEnableCB_clicked()
{
	if (this->ui->CalEnableCB->isChecked())
	{
		this->frame_window->cal_enabled=true;
	}
	else
	{
		this->frame_window->cal_enabled=false;
	}
	GPU_Params_Update(1);
}

void MainWindow::on_actionAcknowledgements_triggered()
{
	QDesktopServices::openUrl(QUrl("http://imi.cas.sc.edu/mirc/"));
}

void MainWindow::on_actionAbout_triggered()
{
	QMessageBox::information(NULL,
			QString("AEO-Light v")+QString(APP_VERSION_STR),
			"AEO-Light is an open-source software that extracts audio "
			"from optical sound tracks of motion picture film. AEO-Light is "
			"produced at the University of South Carolina by a team comprised "
			"of faculty and staff from the University Libraries' Moving "
			"Image Research Collections (MIRC) and the College of Arts and "
			"Sciences Interdisciplinary Mathematics Institute (IMI), with "
			"contributions from Tommy Aschenbach (Video & Film Solutions). "
			"\n\n"
			"Project funding comes from the Preservation and Access Division"
			"of the National Endowment for the Humanities. AEO-Light is "
			"available through an open-source licensing agreement. The "
			"complete terms are available in the AEO-Light Documentation."
			"\n\n"
			"This software uses libraries from the FFmpeg project under "
			"the GPLv2.0"
			);
}


void MainWindow::DeleteTempSoundFile(void)
{
	try
	{
		ExtractedSound sample;
		ExtractedSound emptySound;
		for(int i=0; i<samplesPlayed.size(); ++i)
		{
			sample = samplesPlayed[i];
			if(sample.sound != NULL)
			{
				sample.sound->stop();
				QFile::remove(sample.sound->fileName());
			}
			samplesPlayed[i] = emptySound;
		}
	}
	catch(std::exception &e)
	{
		QMessageBox w;
		QString msg("Error deleting temp file: \n");
		int answer;

		msg += QString(e.what());

		w.setText(msg);
		w.setStandardButtons(QMessageBox::Abort | QMessageBox::Ok);
		w.setDefaultButton(QMessageBox::Ok);
		answer = w.exec();

		if(answer == QMessageBox::Abort) exit(1);
		return;
	}
}

//----------------------------------------------------------------------------
QTextStream &MainWindow::Log()
{
	#ifdef _WIN32
	static QFile logfile(QString("%1/%2").arg(QDir::homePath(),
			"AEO-log.txt"));
	#else
	static QFile logfile(QString("%1/%2").arg(QDir::homePath(),
			".aeolight.log.txt"));
	#endif

	if(!log.device() || !log.device()->isOpen())
	{
		logfile.open(QIODevice::WriteOnly);
		log.setDevice(&logfile);
	}

	log.flush();

	return log;
}

void MainWindow::LogSettings()
{
	Log() << "\n----- OpenGL SETTINGS -----\n";
	saveproject(Log());
	Log() << "---------------------------\n";

	if(scan.soundBounds.size() > 0)
	{
		Log() << "\n----- PROJECT SETTINGS -----\n";
		Log() << "FirstFrame: " << scan.firstFrameIndex << "\n";
		Log() << "LastFrame: " << scan.lastFrameIndex << "\n";
		//Log() << "Frames Per Second: " << scan.framesPerSecond << "\n";

		Log() << "Number of soundtracks: " <<
				this->scan.soundBounds.size() << "\n";

		for(int i=0; i<this->scan.soundBounds.size(); ++i)
			Log() << "Sound bounds: " << (scan.soundBounds[i].Left()) <<
					" - " << (scan.soundBounds[i].Right()) << "\n";

		Log() << "----------------------------\n";
	}
}

void MainWindow::on_monostereo_PD_currentIndexChanged(int index)
{
	frame_window->stereo = float(index);
}

void MainWindow::on_actionShow_Soundtrack_Only_triggered()
{
	ui->showSoundtrackOnlyCheckBox->setChecked(
			ui->actionShow_Soundtrack_Only->isChecked());
	GPU_Params_Update(1);
}

void MainWindow::on_actionWaveform_Zoom_triggered()
{
	ui->waveformZoomCheckBox->setChecked(ui->actionWaveform_Zoom->isChecked());
	GPU_Params_Update(1);
}

void MainWindow::on_actionShow_Overlap_triggered()
{
	ui->showOverlapCheckBox->setChecked(ui->actionShow_Overlap->isChecked());
	GPU_Params_Update(1);
}

void MainWindow::on_frameInSpinBox_valueChanged(int arg1)
{
      ui->frameOutTimeCodeLabel->setText(Compute_Timecode_String(arg1 - this->scan.inFile.FirstFrame()));
}

void MainWindow::on_cancelButton_clicked()
{
	this->requestCancel = true;
}

void MainWindow::on_waveformZoomCheckBox_clicked(bool checked)
{
	ui->actionWaveform_Zoom->setChecked(checked);
	GPU_Params_Update(1);
}

void MainWindow::on_showOverlapCheckBox_clicked(bool checked)
{
	ui->actionShow_Overlap->setChecked(checked);
	GPU_Params_Update(1);
}

void MainWindow::on_showSoundtrackOnlyCheckBox_clicked(bool checked)
{
	ui->actionShow_Soundtrack_Only->setChecked(checked);
	GPU_Params_Update(1);
}

void MainWindow::on_actionOpen_Source_triggered()
{
	on_sourceButton_clicked();
}

void MainWindow::on_actionLoad_Settings_triggered()
{
	on_loadprojectButton_clicked();
}

void MainWindow::on_actionSave_Settings_triggered()
{
	on_saveprojectButton_clicked();
}

void MainWindow::on_actionQuit_triggered()
{
	std::exit(0);
}

void MainWindow::on_frame_numberSpinBox_editingFinished()
{

}

void MainWindow::on_frameOutSpinBox_valueChanged(int arg1)
{
	ui->frameOutTimeCodeLabel->setText(Compute_Timecode_String(arg1 - this->scan.inFile.FirstFrame()));
}


void MainWindow::on_enqueueButton_clicked()
{
	if(this->extractQueue.size() >= 5) return; // queue full

	// Ask for output filename
	QString expDir;

	if(this->prevExportDir.isEmpty())
	{
		QSettings settings;
		settings.beginGroup("default-folder");
		expDir = settings.value("export").toString();
		if(expDir.isEmpty())
		{
			expDir = QStandardPaths::writableLocation(
						QStandardPaths::DocumentsLocation);
		}
		settings.endGroup();
	}
	else
	{
		expDir = this->prevExportDir;
	}

	QString filename = QFileDialog::getSaveFileName(
				this,tr("Export audio to"),expDir,"*.wav");
	if(filename.isEmpty()) return;

	this->prevExportDir = QFileInfo(filename).absolutePath();

	ExtractTask task;
	task.output = filename;
	task.source = QString(this->scan.inFile.GetFileName().c_str());
	task.srcFormat = this->scan.inFile.GetFormat();

	qDebug() << task.source;

	task.params = ExtractionParamsFromGUI();
	this->extractQueue.push_back(task);

	UpdateQueueWidgets();
}

void MainWindow::UpdateQueueWidgets(void)
{
	int i;

	QLabel *label[5];
	label[0] = ui->queue1Label;
	label[1] = ui->queue2Label;
	label[2] = ui->queue3Label;
	label[3] = ui->queue4Label;
	label[4] = ui->queue5Label;

	QPushButton *btn[5];
	btn[0] = ui->queueDelete1Button;
	btn[1] = ui->queueDelete2Button;
	btn[2] = ui->queueDelete3Button;
	btn[3] = ui->queueDelete4Button;
	btn[4] = ui->queueDelete5Button;

	for(i=0; i<this->extractQueue.size(); ++i)
	{
		label[i]->setText(
				QString("%1-%2 %3").
				arg(extractQueue[i].params.frameIn).
				arg(extractQueue[i].params.frameOut).
				arg(QFileInfo(extractQueue[i].source).fileName()));
		label[i]->setEnabled(true);
		btn[i]->setEnabled(true);
	}
	ui->queueExtractButton->setEnabled(i>0);
	for(i=this->extractQueue.size(); i<5; ++i)
	{
		label[i]->setText("Not Set");
		label[i]->setEnabled(false);
		btn[i]->setEnabled(false);
	}
}


void MainWindow::on_queueDelete1Button_clicked()
{
	this->extractQueue.erase(extractQueue.begin());
	UpdateQueueWidgets();
}

void MainWindow::on_queueDelete2Button_clicked()
{
	this->extractQueue.erase(extractQueue.begin()+1);
	UpdateQueueWidgets();
}

void MainWindow::on_queueDelete3Button_clicked()
{
	this->extractQueue.erase(extractQueue.begin()+2);
	UpdateQueueWidgets();
}

void MainWindow::on_queueDelete4Button_clicked()
{
	this->extractQueue.erase(extractQueue.begin()+3);
	UpdateQueueWidgets();
}

void MainWindow::on_queueDelete5Button_clicked()
{
	this->extractQueue.erase(extractQueue.begin()+4);
	UpdateQueueWidgets();
}

void MainWindow::on_queueExtractButton_clicked()
{
	bool success;

	while(this->extractQueue.size()>0)
	{
		if(!this->NewSource(
				this->extractQueue[0].source,
				this->extractQueue[0].srcFormat))
			break;

		this->ExtractionParametersToGUI(this->extractQueue[0].params);

		success = this->extractGL(this->extractQueue[0].output);

		if(success)
		{
			this->extractQueue.erase(extractQueue.begin());
			UpdateQueueWidgets();
		}
		else
			break;
	}
}

void MainWindow::on_soundtrackDefaultsButton_clicked()
{
	SaveDefaultsSoundtrack();
}

void MainWindow::on_imageDefaultsButton_clicked()
{
	SaveDefaultsImage();
}

void MainWindow::on_extractDefaultsButton_clicked()
{
	SaveDefaultsAudio();
}

void MainWindow::on_actionPreferences_triggered()
{
	preferencesdialog *pref = new preferencesdialog(this);
	pref->setWindowTitle("Preferences");
	pref->exec();

	// the dialog itself modifies the app's preferences if accepted,
	// so there's no additional processing to do here.

	delete pref;
}

void MainWindow::on_actionReport_or_track_an_issue_triggered()
{
	QDesktopServices::openUrl(QUrl("https://github.com/usc-imi/aeo-light/issues"));
}

void MainWindow::on_OverlapPixCheckBox_clicked(bool checked)
{
	ui->pixLabel->setEnabled(checked);
	ui->leftPixSpinBox->setEnabled(checked);
	ui->rightPixSpinBox->setEnabled(checked);
	ui->leftPixSlider->setEnabled(checked);
	ui->rightPixSlider->setEnabled(checked);
}
