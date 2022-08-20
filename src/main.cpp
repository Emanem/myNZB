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

#include <iostream>
#include <stdexcept>
#include <limits>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <map>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <unistd.h>
#include <dirent.h>
#include <cerrno>
#include "settings.h"
#include "connection.h"
#include "nzb.h"
#include "nzb_file.h"
#include "notify.h"
#include "sig.h"
#include "utils.h"
#include "www.h"

const static std::string	__myNZB__ = "myNZB",
				__version__ = "0.3.1";

const static int		__UID__ = geteuid(),
				__GID__ = getegid();

// Macros to see if a file/directory is rwx by current user
#define IS_O_RWX(x)	((x & (S_IROTH|S_IWOTH|S_IXOTH))==(S_IROTH|S_IWOTH|S_IXOTH))
#define IS_G_RWX(x, d)	(((int)d==__GID__) && (S_IRGRP|S_IWGRP|S_IXGRP)==(x & (S_IRGRP|S_IWGRP|S_IXGRP)))
#define IS_U_RWX(x, d)	(((int)d==__UID__) && (S_IRUSR|S_IWUSR|S_IXUSR)==(x & (S_IRUSR|S_IWUSR|S_IXUSR)))
// a file/directory is rwx by current user if
// (u+rwx) OR (not_user AND (g+rwx OR (not_group AND o+rxw)))
#define IS_F_RWX(x, ud, gd)	(IS_U_RWX(x, ud) || (((int)ud!=__UID__) && (IS_G_RWX(x, gd) || (((int)gd!=__GID__) && IS_O_RWX(x)))))

// Macros to see if a file is rx by current user
#define IS_O_RX(x)	((x & (S_IROTH|S_IXOTH))==(S_IROTH|S_IXOTH))
#define IS_G_RX(x, d)	(((int)d==__GID__) && (S_IRGRP|S_IXGRP)==(x & (S_IRGRP|S_IXGRP)))
#define IS_U_RX(x, d)	(((int)d==__UID__) && (S_IRUSR|S_IXUSR)==(x & (S_IRUSR|S_IXUSR)))
// a file is rx by current user if
// (u+rx) OR (not_user AND (g+rx OR (not_group AND o+rx)))
#define IS_F_RX(x, ud, gd)	(IS_U_RX(x, ud) || (((int)ud!=__UID__) && (IS_G_RX(x, gd) || (((int)gd!=__GID__) && IS_O_RX(x)))))

void print_version(void) {
	std::cerr <<	__myNZB__ << " v" << __version__ << " - (C) 2009-2022 E. Oriani\n"
		  <<	std::flush;
}

void print_help(void) {
	print_version();
	std::cerr <<	"Usage: myNZB [options] file0.nzb file1.nzb ...\n"
			"\t-s: NNTP server to connect (use the colon notation to specify the port, eg. \"nntp.server.com:119\")\n"
			"\t-u: NNTP username (default none, eg. \"\")\n"
			"\t-p: NNTP password (default none, eg. \"\")\n"
			"\t-c: number of parallel connections to NNTP server (default 2)\n"
			"\t-t: wait time (ms) in between dowloading NZB segments (default 0)\n"
			"\t-y: yydecode path (default to: \"/usr/bin/yydecode\")\n"
			"\t-o: output directory (default \"./\")\n"
			"\t-d: create a subdirectory with NZB file name for each NZB to process (default not set)\n"
			"\t-x: percentage value to allow retry download segments with errors (default 0.2)\n"
			"\t-r: reset TCP/IP connections with NNTP server for each segment\n"
			"\t-n: do NOT notify file activity through libnotify (dynamically loaded)\n"
			"\t-l: set log level:\n"
			"\t\t0 No log\n"
			"\t\t1 Errors only\n"
			"\t\t2 Warnings and errors\n"
			"\t\t3 Info, warnings and errors (default)\n"
			"\t\t4 Debug, info, warnings and errors\n"
			"\t-z: NZB segments max retries (default 3 retries)\n"
			"\t-C: set wait time (ms) when creating new NNTP connections (default 0, no timeout)\n"
			"\t-e: use SSL connections\n"
			"\t-F: set to force writing into temporary files instead of memory (default not set)\n"
			"\t-P: (try to) skip the \".par2\" files\n"
			"\t-T: set temporary directory (default \"./\")\n"
			"\t-S: set recv timeout (msec) on NNTP connections (default 2000 , 2 sec)\n"
			"\t-W: specify a port:password (optional) for the report HTTP web server (default 0, no web server)\n"
			"\t-Y: strict search for yEnc encoding in NNTP subject (default false)\n"
			"\t-h: print this help and exit\n"
			"\t-v: print version and exit\n"
		 <<	std::flush;
}

