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

#include "frame_view_gl.h"

#include <iostream>
#include <fstream>
#include <string>
#include <QDebug>
#include <QString>
#include <QResource>
#include <QMessageBox>
#include <QWidget>
#include <QMouseEvent>
#include <QCursor>
#include <QContextMenuEvent>
//#include <qopengl.h>
#include <math.h>
#include <stdlib.h>

#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunsequenced"
#endif
#include "DspFilters/Dsp.h"
#if defined(__clang__)
# pragma clang diagnostic pop
#endif

#include "aeoexception.h"

#define PI 3.14159265358979323846

#define FRAME_Y_RATIO (0.75)

const char *gluErrorString(GLenum glerror)
{
	switch(glerror)
	{
	case GL_NO_ERROR: return "No error";
	case GL_INVALID_ENUM: return "Invalid Enumeration";
	case GL_INVALID_VALUE: return "Invalid Value";
	case GL_INVALID_OPERATION: return "Invalid Operation";
	case GL_STACK_OVERFLOW: return "Stack Overflow";
	case GL_STACK_UNDERFLOW: return "Stack Underflow";
	case GL_OUT_OF_MEMORY: return "Out of Memory";
	case GL_INVALID_FRAMEBUFFER_OPERATION:
		return "Invalid Framebuffer Operation";
	default: return "Unknown GL Error";
	}
}

void Frame_Window::CheckGLError(const char *fn, int line)
{
	QMessageBox w;
	GLenum glerror;
	QString msg("");
	int answer;
	static bool show = true;

#ifndef Q_OS_WIN32
	while((glerror = glGetError()) != GL_NO_ERROR)
	{
		msg += QString("GL Error in %1 at line %2:\n%3\n").arg(
					QString(fn), QString::number(line),
					QString(gluErrorString(glerror)));
	}

	if(msg.length() > 0)
	{
		if(this->logger)
		{
			(*(this->logger)) << msg << "\n";
		}

		#ifdef QT_DEBUG
		if(show)
		{
			msg += "\nContinue?\n";
			w.setText(msg);
			w.setStandardButtons(QMessageBox::Yes | QMessageBox::YesToAll |
					QMessageBox::No);
			w.setDefaultButton(QMessageBox::Yes);
			answer = w.exec();

			if(answer == QMessageBox::No) exit(1);
			if(answer == QMessageBox::YesToAll) show = false;
		}
		#endif
	}
#endif
}

#ifdef Q_DEBUG
#define CHECK_GL_ERROR(f,l) CheckGLError(f,l)
#else
#define CHECK_GL_ERROR(f,l) {}
#endif

const char *gluFBOString(GLenum fbos)
{
	switch(fbos)
	{
	case(GL_FRAMEBUFFER_COMPLETE): return "frambuffer complete.";
	case(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT):
		return "framebuffer attachment points are framebuffer incomplete.";
	case(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT):
		return "framebuffer does not have at least one image attached to it.";
	case(GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER):
		return "incomplete draw buffer.";
	case(GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER):
		return "incomplete read buffer.";
	case(GL_FRAMEBUFFER_UNSUPPORTED):
		return "framebuffer unsupported.";
	case(GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE):
		return "framebuffer incomplete multisample.";
	case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
		return "framebuffer incomplete layer targets.";
	default: return "Unknown GL framebuffer Error";
	}
}

void CheckFrameBuffer(GLenum fbo, const char *fn, int line)
{
	QMessageBox w;
	GLenum glerror;
	QString msg("");
	int answer;
	static bool show = true;

	GLenum fboErr;

#ifdef __APPLE__
	if((fboErr = glCheckFramebufferStatus(fbo)) != GL_FRAMEBUFFER_COMPLETE)
	{
		msg += QString("GL FBO Error in %1 at line %2:\n%3\n").arg(
				QString(fn), QString::number(line),
				QString(gluFBOString(fboErr)));
	}

	if(msg.length() > 0)
	{
		/*
		if(this->logger)
		{
			(*(this->logger)) << msg << "\n";
		}
		*/

		if(show)
		{
			msg += "\nContinue?\n";
			w.setText(msg);
			w.setStandardButtons(QMessageBox::Yes | QMessageBox::YesToAll |
								 QMessageBox::No);
			w.setDefaultButton(QMessageBox::Yes);
			answer = w.exec();

			if(answer == QMessageBox::No) exit(1);
			if(answer == QMessageBox::YesToAll) show = false;
		}
	}
#endif
}

#ifdef QT_DEBUG
	#define CHECK_GL_FBO(fbo,f,l) CheckFrameBuffer(fbo,f,l)
#else
	#define CHECK_GL_FBO(fbo,f,l) {}
#endif

#define CUR_OP(s) { if(currentOperation) (*currentOperation) = s; }

Frame_Window::Frame_Window(int w,int h)
	: m_program(0)
	, m_frame(0)
	, OGL_Fun()
{
	// working data
	FileRealBuffer = NULL;

	// view parameters
	WFMzoom=1.0f;

	calval=0;

	o_color=0;
	currstart=0;
	rendermode=0;

	lift=0;
	gamma=1.0f;
	gain=1.0f;
	threshold = 0.5;
	blur = 0;
	stereo = 0;
	thresh=false;

	input_w=w;
	input_h=h;

	trackonly = false;

	bestloc = 0;
	lowloc = 0;

	negative=false;

	overlap_target = 2; // use both sound and picture

	desaturate = false;

	is_preload = false;

	is_calc = false;
	is_calculating=false;

	samplesperframe=2000;
	samplesperframe_file =2000;

	bestmatch.postion = 0;
	bestmatch.value = 0;
	currmatch.postion = 0;
	currmatch.value = 0;
	match_array = new overlap_match[5];

	cal_enabled=false;
	cal_points=2000;
	is_caling=false;

	sound_prev.resize(2000);
	sound_curr.resize(2000);

	channels =2;
	audio_sample_buffer= new float [2*4095*channels];
	audio_compare_buffer= new float [2*samplesperframe*8];

	bounds[0] = bounds[1] = bounds[2] = bounds[3] = 0;
	overlap[0] = overlap[1] = overlap[2] = overlap[3] = 0;

	height_avg = new float[50];
	h_avg = 0;
	match_avf= new float[5];

	pixbounds[0] = pixbounds[1] = 0;

    rot_angle = 0.0;

	match_inc=0;
	height_inc= 0;

	overlapshow=false;
	is_rendering =false;
	is_debug = false;
	overrideOverlap = 0;

	fps = 24.0;
	duration = 0; // milliseconds
	bit_depth = 16;
	sampling_rate = 48000;

	clear_cal = false;

	logger = NULL;
	currentOperation = NULL;

	// private:
	paramUpdateCB = NULL;
	paramUpdateUserData = NULL;

	samplepointer = 0;

	new_frame = false;

	audio_draw_buffers = NULL;
	audio_pbo = 0;

	m_posAttr = 0;
	m_texAttr = 0;
	m_matrixUniform = 0;
	m_inputsize_loc = 0;
	m_rendermode_loc = 0;
	m_manipcontrol_loc = 0;
	m_show_loc = 0;
	m_overlap_target_loc = 0;
	m_neg_loc = 0;
	m_overlap_loc = 0;
	stereo_loc = 0;
	pix_bounds_loc = 0;
	dminmax_loc = 0;
	m_colorcontrol_loc = 0;
	m_bounds_loc = 0;
    m_rot_angle = 0;
	m_calcontrol_loc = 0;
	m_overlapshow_loc = 0;
	frame_texture = 0;
	adj_frame_fbo = 0;
	adj_frame_texture = 0;
	prev_adj_frame_tex = 0;
	audio_fbo = 0;
	audio_file_fbo = 0;
	audio_RGB_texture = 0;
	prev_audio_RGB_texture = 0;
	overlap_compare_audio_texture = 0;
	overlaps_audio_texture = 0;
	output_audio_texture = 0;
	audio_float_texture = 0;
	audio_int_texture = 0;
	cal_audio_texture = 0;
	vo.videobuffer=NULL;
	is_videooutput=0;


}

