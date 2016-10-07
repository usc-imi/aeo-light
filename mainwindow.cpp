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

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "wav.h"
#include "writexml.h"
#include "project.h"
#include <algorithm>
#include <QDebug>
#include <QFileDialog>
#include <QFile>
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

#include "aeoexception.h"

#include "mainwindow.h"
#include "savesampledialog.h"

extern "C" void SegvHandler(int param);
static jmp_buf segvJumpEnv;
static long lastFrameLoad = 0;
static const char *traceCurrentOperation = NULL;
static const char *traceSubroutineOperation = NULL;

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
	return QString("AEO-Light v2.2 beta (" __DATE__ ")");
}


// -------------------------------------------------------------------
// Constructor
// -------------------------------------------------------------------

MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent),
	ui(new Ui::MainWindow)
{
	ui->setupUi(this);

	ui->appNameLabel->setText(
			QString("<html><head/><body><p><span style=\" font-size:24pt;\">")
			+ Version() + QString("</span></p></body></html>"));

#ifdef USE_SERIAL
	ui->calibrateCheckBox->setChecked(this->scan.doCalibrate);
	ui->smoothingMethodComboBox->setCurrentIndex(this->scan.smoothingMethod);
	ui->mAvgSweepsSpinBox->setValue(this->scan.movingAverageNumSweeps);
	ui->mAvgSpanSpinBox->setValue(this->scan.movingAverageHalfSpan);

	QString polyDeg("");
	if(this->scan.polynomialFitDegreeFlags & 0x01) polyDeg += "1";
	if(this->scan.polynomialFitDegreeFlags & 0x02) polyDeg += "2";
	if(this->scan.polynomialFitDegreeFlags & 0x04) polyDeg += "3";
	if(this->scan.polynomialFitDegreeFlags & 0x08) polyDeg += "4";
	ui->polyFitDegreesLineEdit->setText(polyDeg);

	ui->framesForOverlapGuessSpinBox->setValue(this->scan.guessRegNumFrames);
	ui->initSearchRadiusSpinBox->setValue(this->scan.overlapSearchRadius);
	ui->validationRadiusSpinBox->setValue(this->scan.overlapValidationRadius);
	ui->maximumOverlapSpinBox->setValue(this->scan.overlapMaxPercentage);
#endif // USE_SERIAL

	ui->FramePitchendSlider->setValue(this->scan.overlapThreshold);

#ifdef USE_SERIAL
	// Hide the serial controls
	this->adminWidth = this->geometry().width();
	ui->groupBox_2->setVisible(false);
	this->setFixedWidth(ui->tabWidget->geometry().width()+35);
	ui->scrollAreaWidgetContents->setFixedWidth(ui->tabWidget->geometry().width()+20);
#endif // USE_SERIAL

	ui->maxFrequencyFrame->setVisible(false);

	ui->tabWidget->setCurrentIndex(0);
	frame_window = NULL;
	paramCopyLock = false;
	samplesPlayed.resize(4);

	QTimer::singleShot(0, this, SLOT(LicenseAgreement()));

	Log() << QDateTime::currentDateTime().toString() << "\n";
}

MainWindow::~MainWindow()
{
	DeleteTempSoundFile();
	delete ui;
}

