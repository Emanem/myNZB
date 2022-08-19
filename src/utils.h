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

#ifndef _UTILS_H_
#define _UTILS_H_

#include <string>
#include <sstream>
#include <algorithm>

template<typename T>
std::string XtoS(const T& in) {
	std::ostringstream ostr;
	ostr << in;
	return ostr.str();
}

inline bool StoI(const std::string& in, int& ret) {
	std::istringstream istr(in);
	istr >> ret;
	if (istr) return true;
	return false;
}

inline std::string lc(const std::string& in) {
	std::string ret = in;
	std::transform(ret.begin(), ret.end(), ret.begin(), tolower);
	return ret;
}

#endif /*_UTILS_H_*/