Frame_Window::~Frame_Window()
{
	CHECK_GL_ERROR(__FILE__,__LINE__);
	CUR_OP("Deleting frame_texture");
	glDeleteTextures(1,&frame_texture);

	CUR_OP("Deleting adj_frame_fbo");
	//glIsFramebuffer returns true, but glDeleteFrameBuffers crashes.
	// Commenting this out to avoid the crash. It seems to not consume the
	// buffer -- the same value (1) is re-used for adj_frame_fbo on my
	// system (OSX), so itmust be getting deleted somewhere else.
	//if(glIsFramebuffer(adj_frame_fbo))
	//		glDeleteFramebuffers( 1, &adj_frame_fbo);

	CUR_OP("Deleting prev_adj_frame_tex");
	glDeleteTextures(1,&prev_adj_frame_tex);

	CHECK_GL_ERROR(__FILE__,__LINE__);
	CUR_OP("Deleting audio_fbo");
	//glDeleteFramebuffers( 1, &audio_fbo);
	CUR_OP("Deleting audio_file_fbo");
	//glDeleteFramebuffers(1,&audio_file_fbo);

	CHECK_GL_ERROR(__FILE__,__LINE__);
	CUR_OP("Deleting audio_RGB_texture");
	glDeleteTextures(1,&audio_RGB_texture);
	//glDeleteTextures(1,&audio_float_texture);
	CUR_OP("Deleting prev_audio_RGB_texture");
	glDeleteTextures(1,&prev_audio_RGB_texture);
	//glDeleteTextures(1,&audio_int_texture);
	CUR_OP("Deleting overlaps_audio_texture");
	glDeleteTextures(1,&overlaps_audio_texture);
	CUR_OP("Deleting overlap_compare_audio_texture");
	glDeleteTextures(1,&overlap_compare_audio_texture);

	CUR_OP("Deleting cal_audio_texture");
	glDeleteTextures(1,&cal_audio_texture);
	CUR_OP("Deleting output_audio_texture");
	glDeleteTextures(1,&output_audio_texture);

	CHECK_GL_ERROR(__FILE__,__LINE__);

	if(audio_sample_buffer)
	{
		CUR_OP("Deleting audio_sample_buffer");
		delete [] audio_sample_buffer;
	}

	if(audio_compare_buffer)
	{
		CUR_OP("Deleting audio_compare_buffer");
		delete [] audio_compare_buffer;
	}

	if(height_avg)
	{
		CUR_OP("Deleting height_avg");
		delete [] height_avg;
	}

	if(match_avf)
	{
		CUR_OP("Deleting match_avf");
		delete [] match_avf;
	}

	if(match_array)
	{
		CUR_OP("Deleting match_array");
		delete [] match_array;
	}

	if(audio_draw_buffers)
	{
		CUR_OP("audio_draw_buffers");
		delete [] audio_draw_buffers;
	}

	if(m_program)
	{
		CUR_OP("Deleting m_program");
		delete m_program;
	}
	CUR_OP("");
}

GLuint Frame_Window::loadShader(GLenum type, const char *source)
{
	CHECK_GL_ERROR(__FILE__,__LINE__);
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, 0);
	glCompileShader(shader);
	CHECK_GL_ERROR(__FILE__,__LINE__);
	return shader;
}

void Frame_Window::initialize()
{
	initializeOpenGLFunctions();
	#ifndef __APPLE__
	{
		OGL_Fun.initializeOpenGLFunctions();
	}
	#endif
	m_program = new QOpenGLShaderProgram(this);
	m_program->addShaderFromSourceFile(QOpenGLShader::Vertex,
			":/Shaders/vert_shader.vert");
	m_program->addShaderFromSourceFile(QOpenGLShader::Fragment,
			":/Shaders/frag_shader.frag");


	m_program->link();
	m_overlapshow_loc = m_program->uniformLocation("overlapshow");
	pix_bounds_loc = m_program->uniformLocation("pix_boundry");
	m_colorcontrol_loc = m_program->uniformLocation("color_controls");
	m_manipcontrol_loc = m_program->uniformLocation("manip_controls");
	m_calcontrol_loc = m_program->uniformLocation("cal_controls");
	m_inputsize_loc = m_program->uniformLocation("inputsize");
	m_overlap_target_loc = m_program->uniformLocation("overlap_target");
	m_show_loc = m_program->uniformLocation("show_mode");
	m_posAttr = m_program->attributeLocation("posAttr");
	m_texAttr = m_program->attributeLocation("texCoord");
	m_matrixUniform = m_program->uniformLocation("matrix");
	m_bounds_loc = m_program->uniformLocation("bounds");

    m_rot_angle= m_program->uniformLocation("rot_angle");

    m_neg_loc = m_program->uniformLocation("negative");
	stereo_loc=  m_program->uniformLocation("isstereo");
	dminmax_loc = m_program->uniformLocation("dminmax");
	m_rendermode_loc = m_program->uniformLocation("render_mode");
	m_overlap_loc = m_program->uniformLocation("overlap");
	gen_tex_bufs(); //call for all textures and buffers to be created
	overlap[0]=0;
	overlap[1]=0;

	audio_draw_buffers = new GLenum[2];
	audio_draw_buffers[0] = GL_COLOR_ATTACHMENT0;
	audio_draw_buffers[1] = GL_COLOR_ATTACHMENT1;

	m_program->setUniformValue("frame_tex",0);
	m_program->setUniformValue("adj_frame_tex",1);
	m_program->setUniformValue("prev_frame_tex",2);
	m_program->setUniformValue("audio_tex",3);
	m_program->setUniformValue("prev_audio_tex",4);
}


