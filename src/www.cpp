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

#include "www.h"
#include "settings.h"
#include "mt.h"
#include "connection.h"
#include "utils.h"
#include <cstring>
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <sstream>
#include <ctime>

namespace {
	const char	www_top[]="<html>"
"<head>"
"<style type=\"text/css\">"
"body {"
"margin-left: 0%;"
"margin-right: 0%;"
"margin-top: 0%;"
"margin-bottom: 0%;"
"}"
"span.m_title {"
"color: black;"
"font-family: 'arial';"
"font-size: 24px;"
"font-weight: bold;"
"padding:10px 5px;"
"}"
"span.m_leg {"
"color: white;"
"font-family: 'arial';"
"font-size: 12px;"
"font-weight: normal;"
"padding-top: 5px;"
"padding-bottom: 5px;"
"}"
"span.m_fname {"
"color: black;"
"font-family: 'arial';"
"font-size: 14px;"
"font-weight: normal;"
"padding-top: 5px;"
"padding-bottom: 5px;"
"}"
"span.m_fstatus {"
"color: darkblue;"
"font-family: 'arial';"
"font-size: 14px;"
"font-weight: italic;"
"padding-top: 5px;"
"padding-bottom: 5px;"
"}"
"span.m_err {"
"color: red;"
"font-family: 'arial';"
"font-size: 14px;"
"font-weight: normal;"
"padding-top: 5px;"
"padding-bottom: 5px;"
"}"
"span.m_log {"
"color: black;"
"font-family: 'arial';"
"font-size: 10px;"
"font-weight: light;"
"padding:10px 5px;"
"}"
"input.ibutton {"
"font-size: 10px;"
"}"
"input.iinput {"
"font-size: 10px;"
"border: 1px solid;"
"}"
"td.ltd {"
"padding-left:8px;"
"}"
"td.lrtd {"
"padding-left:8px;"
"padding-right:8px;"
"}"
"</style>"
"<title>myNZB - Activity monitor web page</title>"
"</head>"
"<body>"
"<table cellspacing=\"0\" cellpadding=\"0\" width=\"100%\">"
"<tr><td><span class=\"m_title\">myNZB</span></td></tr>"
"<tr bgcolor=\"gray\" height=\"5\"><td></td></tr>"
"	<tr height=\"2\"><td></td></tr>\n"
"	<tr><td>"
"	<table cellpadding=\"0\" cellspacing=\"0\">\n",

			www_mid[] = "</table>"
"	</td></tr>"
"	<tr height=\"2\"><td></td></tr>\n"
"	<tr bgcolor=\"gray\" height=\"5\"><td></td></tr>\n",

			www_down[] = "</table>"
"</body>"
"</html>";

	std::string string_html(const std::string& in) {
		std::ostringstream oss;
		for(std::string::const_iterator it=in.begin(); it != in.end(); ++it)
			if (*it != ' ') oss << *it;
			else oss << "&nbsp;";
		return oss.str();
	}

	std::string html_get_row(const std::string& fname, const std::string& status, const std::string& progress, int& cnt) {
		std::string	color="#F0F0F0";
		if (((cnt++)%2) == 0) color="white";
		std::ostringstream oss;
		oss	<< "<tr bgcolor=\"" << color << "\"><td class=\"ltd\"><span class=\"m_fname\">" << string_html(fname) << "</span></td> <td class=\"ltd\"><span class=\"m_fstatus\">"
			<< string_html(status) << "</span></td> <td width=\"100%\" class=\"lrtd\">" << progress << "</td></tr>\n";
		return oss.str();
	}

	std::string html_get_info(const std::string& info) {
		std::ostringstream oss;
		oss	<< "<tr><td>\n<table><tr><td><span class=\"m_log\">Page generated at " << info << "</span></td>"
			<< "<td><form action=\"/\" method=\"get\" style=\"margin:0\"><input type=\"submit\" value=\"Refresh\" class=\"ibutton\" /></form></td>"
			<< "</tr>\n</table></td></tr>";
		return oss.str();
	}