void MainWindow::LicenseAgreement()
{
	QMessageBox msg(this);
	QString lic;
	QFile licFile(":/LICENSE.txt");

	msg.setText("License Agreement");
	msg.setInformativeText(
			"-------------------------------------------------------------------------------------------------\n"
			"Copyright (c) 2015,2016, South Carolina Research Foundation\n"
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
	// prevent a loop-back from GPU_Params_Update
	if(paramCopyLock) return;
	paramCopyLock = true;

	// If the OpenGL window isn't ready with a scan, skip the update
	//if (frame_window!=NULL && this->scan.inFile.IsReady())
	{
		ui->OverlapSoundtrackCheckBox->setChecked(params.useBounds);
		ui->OverlapPixCheckBox->setChecked(params.usePixBounds);

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

	}

	// release the lock
	paramCopyLock = false;
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

	unsigned char * tex_ptr=NULL;

	int dpx_h;
	int dpx_w;
	bool dpx_e;
	GLenum pix_fmt;
   int num_components;
    traceCurrentOperation = "Retrieving scan image";
	tex_ptr = this->scan.inFile.GetFrameImage(
			this->scan.inFile.FirstFrame()+frame_num,
            tex_ptr,dpx_w,dpx_h, pix_fmt,num_components, dpx_e);
	traceCurrentOperation = "Loading scan into texture";
    frame_window->load_frame_texture(tex_ptr,dpx_w,dpx_h,pix_fmt,num_components,dpx_e);

	traceCurrentOperation = "Freeing texture buffer";
	if (tex_ptr)
		delete [] tex_ptr;

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

	QString selectedType = "";
	QString filename = QFileDialog::getOpenFileName(this,
			tr("Film Source"), ".",
			fileTypeFilters.join(";;"),
			&selectedType /* , QFileDialog::DontUseNativeDialog */ );

	if(filename.isEmpty()) return;

	ft = fileTypeFilters.indexOf(selectedType);
	if(ft < 0) return;

	QFileInfo finfo = filename;

	//qDebug() << finfo.baseName();

#ifdef USE_SERIAL
	// If the filename is mirc_admin.dpx, expand the GUI to show the
	// Serial C++ routine controls, and do not change the current source,
	// if any.
	if(finfo.fileName().compare("mirc_admin.dpx",Qt::CaseInsensitive)==0)
	{
		ui->groupBox_2->show();
		this->setFixedWidth(this->adminWidth);
		ui->scrollAreaWidgetContents->setFixedWidth(ui->tabWidget->geometry().width()+20);
		qApp->processEvents();
		return;
	}
#endif // USE_SERIAL

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
		this->scan.SourceScan(filename.toStdString(),
				fileFilterArr[ft].fileType);
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
		return;
	}

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
#ifdef USE_SERIAL
		scan.guessRegNumFrames = ui->framesForOverlapGuessSpinBox->value();
		scan.overlapSearchRadius = ui->initSearchRadiusSpinBox->value();
		scan.overlapValidationRadius = ui->validationRadiusSpinBox->value();
		scan.overlapMaxPercentage = ui->maximumOverlapSpinBox->value();
		scan.overlapThreshold = ui->FramePitchendSlider->value();
		scan.movingAverageNumSweeps = ui->mAvgSweepsSpinBox->value();
		scan.movingAverageHalfSpan = ui->mAvgSpanSpinBox->value();

		QString polyDeg = ui->polyFitDegreesLineEdit->text();
		scan.polynomialFitDegreeFlags = 0;
		if(polyDeg.contains('1')) scan.polynomialFitDegreeFlags |= 0x01;
		if(polyDeg.contains('2')) scan.polynomialFitDegreeFlags |= 0x02;
		if(polyDeg.contains('3')) scan.polynomialFitDegreeFlags |= 0x04;
		if(polyDeg.contains('4')) scan.polynomialFitDegreeFlags |= 0x08;
#endif // USE_SERIAL

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

		traceCurrentOperation = "Updating GPU params";
		GPU_Params_Update(0);
		traceCurrentOperation = "Displaying first frame";
		Load_Frame_Texture(0);
	}

	traceCurrentOperation = "";
	traceSubroutineOperation = "";
	this->frame_window->logger = NULL;
	this->frame_window->currentOperation = NULL;

	// restore the previous SEGV handler
	if(prevSegvHandler != SIG_ERR)
		std::signal(SIGSEGV, prevSegvHandler);
}

//********************* Project Load and Save *********************************
void MainWindow::saveproject(QString fn)
{
	QFile file(fn);
	file.open(QIODevice::WriteOnly);
	QTextStream out(&file);   // we will serialize the data into the file

	saveproject(out);

	file.close();
	qDebug() << file.fileName();
}

void MainWindow::saveproject(QTextStream &out)
{
	out<<"AEO-Light Project Settings\n";
	out<<"Frame Rate = "<<ui->filmrate_PD->currentText()<<"\n";
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
	out<<"Lift = "<<ui->liftSlider->value()<<"\n";
	out<<"Gamma = "<<ui->gammaSlider->value()<<"\n";
	out<<"Gain = "<<ui->gainSlider->value()<<"\n";
	out<<"S-Curve Value = "<<ui->thresholdSlider->value()<<"\n";
	out<<"S-Curve On = "<<ui->threshBox->checkState()<<"\n";
	out<<"Blur = "<<ui->blurSlider->value()<<"\n";
	out<<"Negative = "<<ui->negBox->checkState()<<"\n";
	out<<"Desaturate = "<<ui->desatBox->checkState()<<"\n";
}

void MainWindow::loadproject(QString fn)
{

	QFile file(fn);
	file.open(QIODevice::ReadOnly);
	QTextStream in(&file);   // we will serialize the data into the file

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
	}

	file.close();
}


void MainWindow::on_saveprojectButton_clicked()
{
	QString file1Name = QFileDialog::getSaveFileName(this,
			tr("Save AEO Light Settings"), QDir::homePath(),
			tr("AEO Settings Files (*.aeo)"));

	saveproject(file1Name);
}