void Frame_Window::gen_tex_bufs()
{
	//textures *********************************

	CHECK_GL_ERROR(__FILE__,__LINE__);
	glGenTextures(1,&frame_texture);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, frame_texture);
	CHECK_GL_ERROR(__FILE__,__LINE__);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	CHECK_GL_ERROR(__FILE__,__LINE__);

	glGenTextures(1,&adj_frame_texture);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, adj_frame_texture);
	CHECK_GL_ERROR(__FILE__,__LINE__);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	CHECK_GL_ERROR(__FILE__,__LINE__);

	glGenTextures(1,&prev_adj_frame_tex);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, prev_adj_frame_tex);
	CHECK_GL_ERROR(__FILE__,__LINE__);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	CHECK_GL_ERROR(__FILE__,__LINE__);

	glGenTextures(1,&audio_RGB_texture);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, audio_RGB_texture);
	CHECK_GL_ERROR(__FILE__,__LINE__);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	CHECK_GL_ERROR(__FILE__,__LINE__);

	glGenTextures(1,&prev_audio_RGB_texture);
	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D, prev_audio_RGB_texture);
	CHECK_GL_ERROR(__FILE__,__LINE__);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	CHECK_GL_ERROR(__FILE__,__LINE__);

	glGenTextures(1,&overlap_compare_audio_texture);
	glActiveTexture(GL_TEXTURE5);
	glBindTexture(GL_TEXTURE_2D, overlap_compare_audio_texture);
	CHECK_GL_ERROR(__FILE__,__LINE__);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	CHECK_GL_ERROR(__FILE__,__LINE__);

	glGenTextures(1,&overlaps_audio_texture);
	glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_2D, overlaps_audio_texture);
	CHECK_GL_ERROR(__FILE__,__LINE__);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	CHECK_GL_ERROR(__FILE__,__LINE__);

	glGenTextures(1,&cal_audio_texture);
	glActiveTexture(GL_TEXTURE7);
	glBindTexture(GL_TEXTURE_2D, cal_audio_texture);
	CHECK_GL_ERROR(__FILE__,__LINE__);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	CHECK_GL_ERROR(__FILE__,__LINE__);

	glGenTextures(1,&output_audio_texture);
	glActiveTexture(GL_TEXTURE8);
	glBindTexture(GL_TEXTURE_2D, output_audio_texture);
	CHECK_GL_ERROR(__FILE__,__LINE__);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	CHECK_GL_ERROR(__FILE__,__LINE__);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	CHECK_GL_ERROR(__FILE__,__LINE__);

	glActiveTexture(GL_TEXTURE0);

	GLint progactive;
	glGetIntegerv(GL_CURRENT_PROGRAM, &progactive);
	qDebug() << "Active program = " << progactive;
	GLint texLoc = m_program->uniformLocation("frame_tex");
	qDebug() << "frame_tex loc = " << texLoc;
	CHECK_GL_ERROR(__FILE__,__LINE__);
	glUniform1i(texLoc, 0);  // GL Error: invalid operation
	CHECK_GL_ERROR(__FILE__,__LINE__);
	texLoc = m_program->uniformLocation("adj_frame_tex");
	qDebug() << "adj_frame_tex loc = " << texLoc;
	glUniform1i(texLoc, 1); // GL Error: invalid operation
	CHECK_GL_ERROR(__FILE__,__LINE__);
	//glGetUniformLocation()
	texLoc = m_program->uniformLocation("prev_frame_tex");
	qDebug() << "prev_frame_tex loc = " << texLoc;
	glUniform1i(texLoc,2);
	texLoc = m_program->uniformLocation("audio_tex");
	glUniform1i(texLoc, 3);
	texLoc = m_program->uniformLocation("prev_audio_tex");
	glUniform1i(texLoc, 4);
	texLoc = m_program->uniformLocation("overlap_audio_tex");
	glUniform1i(texLoc, 5);
	texLoc = m_program->uniformLocation("overlapcompute_audio_tex");
	glUniform1i(texLoc, 6);

	texLoc =m_program->uniformLocation("cal_audio_tex");
	glUniform1i(texLoc, 7); // GL Error: invalid operation

	CHECK_GL_ERROR(__FILE__,__LINE__);

	//fbos *********************************

	glGenFramebuffers(1,&adj_frame_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER,adj_frame_fbo);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D,adj_frame_texture);
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGB16F,input_w,input_h,0,
			GL_RGBA,GL_UNSIGNED_INT,NULL);
	glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,
			adj_frame_texture,0);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D,prev_adj_frame_tex);
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGB16F,input_w,input_h,0,GL_RGBA,
			GL_UNSIGNED_INT,NULL);
	glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT1,GL_TEXTURE_2D,
			prev_adj_frame_tex,0);
	CHECK_GL_ERROR(__FILE__,__LINE__);

	glBindFramebuffer(GL_FRAMEBUFFER,0);

	glGenFramebuffers(1,&audio_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER,audio_fbo);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D,audio_RGB_texture);
	glTexImage2D(GL_TEXTURE_2D,0,GL_R32F,2,samplesperframe,0,GL_RGBA,
			GL_UNSIGNED_INT,NULL);
	glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,
			audio_RGB_texture,0);
	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D,prev_audio_RGB_texture);
	glTexImage2D(GL_TEXTURE_2D,0,GL_R32F,2,samplesperframe,0,GL_RGBA,
			GL_UNSIGNED_INT,NULL);
	glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT1,GL_TEXTURE_2D,
			prev_audio_RGB_texture,0);
	glActiveTexture(GL_TEXTURE5);
	glBindTexture(GL_TEXTURE_2D,overlap_compare_audio_texture);
	glTexImage2D(GL_TEXTURE_2D,0,GL_R32F,2,samplesperframe,0,GL_RGBA,
			GL_UNSIGNED_INT,NULL);
	glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT2,GL_TEXTURE_2D,
			overlap_compare_audio_texture,0);
	glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_2D,overlaps_audio_texture);
	glTexImage2D(GL_TEXTURE_2D,0,GL_R32F,2,samplesperframe,0,GL_RGBA,
			GL_UNSIGNED_INT,NULL);
	glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT3,GL_TEXTURE_2D,
			overlaps_audio_texture,0);
	glActiveTexture(GL_TEXTURE7);
	glBindTexture(GL_TEXTURE_2D, cal_audio_texture);
	glTexImage2D(GL_TEXTURE_2D,0,GL_R32F,2,cal_points,0,GL_RGBA,
			GL_UNSIGNED_INT,NULL);
	glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT4,GL_TEXTURE_2D,
			cal_audio_texture,0);

	glGenFramebuffers(1,&audio_file_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER,audio_file_fbo);
	glActiveTexture(GL_TEXTURE8);
	glBindTexture(GL_TEXTURE_2D, output_audio_texture);
	glTexImage2D(GL_TEXTURE_2D,0,GL_R32F,2,4095,0,GL_RGBA,GL_UNSIGNED_INT,NULL);
	glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,
			output_audio_texture,0);

	glActiveTexture(GL_TEXTURE0);

	CHECK_GL_ERROR(__FILE__,__LINE__);
	glDisable(GL_DITHER);
	glDisable(GL_DEPTH_TEST);
}

