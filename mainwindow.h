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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QMainWindow>
#include <QSoundEffect>
#include <QDate>
#include <QProgressDialog>
#include <vector>
#include <queue>
#include "frame_view_gl.h"
#include "readframedpx.h"
#include "project.h"
#include "metadata.h"
#include "videoencoder.h"

#define USE_MUX_HACK
// #define SAVE_CALIBRATION_MASK_IN_PROJECT

namespace Ui {
class MainWindow;
}

#define FPS_NTSC 0
#define FPS_24 1
#define FPS_25 2
#define FPS_FILM FPS_24
#define FPS_PAL FPS_25


#define EXTRACT_TIMER 0x01
#define EXTRACT_LOG 0x02
#define EXTRACT_NOTIFY 0x04



class ExtractedSound
{
public:
	uint32_t frameIn;
	uint32_t frameOut;
	uint16_t bounds[2]; // left, right pixel columns
	uint16_t pixBounds[2]; // left, right pixel columns
	uint16_t framePitch[2]; // top, bottom pixel rows
	uint16_t gamma; // [0,500]
	uint16_t gain; // [0,500]
	uint16_t sCurve; // [0,600]
	uint8_t overlap; // as a percentage of height [0,100]
	int8_t lift; // [-100,100]
	int8_t blur; // [-100,100]
	uint8_t fpsType;
	bool useBounds;
	bool usePixBounds;
	bool useSCurve;
	bool makeNegative;
	bool makeGray;

    QSoundEffect *sound;
	int err;

	ExtractedSound();
	~ExtractedSound();
	void Play() const;
	bool operator ==(const ExtractedSound &ref) const;
	operator bool() const { return (err==0); };
};

class ExtractTask
{
public:
	ExtractedSound params;
	QString source;
	SourceFormat srcFormat;
	QString output;
	MetaData meta;
};

#ifdef USE_MUX_HACK
//----------------------------------------------------------------------------
//---------------------- MUX.C -----------------------------------------------
//----------------------------------------------------------------------------

// a wrapper around a single output AVStream
class OutputStream {
public:
	int requestedWidth;
	int requestedHeight;
	int requestedSamplingRate;
	int requestedSamplesPerFrame;
	int requestedTimeBase;

	AVStream *st;

	/* pts of the next frame that will be generated */
	int64_t next_pts;
	int samples_count;

	AVFrame *frame;
	AVFrame *tmp_frame;

	float t, tincr, tincr2;

	struct SwsContext *sws_ctx;
	struct SwrContext *swr_ctx;
};

#endif

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget *parent = 0);
	~MainWindow();
	void Render_Frame();
	Project scan;
	std::vector< ExtractedSound > samplesPlayed;
	std::vector< ExtractTask > extractQueue;
	int adminWidth;

	void GUI_Params_Update();
	static void GUI_Params_Update_Static(void *userData)
	{ static_cast<MainWindow *>(userData)->GUI_Params_Update(); }

	ExtractedSound ExtractionParamsFromGUI();
	void ExtractionParametersToGUI(const ExtractedSound &params);
	void PlaySample(int index);

private slots:
	void LicenseAgreement();
	bool WriteAudioToFile(const char *fn, const char *videoFn,
			long firstFrame, long numFrames);
	void DeleteTempSoundFile(void);
	void on_sourceButton_clicked();
	bool saveproject(QString);
	bool saveproject(QTextStream &stream);

	void SaveDefaultsSoundtrack();
	void SaveDefaultsImage();
	void SaveDefaultsAudio();
	void LoadDefaults();

	bool LoadProjectSource(QString fn);
	bool LoadProjectSettings(QString fn);
	bool OpenProject(QString fn);
	void OpenStartingProject();
	bool NewSource(QString fn, SourceFormat ft=SOURCE_UNKNOWN);
	bool Load_Frame_Texture(int);
	void GPU_Params_Update(bool renderyes);
	void UpdateQueueWidgets(void);
	QString Compute_Timecode_String(int position);
	uint64_t ComputeTimeReference(int position, int samplingRate);

	void on_leftSpinBox_valueChanged(int arg1);
	void on_rightSpinBox_valueChanged(int arg1);
	void on_MainWindow_destroyed();
	void on_leftSlider_sliderMoved(int position);
	void on_rightSlider_sliderMoved(int position);
	void on_playSlider_sliderMoved(int position);
	void on_markinButton_clicked();
	void on_markoutButton_clicked();
	void on_gammaSlider_valueChanged(int value);
	void on_liftSlider_valueChanged(int value);
	void on_gainSlider_valueChanged(int value);
	void on_lift_resetButton_clicked();
	void on_gamma_resetButton_clicked();
	void on_gain_resetButton_clicked();
	void on_thresh_resetButton_clicked();
	void on_blur_resetButton_clicked();
	void on_thresholdSlider_valueChanged(int value);
	void on_blurSlider_valueChanged(int value);
	void on_negBox_clicked();
	void on_threshBox_clicked();
	void on_desatBox_clicked();
	void on_overlapSlider_valueChanged(int value);
	void on_framepitchstartSlider_valueChanged(int value);
	void on_frame_numberSpinBox_valueChanged(int arg1);

	void on_extractGLButton_clicked();
	bool extractGL(QString filename, bool doTimer);
	bool extractGL(bool doTimer) { return extractGL(QString(), doTimer); }
	ExtractedSound Extract(QString filename, QString videoFilename,
			long firstFrame, long numFrames, uint8_t flags=0);

