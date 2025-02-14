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
#ifndef OPENGLWINDOW_H
#define OPENGLWINDOW_H

#include <QtGui/QWindow>
#include <QDialog>
#include <QtGui/QOpenGLFunctions>
#include <QTextStream>

class QPainter;
class QOpenGLContext;
class QOpenGLPaintDevice;

class OpenGLWindow : public QWindow, protected QOpenGLFunctions
{
	Q_OBJECT
public:
	explicit OpenGLWindow(QWindow *parent = 0);
	~OpenGLWindow();

	virtual void render(QPainter *painter);
	virtual void render();

	virtual void initialize();

	void setAnimating(bool animating);

	void PrintGLVersion(QTextStream &stream);
	QString GLVersionString();
	int MajorVersion() {return m_context->format().majorVersion();}

public slots:
	void renderLater();
	void renderNow();

protected:
	bool event(QEvent *event) Q_DECL_OVERRIDE;

	void exposeEvent(QExposeEvent *event) Q_DECL_OVERRIDE;

private:
	bool m_update_pending;
	bool m_animating;

	QOpenGLContext *m_context;
	QOpenGLPaintDevice *m_device;
};

#endif