void Frame_Window::load_frame_texture(FrameTexture *frame)
{
	GLenum componentformat;
	CHECK_GL_ERROR(__FILE__,__LINE__);
	glActiveTexture(GL_TEXTURE0);
	glPixelStorei(GL_UNPACK_SWAP_BYTES, frame->isNonNativeEndianess) ;
	glBindTexture(GL_TEXTURE_2D,frame_texture);
	CHECK_GL_ERROR(__FILE__,__LINE__);

	switch(frame->nComponents)
	{
	case 4: componentformat = GL_RGBA; break;
	case 3:	componentformat = GL_RGB; break;
	case 1:	componentformat = GL_LUMINANCE; break;
	default: throw AeoException("Invalid num_components");
	}

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16, frame->width, frame->height, 0,
			componentformat, frame->format, frame->buf);

	CHECK_GL_ERROR(__FILE__,__LINE__);
	new_frame=true;
}

void Frame_Window::update_parameters()
{
	m_program->setUniformValue(m_colorcontrol_loc, lift,gamma,gain,
			float(!desaturate));
	m_program->setUniformValue(m_manipcontrol_loc, float(thresh),threshold,
			blur);
	m_program->setUniformValue(m_bounds_loc,
			bounds[0],bounds[1],bounds[2],bounds[3]);
    m_program->setUniformValue(m_rot_angle, rot_angle);
	m_program->setUniformValue(pix_bounds_loc,pixbounds[0],pixbounds[1]);
	m_program->setUniformValue(m_overlap_loc,
			overlap[0],overlap[1],overlap[2],overlap[3]);
	m_program->setUniformValue(m_show_loc, float(trackonly));
	m_program->setUniformValue(stereo_loc, stereo);
	m_program->setUniformValue(m_neg_loc, float(negative));
	m_program->setUniformValue(m_calcontrol_loc, float(cal_enabled),
			float(is_caling),0,0);
	m_program->setUniformValue(m_overlapshow_loc, float(overlapshow));
	m_program->setUniformValue(m_inputsize_loc, float(input_w), float(input_h));
	m_program->setUniformValue(m_overlap_target_loc, overlap_target);
}

void Frame_Window::CopyFrameBuffer(GLuint fbo, int width, int height)
{
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glDrawBuffer(GL_COLOR_ATTACHMENT1);
	glClear(GL_COLOR_BUFFER_BIT);
	glReadBuffer(GL_COLOR_ATTACHMENT0);

	{
		#ifndef __APPLE__
		OGL_Fun.
		#endif
		glBlitFramebuffer(
				0,0,width,height,
				0,0,width,height,
				GL_COLOR_BUFFER_BIT,GL_NEAREST);
	}

	glReadBuffer(0);
	glDrawBuffer(0);
}

