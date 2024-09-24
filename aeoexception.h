//-----------------------------------------------------------------------------
// This file is part of Virtual Film Bench
//
// Copyright (c) 2024 University of South Carolina and Thomas Aschenbach
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the
// Free Software Foundation; either version 2 of the License, or (at your
// option) any later version.
//
// Virtual Film Bench is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
// for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
//
// Funding for Virtual Film Bench development was provided through a grant from the
// National Endowment for the Humanities
//-----------------------------------------------------------------------------

#ifndef AEOEXCEPTION_H
#define AEOEXCEPTION_H

#include <stdexcept>
#include <QString>

class AeoException : public std::runtime_error
{
public:
	AeoException(const char *msg) : std::runtime_error(std::string(msg)) {}
	AeoException(const std::string &msg = "") : std::runtime_error(msg) {}
	AeoException(const QString &msg) : std::runtime_error(msg.toStdString()) {}
};

#endif // AEOEXCEPTION_H
