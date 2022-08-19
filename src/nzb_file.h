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

#ifndef _NZB_FILE_H_
#define _NZB_FILE_H_

#include <vector>
#include <string>
#include <set>

struct NZB_file {
	std::string			seg_name;
	std::set<std::string>		groups;
	std::vector<std::string>	segments;

	NZB_file(const std::string& _seg_name, const std::set<std::string>& _groups, const std::vector<std::string>& _segments) : seg_name(_seg_name), groups(_groups), segments(_segments) {
	}
};

extern std::string get_NZB_filename(const std::string& fname);
extern std::string get_NZB_dirname(const std::string& fname);
extern void get_NZB_content(const std::string& filename, std::vector<NZB_file>& files, const std::string& outdir);

#endif /*_NZB_FILE_H_*/