void Frame_Window::render()
{

	CUR_OP("setUniformValues");
	m_program->bind();
	m_program->setUniformValue("frame_tex",0);
	m_program->setUniformValue("adj_frame_tex",1);
	m_program->setUniformValue("prev_frame_tex",2);
	m_program->setUniformValue("audio_tex",3);
	m_program->setUniformValue("prev_audio_tex",4);
	m_program->setUniformValue("overlap_audio_tex",5);
	m_program->setUniformValue("overlapcompute_audio_tex",6);
	m_program->setUniformValue("cal_audio_tex",7);

	const qreal retinaScale = devicePixelRatio();

	CUR_OP("set matrix to identity");

	QMatrix4x4 matrix;
	// matrix.perspective(60.0f, 4.0f/3.0f, 0.1f, 100.0f);
	// matrix.translate(0, 0, -2);
	matrix.setToIdentity();

	CUR_OP("set vertex arrays");

	GLfloat * fullarray ;
	//matrix.rotate(45, 0, 1, 0);
	m_program->setUniformValue(m_matrixUniform, matrix);
	update_parameters();
	GLfloat verticesPix[] ={
		-1, -1, 0, // bottom left corner
		-1,  1, 0, // top left corner
		 1,  1, 0, // top right corner
		 1, -1, 0  // bottom right corner
	};

	GLubyte indices[] = {
		0,1,2, // first triangle (bottom left - top left - top right)
		0,2,3  // second triangle (bottom left - top right - bottom right)
	};

	GLfloat verticesTex[] ={
		0, 0, 0, // bottom left corner
		0, 1, 0, // top left corner
		1, 1, 0, // top right corner
		1, 0, 0  // bottom right corner
	};

	GLfloat verticesTRO[] ={
		bounds[0], 0, 0, // bottom left corner
		bounds[0], 1, 0, // top left corner
		bounds[1], 1, 0, // top right corner
		bounds[1], 0, 0  // bottom right corner
	};

	int jitter;
	int floor = 0, ceiling = 10, range = (ceiling - floor);

	jitter =(ceiling/2)- (floor+int(rand()%range) );
	float jitteroffset = (1.0/input_h) * jitter;
	GLfloat verticesTexJitter[] ={
		0, 0+jitteroffset, 0, // bottom left corner
		0, 1+jitteroffset, 0, // top left corner
		1, 1+jitteroffset, 0, // top right corner
		1, 0+jitteroffset, 0  // bottom right corner
	};


	glVertexAttribPointer(m_posAttr, 3, GL_FLOAT, GL_FALSE, 0, verticesPix);
	CHECK_GL_ERROR(__FILE__,__LINE__);

	glVertexAttribPointer(m_texAttr, 3, GL_FLOAT, GL_FALSE, 0, verticesTex);
	CHECK_GL_ERROR(__FILE__,__LINE__);

	glEnableVertexAttribArray(0);
	CHECK_GL_ERROR(__FILE__,__LINE__);
	glEnableVertexAttribArray(1);
	CHECK_GL_ERROR(__FILE__,__LINE__);


	if(new_frame)
	{
		CUR_OP("binding adj_frame_fbo");
		CopyFrameBuffer(adj_frame_fbo, input_w, input_h);
		CHECK_GL_ERROR(__FILE__,__LINE__);

		CUR_OP("binding to audio_fbo");
		CopyFrameBuffer(audio_fbo, 2, samplesperframe);
		CHECK_GL_ERROR(__FILE__,__LINE__);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0); // GL Error: invalid operation
	CHECK_GL_ERROR(__FILE__,__LINE__);

	//************************Adjustment Render********************************
	// Input Textures: frame_tex (original from file)
	// Renders to: adj_frame_tex
	// Description: applies color and density correction to image


	bool jitteractive = false;

	CUR_OP("adjustment render (mode 0)");
	m_program->setUniformValue(m_rendermode_loc, 0.0f);

	CUR_OP("new frame vertext attrib pointed to verticesTex");

	if(!new_frame || !jitteractive)
		glVertexAttribPointer(m_texAttr, 3, GL_FLOAT, GL_FALSE, 0, verticesTex);
	else
		glVertexAttribPointer(m_texAttr, 3, GL_FLOAT, GL_FALSE, 0,
				verticesTexJitter);


	CHECK_GL_ERROR(__FILE__,__LINE__);
	CUR_OP("binding to adj_frame_fbo for new frame");
	glBindFramebuffer(GL_FRAMEBUFFER,adj_frame_fbo);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	glViewport(0,0, input_w, input_h);
	glClear(GL_COLOR_BUFFER_BIT);
	CUR_OP("drawTriangles for adj_frame_fbo new frame");
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, indices);
	CHECK_GL_ERROR(__FILE__,__LINE__);


	//********************************Audio RENDER*****************************
	// Input Textures: adj_frame_texture (adjusted image texture)
	// Renders to: audio_RGB_texture
	// Description: steps through each line within x boundary and computes
	//   value for display

	CUR_OP("audio render (mode 1)");
	m_program->setUniformValue(m_rendermode_loc, 1.0f);
	CUR_OP("setting vertexSttribPointer for audio render");
	glVertexAttribPointer(m_texAttr, 3, GL_FLOAT, GL_FALSE, 0, verticesTex);
	CHECK_GL_ERROR(__FILE__,__LINE__);
	CUR_OP("binding to audio_fbo in mode 1");
	glBindFramebuffer(GL_FRAMEBUFFER,audio_fbo);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	glViewport(0,0, 2, samplesperframe);

	glClear(GL_COLOR_BUFFER_BIT);

	CUR_OP("drawElements for audio_fbo in mode 1");
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, indices);


	CHECK_GL_ERROR(__FILE__,__LINE__);
	glBindFramebuffer(GL_FRAMEBUFFER,audio_fbo);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	CHECK_GL_ERROR(__FILE__,__LINE__);

	//copy float buffer out
	CUR_OP("copy float buffer out of audio_fbo in mode 1");
	glReadPixels(0,0,2,samplesperframe,GL_RED, GL_FLOAT,audio_compare_buffer);
	fullarray = (static_cast<GLfloat*>(audio_compare_buffer));

	CUR_OP("getting dmin and dmax from audio_fbo in mode 1");
	GLfloat* subdminarray = &fullarray[samplesperframe-samplesperframe/4];
	float dmin =0.0;// GetMin(subdminarray,samplesperframe/4);
	float dmax =1.0;// GetMax(subdminarray,samplesperframe/4);
	m_program->setUniformValue(dminmax_loc, dmin,dmax);

	//**********************************Cal RENDER*****************************
	// Input Textures: adj_frame_texture (adjusted image texture)
	// Renders to: cal_audio_texture
	// Description: averages lines with alpha 0.005 200 frames
	if(is_caling)
	{
		CUR_OP("Cal Render");

		m_program->setUniformValue(m_rendermode_loc, 1.0f);
		glVertexAttribPointer(m_texAttr, 3, GL_FLOAT, GL_FALSE, 0, verticesTRO);
		CHECK_GL_ERROR(__FILE__,__LINE__);
		glBindFramebuffer(GL_FRAMEBUFFER,audio_fbo);
		glDrawBuffer(GL_COLOR_ATTACHMENT4);
		glViewport(0,0, 2, cal_points);
		if(clear_cal)
		{
			glClear(GL_COLOR_BUFFER_BIT);
			clear_cal=false;
		}
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		//  glBlendFunc (GL_SRC_ALPHA, GL_SRC_ALPHA);
		glEnable( GL_BLEND );
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, indices);
		glDisable( GL_BLEND );
		glBindFramebuffer(GL_FRAMEBUFFER,audio_fbo);
		glReadBuffer(GL_COLOR_ATTACHMENT4);
		glReadPixels(0, 0, 1, cal_points,GL_RED, GL_FLOAT,audio_compare_buffer);
		CHECK_GL_ERROR(__FILE__,__LINE__);
		fullarray = (static_cast<GLfloat*>(audio_compare_buffer));
		calval=GetAverage(fullarray,cal_points);

		//glClear(GL_COLOR_BUFFER_BIT);

		glBindFramebuffer(GL_FRAMEBUFFER,audio_fbo);
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		CHECK_GL_ERROR(__FILE__,__LINE__);
	}
	//**************** RENDER Audio & Pix for Overlap for computations*********
	// x0 = curr *** x1 =prev
	// Input Textures: adj_frame_texture (adjusted image texture)
	//   and prev_adj_frame_texture
	// Renders to: overlap_compare_audio_texture
	// Description: computes 1d waveform for current and previous adjusted
	//   frames. pixel column 0 is current and column 1 is previous

	CUR_OP("Audio overlap render (mode 4)");
	m_program->setUniformValue(m_rendermode_loc, 4.0f);
	CUR_OP("Set VertextAttribPointer for Audio overlap render (mode 4)");
	glVertexAttribPointer(m_texAttr, 3, GL_FLOAT, GL_FALSE, 0, verticesTex);
	CHECK_GL_ERROR(__FILE__,__LINE__);
	CUR_OP("Binding audio_fbo for Audio overlap render (mode 4)");
	glBindFramebuffer(GL_FRAMEBUFFER,audio_fbo);
	glDrawBuffer(GL_COLOR_ATTACHMENT2);
	glViewport(0,0, 2, samplesperframe);
	glClear(GL_COLOR_BUFFER_BIT);
	CUR_OP("Drawing elements for Audio overlap render (mode 4)");
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, indices);
	CHECK_GL_ERROR(__FILE__,__LINE__);
	glBindFramebuffer(GL_FRAMEBUFFER,audio_fbo);

	//****************************overlap renders *****************************
	// Input Textures: overlap_compare_audio_texture
	// Renders to: overlaps_audio_texture
	// Description: slides curr and previous 1d arrays over each other and
	// takes the absolute value difference
	//  location is 2 * tex coord

	CUR_OP("Drawing overlaps (mode 5)");
	m_program->setUniformValue(m_rendermode_loc, 5.0f);
	CUR_OP("Set vertexAttribPointer for Drawing overlaps (mode 5)");
	glVertexAttribPointer(m_texAttr, 3, GL_FLOAT, GL_FALSE, 0, verticesTex);
	CHECK_GL_ERROR(__FILE__,__LINE__);
	CUR_OP("binding audio_fbo for Drawing overlaps (mode 5)");
	glBindFramebuffer(GL_FRAMEBUFFER,audio_fbo);

	glDrawBuffer(GL_COLOR_ATTACHMENT3);

	glViewport(0,0, 2, samplesperframe);

	glClear(GL_COLOR_BUFFER_BIT);

	//  glBindTexture(GL_TEXTURE_2D,adj_frame_texture);

	CUR_OP("drawing elements for Drawing overlaps (mode 5)");
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, indices);
	CHECK_GL_ERROR(__FILE__,__LINE__);

	glBindFramebuffer(GL_FRAMEBUFFER,audio_fbo);
	glReadBuffer(GL_COLOR_ATTACHMENT3);
	CUR_OP("reading pixels for audio_compare_buffer");
	glReadPixels(0,0,1,samplesperframe,GL_RED, GL_FLOAT,audio_compare_buffer);
	CHECK_GL_ERROR(__FILE__,__LINE__);

	//***********************Find best overlap match***************************
	float low=20.0;
	float temp;
	lowloc=0;
	// for (int i = 4; i<2*(samplesperframe * overlap[1]); i++)

	int start ;
	int end ;
	int pitchline;
	int lowest;
	fullarray = (static_cast<GLfloat*>(audio_compare_buffer));

	CUR_OP("setting search parameters for finding best overlap");
	// end = std::max(end,start);
	bool outsidefind = false;
	start = (overlap[2]+overlap[3]) * samplesperframe -
			(overlap[1]*0.5*samplesperframe) ;
	end = (overlap[2]+overlap[3]) * samplesperframe +
			(overlap[1]*0.5*samplesperframe) ;
	pitchline = (overlap[2]+overlap[3]) * samplesperframe;

	start = std::max(4,start);
	end = std::min(end,1998);
	end=std::max(end,start);
	pitchline = (end+start)/2;

	GLfloat* subarray = &fullarray[samplesperframe-end];

	CUR_OP("getting best match from subarray in finding best overlap");
	GetBestMatchFromFloatArray(subarray,(end-start),end, bestmatch);

	int s_start,s_end;
	int s_size = end-start;
	int s_mid = start + (s_size/2);
	int s_i_size;
	int best=0;

	for (int i = 1; i<6; i++)
	{
		s_i_size =  ((s_size/2)/5);
		if(i==1)
		{
			s_start = s_mid -(4);
			s_end   = s_mid +(4);

		}
		else
		{
			s_start = s_mid -(s_i_size*i);
			s_end   = s_mid +(s_i_size*i);
		}

		s_start= std::max(4,s_start);
		s_end = std::min(s_end,1998);

		subarray = &fullarray[samplesperframe-s_end];

		CUR_OP("getting best match 2 from subarray in finding best overlap");
		GetBestMatchFromFloatArray(subarray,(s_end-s_start),s_end,
				match_array[i-1]) ;
	}
	if(is_calc)
	{
		bestmatch= match_array[0];
	}

	else
		bestmatch= match_array[4];

	CUR_OP("recording best overlap");
	overlap[0] = (float)(bestmatch.postion)/2000.0;

	bool usegl=true;

	if(overrideOverlap > 0)
	{
		lowloc = overrideOverlap;
		usegl = false;
	}

	CHECK_GL_ERROR(__FILE__,__LINE__);

	CUR_OP("logging results of overlap computation");
	if(logger)
		(*logger) <<
				" OpenGL overlap "<< bestmatch.postion <<
				" Using " << (overrideOverlap?"Override ":"OpenGL ") <<
				lowloc <<
				" FrameStart " << overlap[3] <<
				" FrameStop " << 1.0+(overlap[3] - overlap[0]) <<
				" start search " << start <<
				" end search " << end <<
				"   " << outsidefind <<
				"\n";

	qDebug() << "jitter: " << jitter << "smid: " << s_mid <<
			" Opengl overlap " << bestmatch.postion << " Using OpenGL: " <<
			usegl << " Override: " << overrideOverlap << " FrameStart   " <<
			overlap[3] << " frameStop " << (1.0 - overlap[0] + overlap[3]) <<
			" start search" << start << " end search" << end << "   " <<
			outsidefind;

	qDebug()<<"MA[0] "<< match_array[0].postion<<" , "<<match_array[0].value;
	qDebug()<<"MA[1] "<< match_array[1].postion<<" , "<<match_array[1].value;
	qDebug()<<"MA[2] "<< match_array[2].postion<<" , "<<match_array[2].value;
	qDebug()<<"MA[3] "<< match_array[3].postion<<" , "<<match_array[3].value;
	qDebug()<<"MA[4] "<< match_array[4].postion<<" , "<<match_array[4].value;

	CUR_OP("calling update_parameters() in overlap computation");
	update_parameters();

    //**********************Video output render*******************************
    // Input Textures: picture textures
    // Renders to: vo fbo
    // Description: draw for video ouput

    if(is_videooutput)
    {
        CUR_OP("screen render (mode 2)");
        m_program->setUniformValue(m_rendermode_loc, 0.0f);
        CUR_OP("setting vertexAttribPointer for screen render (mode 2)");

            glVertexAttribPointer(m_texAttr, 3, GL_FLOAT, GL_FALSE, 0,
                    verticesTex);

        CUR_OP("binding frambuffer for screen render (mode 2)");
        glBindFramebuffer(GL_FRAMEBUFFER,vo.video_output_fbo);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        CHECK_GL_ERROR(__FILE__,__LINE__);
        glClear(GL_COLOR_BUFFER_BIT);

        glViewport(0,0,vo.width,vo.height);

        CUR_OP("drawing elements for screen render (mode 2)");
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, indices);
        CHECK_GL_ERROR(__FILE__,__LINE__);

    }
	//**********************Pix to screen render*******************************
	// Input Textures: picture textures
	// Renders to: screen back buffer
	// Description: display picture

	if(!is_calculating)
	{
		CUR_OP("screen render (mode 2)");
		m_program->setUniformValue(m_rendermode_loc, 2.0f);
		CUR_OP("setting vertexAttribPointer for screen render (mode 2)");
		if(trackonly)
			glVertexAttribPointer(m_texAttr, 3, GL_FLOAT, GL_FALSE, 0,
					verticesTRO);
		else
			glVertexAttribPointer(m_texAttr, 3, GL_FLOAT, GL_FALSE, 0,
					verticesTex);

		CUR_OP("binding frambuffer for screen render (mode 2)");
		glBindFramebuffer(GL_FRAMEBUFFER,0);
		glDrawBuffer(GL_BACK);
		CHECK_GL_ERROR(__FILE__,__LINE__);
		glClear(GL_COLOR_BUFFER_BIT);

		glViewport(0,
				int(float(height())*retinaScale*(1.0-FRAME_Y_RATIO)),
				int(float(width())*retinaScale),
				int(float(height())*retinaScale*FRAME_Y_RATIO));

		CUR_OP("drawing elements for screen render (mode 2)");
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, indices);
		CHECK_GL_ERROR(__FILE__,__LINE__);

		//*************soundwaveform and pix of sound to screen****************
		// Input Textures:
		// Renders to: screen back buffer
		// Description: display picture
		CUR_OP("soundwaveform render (mode 3)");
		m_program->setUniformValue(m_rendermode_loc, 3.0f);

		CUR_OP("setting vertexAttribPointer for soundwaveform render (mode 3)");
		glVertexAttribPointer(m_texAttr, 3, GL_FLOAT, GL_FALSE, 0, verticesTex);
		CHECK_GL_ERROR(__FILE__,__LINE__);
		glBindFramebuffer(GL_FRAMEBUFFER,0);
		glDrawBuffer(GL_BACK);

		glViewport(0,0, width() * retinaScale*WFMzoom, height() *
				retinaScale*(1.0-FRAME_Y_RATIO));

		CUR_OP("drawing elements for soundwaveform render (mode 3)");
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, indices);
		CHECK_GL_ERROR(__FILE__,__LINE__);
	}

	//*************************************************************************
	// Overlap Compute with new coordinates
	CUR_OP("overlap computer with new coordinates");
	float bestvalue = 1.0f+(overlap[3] - ((float)(bestloc)/2000.0));
	float bestvalueoffset =
			1.0f+(overlap[3] - ((float)(bestmatch.postion)/2000.0));

	GLfloat verticesTRO_ForFile[] ={
		bounds[0], overlap[3], 0, // bottom left corner
		bounds[0], bestvalueoffset, 0, // top left corner
		bounds[1], bestvalueoffset, 0, // top right corner
		bounds[1], overlap[3], 0 // bottom right corner
	};

	//***********************Audio RENDER for file*****************************
	// Input Textures: prev_adj_frame_texture
	// Renders to: output_audio_texture
	// Description: computes audio from prev texture between x and y
	//   calculated space.

	CUR_OP("audio render for file (mode 1.5)");
	m_program->setUniformValue(m_rendermode_loc, 1.5f);
	CUR_OP("set vertexAttribPointer  for audio render for file (mode 1.5)");
	glVertexAttribPointer(m_texAttr, 3, GL_FLOAT, GL_FALSE, 0,
			verticesTRO_ForFile);
	CHECK_GL_ERROR(__FILE__,__LINE__);
	CUR_OP("binding audio_file_fbo for audio render for file (mode 1.5)");
	glBindFramebuffer(GL_FRAMEBUFFER,audio_file_fbo);

	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	glViewport(0,0, 2, samplesperframe_file);

	glClear(GL_COLOR_BUFFER_BIT);

	//  glBindTexture(GL_TEXTURE_2D,adj_frame_texture);

	CUR_OP("drawing elements for audio render for file (mode 1.5)");
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, indices);
	CHECK_GL_ERROR(__FILE__,__LINE__);

	glBindFramebuffer(GL_FRAMEBUFFER,audio_file_fbo);
	glReadBuffer(GL_COLOR_ATTACHMENT0);

	// is_rendering = recording to filebuffer
	// new_frame indicates a frame texture was loaded
	if (is_rendering && new_frame )
	{
		CUR_OP("reading left channel for audio render for file (mode 1.5)");
		//copy float buffer out for file left channel
		glReadPixels(0, 0, 1, samplesperframe_file,GL_RED, GL_FLOAT,
				&FileRealBuffer[0][samplepointer]);

		CUR_OP("reading right channel for audio render for file (mode 1.5)");
		//copy float buffer out for file right channel
		glReadPixels(1, 0, 1, samplesperframe_file,GL_RED, GL_FLOAT,
				&FileRealBuffer[1][samplepointer]);

		samplepointer+=samplesperframe_file;
	}
	CHECK_GL_ERROR(__FILE__,__LINE__);

	CUR_OP("binding fbo 0 for audio render for file (mode 1.5)");
	glBindFramebuffer(GL_FRAMEBUFFER,0);

	CUR_OP("disable vertex array");
	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(0);
	CHECK_GL_ERROR(__FILE__,__LINE__);

	CUR_OP("release m_program");
	m_program->release();

	CUR_OP("increment frame counter");
	++m_frame;

	CUR_OP("");

	new_frame=false;
}

