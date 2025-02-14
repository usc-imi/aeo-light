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
#include "mainwindow.h"

#include <QApplication>
#include <QtPlugin>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);

	a.setOrganizationName("Interdisciplinary Mathematics Institute");
	a.setOrganizationDomain("imi.cas.sc.edu");
	a.setApplicationName("AEO-Light");
	a.setApplicationVersion(APP_VERSION_STR);

	MainWindow w;

	for(int i=0; i<argc; i++) std::cerr << i << ": " << argv[i] << "\n";
	if(argc > 1) w.SetStartingProject(argv[1]);

    w.resize(700,800);
	w.show();
	return a.exec();

	// If you have any clean-up code to execute, don't put it here.
	// Connect it to the aboutToQuit() signal or use qAddPostRoutine().
}
