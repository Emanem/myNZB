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

#ifndef _SETTINGS_H_
#define _SETTINGS_H_

#include <string>
#include <iostream>

#define	LEVEL_LOG_ERROR		(0x01)
#define	LEVEL_LOG_WARNING	(0x02)
#define	LEVEL_LOG_INFO		(0x04)
#define	LEVEL_LOG_DEBUG		(0x08)

#define	__LOG(x)		if ( x & settings::LOG) std::cerr

#define LOG_ERROR 	__LOG(LEVEL_LOG_ERROR) << "[ERROR] "
#define LOG_WARNING	__LOG(LEVEL_LOG_WARNING) << "[WARNING] "
#define LOG_INFO 	__LOG(LEVEL_LOG_INFO) << "[INFO] "
#define LOG_DEBUG 	__LOG(LEVEL_LOG_DEBUG) << "[DEBUG] "

namespace settings {

extern std::string 		NNTP_server;
extern unsigned short int	NNTP_port;
extern std::string 		NNTP_user;
extern std::string 		NNTP_pass;
extern bool			NNTP_strict_yenc;
extern bool			NNTP_skip_par;
extern char 			LOG;
extern int			NNTP_max_conn;
extern int			NNTP_wait_segment;
extern int			NNTP_recv_tmout;
extern int			NNTP_wait_conn;
extern std::string		YYDECODE_PATH;
extern std::string		OUT_DIR;
extern std::string		TMP_DIR;
extern bool			OUT_DIR_NAME;
extern bool			USE_CONN_POOL;
extern bool			TEMPORARY_FILES;
extern float			MAX_PERC_REPAIR;
extern int			MAX_SEG_RETRY;
extern std::string		LIB_NOTIFY;
extern bool			USE_LIB_NOTIFY;
extern unsigned short		WWW_PORT;
extern std::string		WWW_PASS;
extern bool			USE_SSL;

}

#endif /*_SETTINGS_H_*/