	std::string get_localtime(void) {
		time_t 		ctm = time(NULL);
		struct tm	tm;
		char		buf[64];
		localtime_r(&ctm, &tm);
		return asctime_r(&tm, buf);
	}

	bool get_http_action(const char* buf, std::string& action) {
		const char 		*CRLF = "\r\n";
		const std::string	f_line(&buf[0], (size_t)(strstr(&buf[0], CRLF) - &buf[0]));
		const size_t		s_1 = f_line.find(' ');
		if (s_1 != std::string::npos) {
			action=f_line.substr(0, s_1);
			return true;
		}
		return false;
	}

	bool get_http_path(const char* buf, std::string& path) {
		const char 		*CRLF = "\r\n";
		const std::string	f_line(&buf[0], (size_t)(strstr(&buf[0], CRLF) - &buf[0]));
		const size_t		s_1 = f_line.find(' ');
		if (s_1 != std::string::npos) {
			const size_t s_2 = f_line.find(' ', s_1+1);
			if (s_2 != std::string::npos) {
				path=f_line.substr(s_1+1, s_2-s_1-1);
				return true;
			}
		}
		return false;
	}

	bool get_http_sfield(const char* buf, const std::string& field, std::string& value) {
		const char 		*CRLF = "\r\n";
		const std::string	crlf_field = std::string(CRLF)+field+":";
		const char		*p_field = strstr(&buf[0], crlf_field.c_str()),
					*p_field_term = (p_field) ? strstr(p_field + crlf_field.length(), CRLF) : 0;
		if (p_field && p_field_term) {
			value= std::string(p_field + crlf_field.length(), p_field_term);
			return true;
		}
		return false;
	}

	bool get_http_ifield(const char* buf, const std::string& field, int& value) {
		std::string svalue;
		if (get_http_sfield(buf, field, svalue) && StoI(svalue, value))
			return true;
		return false;
	}

	class www_srv : public mt::Thread {
		volatile bool			_run;
		std::unique_ptr<Connection>	_srv;
	public:
		www_srv() : _run(true), _srv(nullptr) {
		}

		virtual void run(void);

		void stop(void) {
			_run = false;
			if (_srv.get()) _srv->Close();
		}	
	};

	class f_status_hndl {
		www_srv					_th_srv;
		mt::Mutex				_mtx,
							_mtx_progress;
		std::map<std::string, std::string>	_status;
		std::set<std::string>			_sids;
		std::string				_current;
		std::vector<char>			_progress;

		f_status_hndl() {
			if (0 != settings::WWW_PORT) _th_srv.start();
		}

		f_status_hndl(const f_status_hndl&);
		f_status_hndl& operator=(const f_status_hndl&);
	public:
		static f_status_hndl& Instance(void) {
			static f_status_hndl _f;
			return _f;
		}

		std::string add_sid(void) {
			std::string ret = XtoS(rand());
			for(int i=0; i < 4; ++i)
				ret += XtoS(rand());
			mt::ScopedLock	_sl(_mtx);
			_sids.insert(ret);
			return ret;
		}

		bool check_sid(const std::string& sid) {
			mt::ScopedLock	_sl(_mtx);
			std::set<std::string>::const_iterator it = _sids.find(sid);
			return it != _sids.end();
		}

		bool check_sid_http(const char* buf, std::string& sid) {
			std::string	c_sid;
			if (get_http_sfield(buf, "Cookie", c_sid)) {
				// check sid is ok
				std::size_t p_sid = c_sid.find("sid=");
				if (std::string::npos != p_sid) {
					sid = c_sid.substr(p_sid+4);
					return check_sid(sid);
				}
			}
			return false;
		}

		void update(const std::string& file, const std::string& status) {
			mt::ScopedLock	_sl(_mtx);
			_status[file]=status;
		}