void MainWindow::on_loadprojectButton_clicked()
{
	QString file1Name = QFileDialog::getOpenFileName(this,
			tr("Open AEO Light Settings"), QDir::homePath(),
			tr("AEO Settings Files (*.aeo)"));

	loadproject(file1Name);
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

void MainWindow::extractGL(bool doTimer)
{
	// Ask for output filename
	QString filename = QFileDialog::getSaveFileName(
				this,tr("Export audio to"),".","*.wav");
	if(filename.isEmpty()) return;

	long firstFrame =
			ui->frameInSpinBox->value() - this->scan.inFile.FirstFrame();
	long numFrames =
			ui->frameOutSpinBox->value() - ui->frameInSpinBox->value() + 1;

	QElapsedTimer timer;
	if(doTimer) timer.start();

	bool success = Extract(filename, firstFrame, numFrames, EXTRACT_LOG);

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

	this->LogClose();

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

	sample = Extract(qtmp.fileName(), firstFrame, numFrames, EXTRACT_LOG);

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

ExtractedSound MainWindow::Extract(QString filename, long firstFrame,
		long numFrames, uint8_t flags)
{
	ExtractedSound ret;
	bool success;

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


extern "C" void SegvHandler(int param)
{
	longjmp(segvJumpEnv, 1);
}

bool MainWindow::WriteAudioToFile(const char *fn, long firstFrame,
		long numFrames)
{
	bool ret;

	ret = true;

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
	wav wout(samplerate);
	wout.nChannels = 2;

	wout.samplesPerFrame = frameratesamples;
	wout.bitsPerSample = frame_window->bit_depth;
	frame_window->samplesperframe_file =frameratesamples;
	frame_window->PrepareRecording(numFrames * frameratesamples);

	try
	{
		traceCurrentOperation = "Opening Output File";
		if(wout.open(fn) == NULL) throw 1;

		traceCurrentOperation = "Load Base Texture";
		if(!Load_Frame_Texture(firstFrame + 0)) throw 1; 
		frame_window->is_rendering=true;

		unsigned int sec;
		unsigned int frames;
		QStringList TCL = this->scan.inFile.TimeCode.split(QRegExp("[:]"),QString::SkipEmptyParts);
		sec =(((TCL[0].toInt() * 3600 )+ (TCL[1].toInt()* 60)+TCL[2].toInt()));
        frames = TCL[3].toInt() +firstFrame +( sec * fps_timbase);
        frames += ui->advance_CB->currentIndex();
        sec=frames/fps_timbase;
		frames= frames%fps_timbase;
		wout.set_timecode(sec,frames);

		traceCurrentOperation = "Load Texture";
		if(!Load_Frame_Texture(firstFrame + 1)) throw 1;

		for (long a = 2; a<= numFrames; a++)
		{
			traceCurrentOperation = "Load Texture";\
			if (a + firstFrame > this->scan.inFile.NumFrames()-1 )
			{
				if(!Load_Frame_Texture(firstFrame + a - 1)) break;
			}
			else
			{
				if(!Load_Frame_Texture(firstFrame + a)) break;
			}
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

		frame_window->is_rendering =false;
		traceCurrentOperation = "Process Recording";
		frame_window->ProcessRecording(numFrames * frameratesamples);
		traceCurrentOperation = "Writing wav file";
		wout.writebuffer(frame_window->FileRealBuffer,
				numFrames * frameratesamples);
		traceCurrentOperation = "Closing wav file";
		wout.close();
		traceCurrentOperation = "";

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
	ui->thresholdSlider->setValue(50);
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
	QMessageBox::information(NULL,"AEO-Light Acknowledgements",
			"AEO-Light is an open-source software that extracts audio "
			"from optical sound tracks of motion picture film. AEO-Light is "
			"produced at the University of South Carolina by a team comprised "
			"of faculty and staff from the University Libraries' Moving "
			"Image Research Collections (MIRC) and the College of Arts and "
			"Sciences Interdisciplinary Mathematics Institute (IMI)."
			"Project funding comes from the Preservation and Access Division"
			"of the National Endowment for the Humanities. AEO-Light is "
			"available through an open-source licensing agreement. The "
			"complete terms are available in the AEO-Light Documentation."
			"\n\n"
			"This software uses libraries from the FFmpeg project under "
			"the GPLv2.0"
			);
}

void MainWindow::on_actionAbout_triggered()
{
	QDesktopServices::openUrl(QUrl("http://imi.cas.sc.edu/mirc/"));
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
	on_saveprojectButton_clicked();
}

void MainWindow::on_actionSave_Settings_triggered()
{
	on_loadprojectButton_clicked();
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

