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

#include <stdexcept>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <sys/types.h>
#include <dirent.h>
#include <cstring>
#include <locale>
#include "nzb_file.h"
#include "settings.h"
#include "utils.h"

namespace {
	class xml_string {
		xmlChar*	_str;
	public:
		xml_string(xmlChar* c) {
			_str=c;
		}

		std::string operator()(void) {
			return std::string((const char*)_str);
		}

		~xml_string() {
			if (_str) xmlFree(_str);
		}
	};

	class xml2_doc {
		xmlDocPtr	_doc;
	public:
		xml2_doc(const char* docname) {
			_doc = xmlParseFile(docname);
		}

		xmlDocPtr operator()(void) {
			return _doc;
		}

		~xml2_doc() {
			xmlFreeDoc(_doc);
		}
	};

	std::string alphanum_lower(const std::string& in) {
		std::string	out;
		for(std::string::const_iterator it = in.begin(); it != in.end(); ++it)
			out += std::isalnum(*it) ? std::tolower(*it) : '_';

		return out;
	}
}

static void list_dir(const std::string& dir, std::vector<std::string>& content) {
	content.clear();
	DIR* d = opendir(dir.c_str());
	if(d) {
		struct dirent * de = NULL;
		while ((de = readdir(d))) {
			if (de->d_type == DT_REG) content.push_back(lc(de->d_name));
		}
		closedir(d);
	}
}

static bool is_already(const std::vector<std::string>& content, const std::string& fname) {
	for(std::vector<std::string>::const_iterator it = content.begin(); it != content.end(); ++it) {
		const std::string	lc_fname = alphanum_lower(fname),
					lc_it_str = alphanum_lower(*it);
		if (strstr(lc_fname.c_str(), lc_it_str.c_str()))
			return true;
	}
	return false;
}

static bool is_par(const std::string& fname) {
	if (!settings::NNTP_skip_par) return false;
	const char *p = strstr(fname.c_str(), ".par2");
	if (p) return true;
	return false;
}

std::string get_NZB_filename(const std::string& fname) {
	std::string ret = fname;
	// get rid of the last '/'
	std::size_t 	p_lslash = ret.rfind('/');
	if (std::string::npos != p_lslash) {
		ret = ret.substr(p_lslash+1);
	}
	std::size_t 	p_ext = ret.rfind(".nzb");
	if (std::string::npos != p_ext && (p_ext+4)==ret.length()) ret = ret.substr(0, p_ext);
	return ret;
}

std::string get_NZB_dirname(const std::string& fname) {
	std::string ret = fname;
	// get rid of the last '/'
	std::size_t 	p_lslash = ret.rfind('/');
	if (std::string::npos != p_lslash) {
		ret = ret.substr(p_lslash+1);
	}
	std::size_t 	p_ext = ret.rfind(".nzb");
	if (std::string::npos != p_ext && (p_ext+4)==ret.length()) ret = ret.substr(0, p_ext);
	else ret += ".files";
	return ret;
}

void get_NZB_content(const std::string& filename, std::vector<NZB_file>& files, const std::string& outdir) {
	xml2_doc doc(filename.c_str());
	if (!doc()) throw std::runtime_error("Can't open NZB file (not XML)");
	xmlNodePtr cur = xmlDocGetRootElement(doc());
	if (!cur) throw std::runtime_error("Can't open NZB file (XML main element missing)");
	if (xmlStrcmp(cur->name, (const xmlChar *) "nzb")) throw std::runtime_error("File is not NZB");
	files.clear();
	// scan the directories
	std::vector<std::string>	already_files;
	list_dir(outdir, already_files);
	// now parse children
	cur = cur->xmlChildrenNode;
	while(cur) {
		if (!xmlStrcmp(cur->name, (const xmlChar *)"file")) {
			// we're in file element get he subject
			std::string			subject(lc(xml_string(xmlGetProp(cur, (const xmlChar*)"subject"))()));
			std::set<std::string>		groups;
			std::vector<std::string>	segments;
			// here check if we have to skip or not
			if (!is_par(subject) && !is_already(already_files, subject)) {
				xmlNodePtr file_cnt = cur->xmlChildrenNode;
				if (!file_cnt) throw std::runtime_error("NZB file is corrupted");
				while(file_cnt) {
					// get groups information
					if (!xmlStrcmp(file_cnt->name, (const xmlChar *) "groups")) {
						xmlNodePtr groups_cnt = file_cnt->xmlChildrenNode;
						if (!groups_cnt) throw std::runtime_error("NZB file is corrupted (groups missing)");
						while(groups_cnt) {
							if (!xmlStrcmp(groups_cnt->name, (const xmlChar *) "group")) {
								groups.insert(xml_string(xmlNodeListGetString(doc(), groups_cnt->xmlChildrenNode, 1))());
							}
							groups_cnt = groups_cnt->next;
						}
					}
					// get segments info
					if (!xmlStrcmp(file_cnt->name, (const xmlChar *) "segments")) {
						xmlNodePtr segs_cnt = file_cnt->xmlChildrenNode;
						if (!segs_cnt) throw std::runtime_error("NZB file is corrupted (segments missing)");
						while(segs_cnt) {
							if (!xmlStrcmp(segs_cnt->name, (const xmlChar *) "segment")) {
								segments.push_back(xml_string(xmlNodeListGetString(doc(), segs_cnt->xmlChildrenNode, 1))());
							}
							segs_cnt = segs_cnt->next;
						}
					}
					file_cnt = file_cnt->next;
				}
				// here the results
				LOG_INFO << "Adding [" << subject << "]" << std::endl;
				files.push_back(NZB_file(subject, groups, segments));
			} else {
				LOG_INFO << "Skipping [" << subject << "]" << std::endl;
			}
		}
		cur = cur->next;
	}
}

