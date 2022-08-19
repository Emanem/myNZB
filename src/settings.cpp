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

#include "settings.h"

namespace settings {

std::string 		NNTP_server = "";
unsigned short int	NNTP_port = 0;
std::string 		NNTP_user = "";
std::string 		NNTP_pass = "";
bool			NNTP_strict_yenc = false;
bool			NNTP_skip_par = false;
char			LOG = 0x07;
int			NNTP_max_conn = 2;
int			NNTP_wait_segment = 0;
int			NNTP_recv_tmout = 2000;
int			NNTP_wait_conn = 0;
std::string		YYDECODE_PATH="/usr/bin/yydecode";
std::string		OUT_DIR = "./";
std::string		TMP_DIR = "./";
bool			OUT_DIR_NAME = false;
bool			USE_CONN_POOL = true;
bool			TEMPORARY_FILES = false;
float			MAX_PERC_REPAIR = 0.2;
int			MAX_SEG_RETRY = 3;
std::string		LIB_NOTIFY = "libnotify.so.1";
bool			USE_LIB_NOTIFY = true;
unsigned short		WWW_PORT = 0;
std::string		WWW_PASS = "";
bool			USE_SSL = false;

}