		void progress_reset(const std::string& current, const int& sz) {
			mt::ScopedLock	_sl(_mtx_progress);
			_current = current;
			_progress.resize(0);
			_progress.resize(sz);
		}

		void progress_set(const int& idx, const char status) {
			mt::ScopedLock	_sl(_mtx_progress);
			if ((int)_progress.size() > idx) _progress[idx] = status;
		}

		std::string get_progress_html(const std::string& nzb) {
			std::vector<char>	c_progress;
			{
				mt::ScopedLock	_sl(_mtx_progress);
				if (_current != nzb) return "";
				c_progress = _progress;
			}
			std::ostringstream	oss;
			int			idx = 0;
			oss	<< "<table width=\"100%\" height=\"8\" cellpadding=\"0\" cellspacing=\"0\"><tr>";
			for(std::vector<char>::const_iterator it = c_progress.begin(); it != c_progress.end(); ++it, ++idx) {
				oss << "<td bgcolor=\"";
				switch(*it) {
					case www::S_UNDEF:
						oss << "lightgrey\"></td>";
						break;
					case www::S_DL:
						oss << "yellow\"></td>";
						break;
					case www::S_OK:
						oss << "green\"></td>";
						break;
					case www::S_ERR:
						oss << "red\"></td>";
						break;
					default:
						oss << "blue\"></td>";
						break;
				}
				if (0 == (idx%16)) oss << '\n';
			}
			oss	<< "</tr></table>\n";
			return oss.str();	
		}

		void get_status_html(std::string& in, const std::string& sid = "") {
			std::map<std::string, std::string>	t_status;
			{
				mt::ScopedLock	_sl(_mtx);
				t_status = _status;
			}
			int					cnt=0;	
			std::ostringstream			oss;
			oss	<< www_top
				<< "\n<tr bgcolor=\"black\"><td class=\"ltd\"><span class=\"m_leg\">NZB</span></td><td class=\"ltd\"><span class=\"m_leg\">Files</span></td>"
				<< "<td class=\"lrtd\"><span class=\"m_leg\">Progress</span></td>\n</tr><tr height=\"2\"><td></td><td></td><td></td></tr>\n";
			for(std::map<std::string, std::string>::const_iterator it = t_status.begin(); it != t_status.end(); ++it)
				oss << html_get_row(it->first, it->second, get_progress_html(it->first), cnt);
			oss	<< www_mid;
			oss	<<  html_get_info(get_localtime());
			oss	<< www_down;
			std::ostringstream			http_oss;
			http_oss	<< "HTTP/1.1 200 OK" << "\r\n";
			if (sid.length() > 0) http_oss << "Set-Cookie: sid=" << sid << "\r\n";
			http_oss	<< "Content-Length: " << oss.str().length() << "\r\n\r\n"
					<< oss.str();
			in = http_oss.str();
		}

		void get_login_html(std::string& in, const std::string& msg ="") {
			std::ostringstream			oss;
			oss	<< www_top;
			if (msg.length() > 0)
				oss << "<tr bgcolor=\"white\"><td width=\"5\"></td> <td><span class=\"m_err\">" << msg << "</span></td> </tr>";
			oss 	<< 	"<tr bgcolor=\"white\"><td width=\"5\"></td> <td>"
					"<form action=\"\" method=\"post\" style=\"margin:0\"><span class=\"m_fname\">Enter password <input type=\"password\" name=\"password\" class=\"iinput\"/>"
					"<input type=\"submit\" value=\"Login\" class=\"ibutton\" /></form></span></td> </tr>";
			oss	<< www_mid;
			oss	<<  html_get_info(get_localtime());
			oss	<< www_down;
			std::ostringstream			http_oss;
			http_oss	<< "HTTP/1.1 200 OK" << "\r\n";
			http_oss	<< "Content-Length: " << oss.str().length() << "\r\n\r\n"
					<< oss.str();
			in = http_oss.str();
		}