void Frame_Window::PrepareRecording(int numsamples)
{
	FileRealBuffer= new float* [2];

	FileRealBuffer[0] = new float[numsamples];
	FileRealBuffer[1] = new float[numsamples];

	if(logger)
	{
		(*logger) << "FileRealBuffer = [" << FileRealBuffer[0] <<
				"," << FileRealBuffer[1] << "] (2x" << numsamples << "\n";
	}
	fprintf(stderr, "FileRealBuffer = [%p,%p]\n",
			FileRealBuffer[0], FileRealBuffer[1]);
	fflush(stderr);

	samplepointer=0;
}

void Frame_Window::DestroyRecording()
{
	delete [] FileRealBuffer[1];
	delete [] FileRealBuffer[0];

	delete[] FileRealBuffer ;
	FileRealBuffer = NULL;
	samplepointer=0;
}

void Frame_Window::ProcessRecording(int numsamples)
{
	Dsp::Filter* fh =
			new Dsp::SmoothedFilterDesign<Dsp::RBJ::Design::LowPass, 2> (1024);
	Dsp::Params hparams;
	hparams[0] = 48000; // sample rate
	hparams[1] = 13500; // cutoff frequency
	hparams[2] = 4.5; // Q
	fh->setParams (hparams);
	fh->process (numsamples,( FileRealBuffer));

	delete fh;

	Dsp::Filter* fl =
			new Dsp::SmoothedFilterDesign<Dsp::RBJ::Design::HighPass, 2> (1024);
	Dsp::Params lparams;
	lparams[0] = 48000; // sample rate
	lparams[1] = 50; // cutoff frequency
	lparams[2] = 1.5; // Q
	fl->setParams (lparams);
	fl->process (numsamples,( FileRealBuffer));

	delete fl;

	if (stereo == 2.0) //push pull
	{
		float phasefixed;
		for (int i = 0; i< numsamples; i++)
		{
			phasefixed =  ( FileRealBuffer[0][i]-FileRealBuffer[1][i]) /2.0;
			FileRealBuffer[1][i]=phasefixed;
			FileRealBuffer[0][i]=phasefixed;
		}
	}


}
void Frame_Window::PrepareVideoOutput(FrameTexture * frame)
{
	if(vo.video_output_fbo!=0)
	{
		glDeleteFramebuffers(1,&vo.video_output_fbo);
		glDeleteTextures(1,&vo.video_output_texture);
	}
	if(vo.videobuffer!=NULL)
		delete[] vo.videobuffer;


	vo.videobuffer=frame->buf;

	vo.height=frame->height;
	vo.width= frame->width;
	glGenTextures(1,&vo.video_output_texture);
	glActiveTexture(GL_TEXTURE9);
	glBindTexture(GL_TEXTURE_2D, vo.video_output_texture);
	CHECK_GL_ERROR(__FILE__,__LINE__);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	CHECK_GL_ERROR(__FILE__,__LINE__);

	glGenFramebuffers(1,&vo.video_output_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER,vo.video_output_fbo);

	glBindTexture(GL_TEXTURE_2D, vo.video_output_texture);
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,frame->width,frame->height,0,
			GL_RGBA,GL_UNSIGNED_BYTE,NULL);
	glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,
			vo.video_output_texture,0);

	glActiveTexture(GL_TEXTURE0);
}