int parse_options(int argc, char *argv[]) {
	opterr = 0;
     	int c;

	while ((c = getopt (argc, argv, "s:u:p:c:t:o:x:y:l:z:C:T:S:W:deFhrnvYP")) != -1) {
		switch (c) {
			case 's':
				{
					const char*	p_colon = strchr(optarg, ':');
					if (!p_colon) settings::NNTP_server = optarg;
					else {
						settings::NNTP_server = std::string(optarg, p_colon-optarg);
						settings::NNTP_port = atoi(p_colon+1);
						if (0 >= settings::NNTP_port) settings::NNTP_port = 119;
					}
				}
             			break;
			case 'u':
				settings::NNTP_user = optarg;
				break;
			case 'p':
				settings::NNTP_pass = optarg;
				break;
			case 'c':
				settings::NNTP_max_conn = atoi(optarg);
				if (0 >= settings::NNTP_max_conn) settings::NNTP_max_conn = 1;
				break;
			case 'd':
				settings::OUT_DIR_NAME = true;
				break;
			case 'e':
				settings::USE_SSL = true;
				Connection::InitSSL();
				break;
			case 'F':
				settings::TEMPORARY_FILES = true;
				break;
			case 't':
				settings::NNTP_wait_segment = atoi(optarg);
				if (0 > settings::NNTP_wait_segment) settings::NNTP_max_conn = 1000;
				break;
			case 'y':
				settings::YYDECODE_PATH = optarg;
				break;
			case 'o':
				settings::OUT_DIR = optarg;
				if (settings::OUT_DIR == "") settings::OUT_DIR = "./";
				if ('/' != *settings::OUT_DIR.rbegin())  settings::OUT_DIR += '/';
				break;
			case 'x':
				settings::MAX_PERC_REPAIR = atof(optarg);
				if (settings::MAX_PERC_REPAIR < 0.0) settings::MAX_PERC_REPAIR = 0.0;
				if (settings::MAX_PERC_REPAIR > 1.0) settings::MAX_PERC_REPAIR = 1.0;
				break;
			case 'l':
				if (isdigit(optarg[0])) {
					const int log_ilev = atoi(optarg);
					switch (log_ilev) {
						case 0:
							settings::LOG = 0x00;
							break;
						case 1:
							settings::LOG = 0x01;
							break;
						case 2:
							settings::LOG = 0x03;
							break;
						case 3:
							settings::LOG = 0x07;
							break;
						case 4:
							settings::LOG = 0x0F;
							break;
						default:
							break;
					}
				}
				break;
			case 'z':
				settings::MAX_SEG_RETRY = atoi(optarg);
				if (settings::MAX_SEG_RETRY < 0) settings::MAX_SEG_RETRY = 3;
				break;
			case 'v':
				print_version();
				exit(0);
				break;
			case 'C':
				settings::NNTP_wait_conn = atoi(optarg);
				if (settings::NNTP_wait_conn <= 0 ) settings::NNTP_wait_conn = 0;
				break;
			case 'P':
				settings::NNTP_skip_par = true;
				break;
			case 'T':
				settings::TMP_DIR = optarg;
				if (settings::TMP_DIR == "") settings::TMP_DIR = "./";
				if ('/' != *settings::TMP_DIR.rbegin())  settings::TMP_DIR += '/';
				break;
			case 'S':
				settings::NNTP_recv_tmout = atoi(optarg);
				if (settings::NNTP_recv_tmout <= 0 ) settings::NNTP_recv_tmout = 0;
				break;
			case 'W':
				if (!strchr(optarg, ':')) {
					const int www_port = atoi(optarg);
					if (www_port <= 0) settings::WWW_PORT = 0;
					else if (www_port > std::numeric_limits<unsigned short>::max()) settings::WWW_PORT = std::numeric_limits<unsigned short>::max();
					else settings::WWW_PORT = www_port;
				} else {
					const char		*colon = strchr(optarg, ':');
					const std::string	s_port(optarg, (size_t)(colon-optarg)),
								s_pass(optarg+(size_t)(colon-optarg)+1);
					const int 		www_port = atoi(s_port.c_str());
					if (www_port <= 0) settings::WWW_PORT = 0;
					else if (www_port > std::numeric_limits<unsigned short>::max()) settings::WWW_PORT = std::numeric_limits<unsigned short>::max();
					else settings::WWW_PORT = www_port;
					settings::WWW_PASS = s_pass;
				}
				break;
			case 'r':
				settings::USE_CONN_POOL = false;
				break;
			case 'n':
				settings::USE_LIB_NOTIFY = false;
				break;
			case 'Y':
				settings::NNTP_strict_yenc = true;
				break;
			case 'h':
				print_help();
				exit(0);
				break;
			case '?':
             			if (strchr("supctoxlzTSW", optopt)) {
					std::cerr << "Option -" << (char)optopt << " requires an argument" << std::endl;
					print_help();
					exit(1);
				} else if (isprint (optopt)) {
					std::cerr << "Option -" << (char)optopt << " is unknown" << std::endl;
					print_help();
					exit(1);
				}
				break;
			default:
				std::cerr << "Invalid option: " << c << std::endl;
				print_help();
				exit(1);
				break;
		}
	}
	// set the default SSL port (564) only if SSL not enabled and port not specified yet
	if(0 == settings::NNTP_port) {
		if(settings::USE_SSL) settings::NNTP_port = 564;
		else settings::NNTP_port = 119;
	}
	return optind;
}

