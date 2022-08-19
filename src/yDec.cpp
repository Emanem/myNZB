/*
*	myNZB (C) 2009-2022 E. Oriani, ema <AT> fastwebnet <DOT> it
*
*	This file is part of myNZB.
*
*	myNZB is free software: you can redistribute it and/or modify
*	it under the terms of the GNU General Public License as published by
*	the Free Software Foundation, either version 3 of the License, or
*	(at your option) any later version.
*
*	myNZB is distributed in the hope that it will be useful,
*	but WITHOUT ANY WARRANTY; without even the implied warranty of
*	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*	GNU General Public License for more details.
*
*	You should have received a copy of the GNU General Public License
*	along with myNZB.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "yDec.h"
#include "settings.h"
#include <iostream>
#include <cstdlib>

bool yDec(const std::string& fNameIn, const std::string& outdir) {
	const std::string cmdline = settings::YYDECODE_PATH + " -f -l \"" + fNameIn + "\" -D \"" + outdir + "\"";
	LOG_INFO << "Executing yydecode (" << cmdline << ") ..." << std::endl;
	const int ret = system(cmdline.c_str());
	LOG_INFO << "yydecode done (" << ret << ")" << std::endl;
	if ( 0==ret ) return true;
	return false;
}