void Frame_Window::read_frame_texture(FrameTexture * frame)
{
	if(vo.videobuffer == NULL) return;

	glBindBuffer(GL_PIXEL_PACK_BUFFER,0);

	//  glPixelStorei(GL_UNPACK_SWAP_BYTES,0);

	glBindFramebuffer(GL_FRAMEBUFFER,vo.video_output_fbo);

	glReadBuffer(GL_COLOR_ATTACHMENT0);

	// Reference Point: LSJ-20170519-1322
	// See mainwindow.cpp:LSJ-20170519-1322
	glReadPixels(0, 0, frame->width, frame->height,GL_RGBA,
			GL_UNSIGNED_INT_8_8_8_8_REV, vo.videobuffer); //copy buffer out to
}

float *Frame_Window::GetCalibrationMask()
{
	/*
	float *buf = new float[cal_points];

	glBindFramebuffer(GL_FRAMEBUFFER,audio_fbo);
	glReadBuffer(GL_COLOR_ATTACHMENT4);
	glReadPixels(0, 0, 1, cal_points,GL_RED, GL_FLOAT,buf);
	*/

	float *buf2 = new float[cal_points*2];
	glActiveTexture(GL_TEXTURE7);
	glBindTexture(GL_TEXTURE_2D, cal_audio_texture);
	glGetTexImage(GL_TEXTURE_2D,0,GL_RED,GL_FLOAT,buf2);

	return buf2;
}