void check_basic(void) {
	if (settings::NNTP_server.length() == 0) throw std::runtime_error("NNTP server not specified");
	//
	struct stat64 s;
	//
	if (stat64(settings::OUT_DIR.c_str(), &s)) throw std::runtime_error("Output directory isn't valid");
	if (!S_ISDIR(s.st_mode) || !IS_F_RWX(s.st_mode, s.st_uid, s.st_gid)) throw std::runtime_error("Output directory isn't a directory or can't be R/W by current user");
	//
	if (stat64(settings::TMP_DIR.c_str(), &s)) throw std::runtime_error("Temporary directory isn't valid");
	if (!S_ISDIR(s.st_mode) || !IS_F_RWX(s.st_mode, s.st_uid, s.st_gid)) throw std::runtime_error("Temporary directory isn't a directory or can't be R/W by current user");
	//
	if (stat64(settings::YYDECODE_PATH.c_str(), &s)) throw std::runtime_error("yydecode path isn't valid");
	if (!S_ISREG(s.st_mode) || !IS_F_RX(s.st_mode, s.st_uid, s.st_gid)) throw std::runtime_error("yydecode isn't valid or can't be R/X by current user");
	//
	for(std::string::const_iterator it = settings::WWW_PASS.begin(); it != settings::WWW_PASS.end(); ++it)
		if (!isalnum(*it)) throw std::runtime_error("The specified password for web HTTP report server isn't valid (only alphanumeric chars allowed)");
}

struct NZB_JOB {
	std::string		outdir;
	std::vector<NZB_file>	nzb_files;
};
typedef std::map<std::string, NZB_JOB>	NZB_MAP_JOBS;

