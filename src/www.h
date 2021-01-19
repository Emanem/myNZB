/*
*	myNZB (C) 2009 E. Oriani, ema <AT> fastwebnet <DOT> it
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

#ifndef _WWW_H_
#define _WWW_H_

#include <string>

namespace www {

static const char	S_UNDEF = 0x00,
			S_DL = 0x01,
			S_OK = 0x02,
			S_ERR = 0x03;

extern void init(void);
extern void update(const std::string& file, const std::string& status = "n/a");
extern void progress_reset(const std::string& current, const int& sz);
extern void progress_set(const int& idx, const char status);

}

#endif /*_WWW_H_*/