void Frame_Window::SetCalibrationMask(const float *mask)
{
	CHECK_GL_ERROR(__FILE__,__LINE__);
	glActiveTexture(GL_TEXTURE7);
	glBindTexture(GL_TEXTURE_2D, cal_audio_texture);
	CHECK_GL_ERROR(__FILE__,__LINE__);

	glTexImage2D(GL_TEXTURE_2D,0,GL_R32F,2,cal_points,0,GL_LUMINANCE,
			GL_FLOAT,mask);

	CHECK_GL_ERROR(__FILE__,__LINE__);

	glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT4,GL_TEXTURE_2D,
			cal_audio_texture,0);
	CHECK_GL_ERROR(__FILE__,__LINE__);
}


float Frame_Window::GetAverage(GLfloat* dArray, int iSize)
{
	double dSum = dArray[0];
	for (int i = 1; i < iSize; ++i)
	{
		dSum += dArray[i];
	}
	return float (dSum/iSize);
}

void Frame_Window::GetBestMatchFromFloatArray(GLfloat* dArray, int iSize,
		int start , overlap_match &bmatch)
{
	int iCurrMin = 0;
	float iCurrMinValue = 0;

	for (int i = 1; i < iSize; ++i)
	{
		if (dArray[iCurrMin] > dArray[i])
		{
			iCurrMin =i;
			bmatch.postion= start-  i;
			bmatch.value = dArray[i];
		}
	}
}

int Frame_Window::GetMinLoc(GLfloat* dArray, int iSize)
{
	int iCurrMin = 0;

	for (int i = 1; i < iSize; ++i)
	{
		if (dArray[iCurrMin] > dArray[i])
		{
			iCurrMin = i;
		}
	}
	return iCurrMin;
}

float Frame_Window::GetMin(GLfloat* dArray, int iSize)
{
	int iCurrMin = 0;

	for (int i = 1; i < iSize; ++i)
	{
		if (dArray[iCurrMin] > dArray[i])
		{
			iCurrMin = i;
		}
	}
	return dArray[iCurrMin];
}

float Frame_Window::GetMax(GLfloat* dArray, int iSize)
{
	int iCurrMax= 0;

	for (int i = 1; i < iSize; ++i)
	{
		if (dArray[iCurrMax] < dArray[i])
		{
			iCurrMax = i;
		}
	}
	return dArray[iCurrMax];
}

void Frame_Window::ParamUpdateCallback(FrameWindowCallbackFunction cb,
		void *userData)
{
	this->paramUpdateCB = cb;
	this->paramUpdateUserData = userData;
}

void Frame_Window::mouseReleaseEvent(QMouseEvent *mouse)
{
	if(mouse->button() == Qt::LeftButton) mouseEvent(mouse);
}

void Frame_Window::mouseEvent(QMouseEvent *mouse)
{
	static float x,y,ny;
	static int grab = -1;
	static int hover = -1;

	static const struct {
		GLfloat *targ;
		float *src;
		float maxval;
		enum Qt::CursorShape cursor;
	} grabArr[] = {
		{ &(this->bounds[0]), &x, 1.0, Qt::SplitHCursor },
		{ &(this->bounds[1]), &x, 1.0, Qt::SplitHCursor },
		{ &(this->pixbounds[0]), &x, 1.0, Qt::SplitHCursor },
		{ &(this->pixbounds[1]), &x, 1.0, Qt::SplitHCursor },
		{ &(this->overlap[2]), &ny, 0.35, Qt::SplitVCursor },
		{ &(this->overlap[3]), &y, 0.35, Qt::SplitVCursor },
		{ NULL, NULL, 0, Qt::ArrowCursor }
	};

	static int i;

	// horizontal boundary markers are recorded as ratio of image width
	x = float(mouse->x())/float(this->width());

	// frame pitch markers are recorded as ratio of frame height
	y = float(mouse->y())/(float(this->height()*FRAME_Y_RATIO));

	if(y > 1.0) return;

	ny = 1.0 - y;

	// button release?
	if(mouse->button() == Qt::LeftButton &&
			!(mouse->buttons() & Qt::LeftButton))
	{
		grab = -1;
		hover = -1;
	}

	// new click?
	if(mouse->button() == Qt::LeftButton)
	{
		if(hover != -1)
		{
			grab = hover;
		}
		else if(this->bounds[0] == 0 && this->bounds[1] == 0)
		{
			this->bounds[0] = x - 0.05;
			if(this->bounds[0] < 0) this->bounds[0] = 0;

			this->bounds[1] = x + 0.05;
			if(this->bounds[1] > 1.0) this->bounds[1] = 1.0;

			// copy the changes to the shader program
			this->update_parameters();

			// copy the changes back to the GUI, if requested
			if(this->paramUpdateCB)
				(*(this->paramUpdateCB))(this->paramUpdateUserData);

			this->renderNow();
		}
		return;
	}

	if(grab == -1)
	{
		// free roaming -- looking for things to grab:
		for(i=0; grabArr[i].targ; ++i)
		{
			if(fabs(*(grabArr[i].src) - *(grabArr[i].targ)) < 0.01)
			{
				hover = i;
				break;
			}
		}
		this->setCursor(grabArr[i].cursor);
		if(grabArr[i].targ == NULL) hover = -1;
	}
	else if(*(grabArr[grab].src) >= 0 &&
			*(grabArr[grab].src) <= (grabArr[grab].maxval))
	{
		// update the value in the frame_view_gl class
		*(grabArr[grab].targ) = *(grabArr[grab].src);

		// copy the changes to the shader program
		this->update_parameters();

		// copy the changes back to the GUI, if requested
		if(this->paramUpdateCB)
			(*(this->paramUpdateCB))(this->paramUpdateUserData);

		this->renderNow();
	}
}