int main(int argc, char *argv[]) {
	try {
		const int f_arg = parse_options(argc, argv);
		// init connections, notify, signals ...
		Connection::Init();
		www::init();
		notify::init();
		sig::init();
		init_nzb_struct();
		srand(time(NULL)|1);
		// check that basic paths/config are ok
		check_basic();
		// create a struct for each nzb
		NZB_MAP_JOBS nzb_jobs;
		for(int i = f_arg; i < argc; ++i) {
			try {
				LOG_INFO << "Adding [" << argv[i] << "]..." << std::endl;
				const std::string	nzb_name = get_NZB_filename(argv[i]),
							c_outdir = (settings::OUT_DIR_NAME) ? settings::OUT_DIR + get_NZB_dirname(argv[i]) + '/' : settings::OUT_DIR;
				std::vector<NZB_file>	t_vec;
				if (nzb_name.length() == 0) throw std::runtime_error("Invalid NZB filename");
				// try to create the directory anyway
				if (c_outdir != settings::OUT_DIR) {
					if (mkdir(c_outdir.c_str(), S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH)) {
						if (errno != EEXIST) throw std::runtime_error("Don't/can't create the required subdirectory");
						LOG_WARNING << "Directory [" << c_outdir << "] already exist" << std::endl;
						struct stat64 s;
						if (stat64(c_outdir.c_str(), &s)) throw std::runtime_error("Output subdirectory isn't valid");
						if (!S_ISDIR(s.st_mode) || !IS_F_RWX(s.st_mode, s.st_uid, s.st_gid)) throw std::runtime_error("Output subdirectory isn't a directory or can't be R/W by current user");
					}
				}
				// get the content
				get_NZB_content(argv[i], t_vec, c_outdir);
				nzb_jobs[nzb_name].outdir = c_outdir;
				nzb_jobs[nzb_name].nzb_files = t_vec;
				if (t_vec.empty()) www::update(nzb_name, "Done (Ok)");
				else www::update(nzb_name, std::string("0/") + XtoS(t_vec.size()));
			} catch(std::exception& e) {
				LOG_ERROR << e.what() << std::endl;
			}
		}
		// for each nzb job proceed
		for(NZB_MAP_JOBS::const_iterator itm = nzb_jobs.begin(); itm != nzb_jobs.end(); ++itm) {
			bool				is_ok=true;
			const std::string&		cur_nzb = itm->first;
			const std::vector<NZB_file>&	vec_NZB_file = itm->second.nzb_files;
			const std::string&		outdir = itm->second.outdir;
			int				idx = 1;
			const int			total = vec_NZB_file.size();
			
			LOG_INFO << "Processing NZB [" << cur_nzb << "] ..." << std::endl;
			notify::message(__myNZB__ + " - Processing NZB ...", cur_nzb);
			for(std::vector<NZB_file>::const_iterator it = vec_NZB_file.begin(); it != vec_NZB_file.end(); ++it, ++idx) {
				const std::string curr_part = XtoS(idx) + "/" + XtoS(total);

				LOG_INFO << "Downloading (" << it->seg_name << ") [" << curr_part << "] ..." << std::endl;
				notify::message(__myNZB__ + " - Downloading [" + curr_part + "] ...", it->seg_name);
				www::update(cur_nzb, curr_part);
				www::progress_reset(cur_nzb, it->segments.size());
				if(fetch_nzb_file(it->seg_name, it->groups, it->segments, outdir, settings::TEMPORARY_FILES)) {
					LOG_INFO << "Done (" << it->seg_name << ") [" << curr_part << "]" << std::endl;
					notify::message(__myNZB__ + " - Ok [" + curr_part + "]", it->seg_name, 3000);
				} else {
					LOG_ERROR << "Error (" << it->seg_name << ") [" << curr_part << "]" << std::endl;
					notify::message(__myNZB__ + " - Error [" + curr_part + "]", it->seg_name);
					is_ok=false;
				}
			}
			if (is_ok) {
				LOG_INFO << "NZB completed, all ok" << std::endl;
				notify::message(__myNZB__ + " - NZB completed", cur_nzb);
				www::update(cur_nzb, "Done (Ok)");
			} else {
				LOG_WARNING << "NZB completed, some parts have to be re downloaded" << std::endl;
				notify::message(__myNZB__ + " - NZB completed with errors", cur_nzb);
				www::update(cur_nzb, "Done (Errors)");
			}
		}
		// cleanup connections
		Connection::Cleanup();
		return 0;
	} catch(sig::except& e) {
		LOG_INFO << e.what() << std::endl;
	} catch(std::exception& e) {
		LOG_ERROR << e.what() << std::endl;
	} catch(...) {
		LOG_ERROR << "Unknonw exception" << std::endl;
	}
	return -1;
}