		~f_status_hndl() {
			_th_srv.stop();
			_th_srv.join();
		}
	};

	// the run method has to be written after
	void www_srv::run(void) {
		try {
			_srv = std::unique_ptr<Connection>(new Connection(settings::WWW_PORT));
			// we do a monothread http req/res handling,
			// we don't expect a lot of HTTP connections...
			_srv->RecvTmOut(250);
			while(_run) {
				std::unique_ptr<Connection> c(_srv->Accept());
				if (!_run || !c.get()) break;
				LOG_INFO << "(www server) : new connection from " << c->GetHost() << std::endl;
				// read the request
				static const char	*TERM = "\r\n\r\n";
				static const int	BUF_SIZE=2048;
				std::vector<char>	buf(BUF_SIZE);
				int			rb = 0;
				// 2 secs tmout
				c->RecvTmOut(2000);
				while(rb < BUF_SIZE) {
					const int c_rb = c->Recv(&buf[rb], BUF_SIZE-rb-1);
					if (c_rb <= 0) {
						c->Close();
						break;
					}
					rb += c_rb;
					buf[rb] = '\0';
					if (strstr(&buf[0], TERM)) break;
					if (rb >= BUF_SIZE) c->Close();
				}
				if (!c->IsConnected()) continue;
				//
				std::string	action,
						http_path;

				if (!get_http_action(&buf[0], action) || !get_http_path(&buf[0], http_path))
					continue;
				LOG_DEBUG << "(www server) : \nbegin http header------\n" << &buf[0] << "\nend http heander--------" << std::endl;
				if (action == "GET") {
					std::string sid;
					// if pass is empty or sid is ok then report the webpage
					if ((settings::WWW_PASS == "") || f_status_hndl::Instance().check_sid_http(&buf[0], sid)) {
						if (http_path == "/") {
							std::string response;
							f_status_hndl::Instance().get_status_html(response, sid);
							c->SendAll(&response[0], response.length());
						}
					} else {
						std::string response;
						f_status_hndl::Instance().get_login_html(response);
						c->SendAll(&response[0], response.length());
					}
				} else if (action == "POST") {
					// get all posted data
					int			c_length = 0;
					if (!get_http_ifield(&buf[0], "Content-Length", c_length) || (c_length <= 0 || c_length > BUF_SIZE)) continue;
					const int 		hdr_size = strstr(&buf[0], TERM) - &buf[0] + 4,
								hdr_data = rb - hdr_size;
					std::vector<char>	data(c_length+1);
					memcpy(&data[0], &buf[hdr_size], hdr_data);
					if (c_length > 0) c->RecvAll(&data[hdr_data], c_length - hdr_data);
					data[c_length] = '\0';
					// we only get password when we get POST on /
					if (http_path == "/") {
						// see if password is ok
						if ((std::string("password=") + settings::WWW_PASS) != &data[0]) {
							std::string response;
							f_status_hndl::Instance().get_login_html(response, "Invalid password!");
							c->SendAll(&response[0], response.length());
							continue;
						}
						// is ok, answer setting the cookie and with all data
						std::string response;
						f_status_hndl::Instance().get_status_html(response, f_status_hndl::Instance().add_sid());
						c->SendAll(&response[0], response.length());
					}
				}
			}
		} catch(std::exception& e) {
			LOG_WARNING << "(www server) : " << e.what() << std::endl;
		}
	}

}

void www::init(void) {
	f_status_hndl::Instance();
}

void www::update(const std::string& file, const std::string& status) {
	f_status_hndl::Instance().update(file, status);
}

void www::progress_reset(const std::string& current, const int& sz) {
	f_status_hndl::Instance().progress_reset(current, sz);
}

void www::progress_set(const int& idx, const char status) {
	f_status_hndl::Instance().progress_set(idx, status);
}