#ifdef USE_SERIAL
	void on_extractSerialButton_clicked();
	void extractSerial(bool doTimer);
#endif // USE_SERIAL

	void on_HeightCalculateBtn_clicked();
	void on_FramePitchendSlider_valueChanged(int value);
	void on_CalBtn_clicked();
	void on_CalEnableCB_clicked();
	void on_actionAcknowledgements_triggered();
	void on_actionAbout_triggered();
	void on_saveprojectButton_clicked();
	void on_loadprojectButton_clicked();
	void on_FramePitchendSlider_sliderMoved(int position);
	void on_leftPixSlider_valueChanged(int value);
	void on_rightPixSlider_valueChanged(int value);
	void on_leftPixSlider_sliderMoved(int position);
	void on_rightPixSlider_sliderMoved(int position);
	void on_leftPixSpinBox_valueChanged(int arg1);
	void on_rightPixSpinBox_valueChanged(int arg1);
	void on_monostereo_PD_currentIndexChanged(int index);
	void on_OverlapSoundtrackCheckBox_stateChanged(int arg1);
	void on_OverlapPixCheckBox_stateChanged(int arg1);
	void on_actionShow_Soundtrack_Only_triggered();
	void on_actionWaveform_Zoom_triggered();
	void on_actionShow_Overlap_triggered();
	void on_frameInSpinBox_valueChanged(int arg1);
    void on_waveformZoomCheckBox_clicked(bool checked);
	void on_showOverlapCheckBox_clicked(bool checked);
	void on_showSoundtrackOnlyCheckBox_clicked(bool checked);
	void on_actionOpen_Source_triggered();
	void on_actionLoad_Settings_triggered();
	void on_actionSave_Settings_triggered();
	void on_actionQuit_triggered();
	void on_frame_numberSpinBox_editingFinished();
	void on_frameOutSpinBox_valueChanged(int arg1);
	void on_playSampleButton_clicked();
	void on_playSample1Button_clicked();
	void on_playSample2Button_clicked();
	void on_playSample3Button_clicked();
	void on_playSample4Button_clicked();
	void on_loadSample1Button_clicked();
	void on_loadSample2Button_clicked();
	void on_loadSample3Button_clicked();
	void on_loadSample4Button_clicked();

	void on_enqueueButton_clicked();

	void on_queueDelete1Button_clicked();

	void on_queueDelete2Button_clicked();

	void on_queueDelete3Button_clicked();

	void on_queueDelete4Button_clicked();

	void on_queueDelete5Button_clicked();

	void on_loadSettingsButton_clicked();

	void on_queueExtractButton_clicked();

	void on_soundtrackDefaultsButton_clicked();

	void on_imageDefaultsButton_clicked();

	void on_extractDefaultsButton_clicked();

	void on_actionPreferences_triggered();

	void on_actionReport_or_track_an_issue_triggered();

	void on_OverlapPixCheckBox_clicked(bool checked);

private:
	QString startingProjectFilename;
	Ui::MainWindow *ui;
	Frame_Window * frame_window = NULL;
	QTextStream log;
	bool paramCopyLock;
	bool requestCancel;
	QString prevProjectDir;
	QString prevExportDir;
	MetaData *currentMeta;
	FrameTexture *currentFrameTexture;
	FrameTexture *outputFrameTexture;
	bool isVideoMuxingRisky;

public:
	void SetStartingProject(QString fn) { startingProjectFilename = fn; };
	int MaxFrequency() const;
	ExtractedSound ExtractionParameters();
	QString Version();
	QTextStream &Log();
	void LogSettings();
	void LogClose()
		{ if(log.device() && log.device()->isOpen()) log.device()->close(); }

#ifdef USE_MUX_HACK
private:

	long encStartFrame;
	long encNumFrames;
	long encCurFrame;
	long encVideoSkip; // discard scanned video frames from start
	long encAudioSkip; // discard extracted audio frame snippets from start
	long encVideoPad;  // add synthetic (black) video frames at start
	long encAudioPad;  // add synthetic (silent) audio frame snippets at start

	long encVideoBufSize;

	AVFrame *encRGBFrame;
	AVFrame *encS16Frame;

	std::queue< uint8_t* > encVideoQueue;
	int64_t encAudioLen; // total number of samples rendered so far
	int64_t encAudioNextPts;

	bool EnqueueNextFrame();
	uint8_t *GetVideoFromQueue();
	AVFrame *GetAudioFromQueue();

	AVFrame *get_audio_frame(OutputStream *ost);
	int write_audio_frame(AVFormatContext *oc, OutputStream *ost);

	AVFrame *get_video_frame(OutputStream *ost);
	int write_video_frame(AVFormatContext *oc, OutputStream *ost);

	int MuxMain(const char *fn_arg, long firstFrame, long numFrames,
			long vidFrameOffset, QProgressDialog &progress);

#endif

};

#endif // MAINWINDOW_H
