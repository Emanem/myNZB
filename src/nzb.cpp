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

#include "nzb.h"

#include <cstring>
#include <cstdlib>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <sstream>
#include <memory>
#include <set>
#include <sys/types.h>
#include <unistd.h>
#include "utils.h"
#include "connection.h"
#include "settings.h"
#include "mt.h"
#include "yDec.h"
#include "connmgr.h"
#include "sig.h"
#include "res_pool.h"
#include "www.h"

class bstream {
public:
	virtual const std::string id(void) const = 0;

	virtual void write(const char* buf, const size_t sz) = 0;

	virtual size_t read(char *buf, const size_t sz) = 0;

	virtual bstream* rewind(void) = 0;

	virtual ~bstream() {
	}
};

class f_bstream : public bstream {
	const std::string	_fname;
	std::fstream		_str;
public:
	f_bstream(const char* fname) : _fname(fname), _str(fname, std::ios::binary|std::ios_base::in|std::ios_base::out) {
		if (!_str.good()) {
			const std::string	err = std::string("Can't open (r/w) file (") + _fname + ")";
			throw std::runtime_error(err.c_str());
		}
	}

	virtual const std::string id(void) const {
		return _fname;
	}

	virtual void write(const char* buf, const size_t sz) {
		_str.write(buf, sz);
		if(!_str) throw std::runtime_error("Can't write file");
	}

	virtual size_t read(char *buf, const size_t sz) {
		_str.read(buf, sz);
		return _str.gcount();
	}

	virtual bstream* rewind(void) {
		_str.seekg(0, std::ios_base::beg);
		_str.seekp(0, std::ios_base::beg);

		return this;
	}
};

class m_bstream : public bstream {
	std::stringstream	_str;
public:
	m_bstream() {
	}

	virtual const std::string id(void) const {
		std::ostringstream	oss;
		oss << "memory (" << (void*)this << ")";

		return oss.str();
	}

	virtual void write(const char* buf, const size_t sz) {
		_str.write(buf, sz);
		if(!_str) throw std::runtime_error("Can't write memory");
	}

	virtual size_t read(char *buf, const size_t sz) {
		_str.read(buf, sz);
		return _str.gcount();
	}

	virtual bstream* rewind(void) {
		_str.seekg(0, std::ios_base::beg);
		_str.seekp(0, std::ios_base::beg);

		return this;
	}
};

typedef shared_ptr<bstream>		SP_bstream;	

typedef res_pool<std::vector<char> >	BufPool;

// On many implementation of std::vector resize should do;
// this method should be internally done in the std::vector
// implementation, but is not guarantee, so...
static inline void vec_adjust_sz(std::vector<char>& in, const size_t sz) {
	const size_t	in_sz = in.size();
	LOG_DEBUG << "vec " << (void*)&in << " size/capacity " << in_sz << '/' << in.capacity() << std::endl;
	// only resize it if it's needed
	if (sz > in_sz) {
		in.resize(sz);
		LOG_DEBUG << "vec " << (void*)&in << " size/capacity (after resize) " << in.size() << '/' << in.capacity() << std::endl;
	}
}

// see http://tools.ietf.org/html/rfc3977
// section 3.1.1 list 6
// provided std::vector<char> has to be '\0' terminated
static int dot_unstuff(std::vector<char>& v) {
	const static char	*DOT_STUFF="\r\n..";
	BufPool::AP_handle	hndl = BufPool::Instance().GetHandle();
	std::vector<char>&	tmp = hndl->get();
	const size_t		expected_sz = v.size();
	vec_adjust_sz(tmp, expected_sz);
	const char		*p_in = &v[0],
				*next_dstuff = 0;
	char			*p_out = &tmp[0];
	int			ret = 0;
	
	while((next_dstuff = strstr(p_in, DOT_STUFF))) {
		++ret;
		const int cpy_sz = next_dstuff-p_in+3;
		if (cpy_sz < 0) throw std::runtime_error("Malformed news BODY");
		memcpy(p_out, p_in, cpy_sz);
		p_out += cpy_sz;
		p_in += cpy_sz+1;
	}
	const int cpy_sz = &v[v.size()] - p_in;
	memcpy(p_out, p_in, cpy_sz);
	p_out += cpy_sz;
	tmp.resize(p_out - &tmp[0]+1);
	tmp[tmp.size()-1] = '\0';
	v.swap(tmp);
	return ret;
}

static int fetch_term(Connection& c, std::vector<char>& buf, const std::string& term) {
	const int	_max_read = buf.size()-1;
	int 		rb = 0;
	do {
		sig::test_raised();
		int c_rb = c.Recv(&buf[0] + rb, _max_read-rb);
		if (c_rb <= 0) {
			throw std::runtime_error("Connection forcibly closed by NNTP server or timed out");
		}
		rb += c_rb;
		buf[rb] = '\0';
		char *p_term = 	strstr(&buf[0], term.c_str());
		if (p_term) {
			*p_term = '\0';
			return p_term - &buf[0];
		}
	} while(rb < _max_read);
	throw std::runtime_error("Buffer too small to fetch terminator");
}

// process authentication
static void auth(Connection& c, std::vector<char>& buf) {
	static const char*	CRLF = "\r\n";
	const std::string	N_A_USERNAME = std::string("AUTHINFO USER ") + settings::NNTP_user + CRLF,
				N_A_PASSWORD = std::string("AUTHINFO PASS ") + settings::NNTP_pass + CRLF;
	
	if (N_A_USERNAME.length()==0) throw std::runtime_error("Username is required but not provided");
	c.SendAll(N_A_USERNAME.c_str(), N_A_USERNAME.length());
	fetch_term(c, buf, CRLF);
	if (!memcmp(&buf[0], "381 ", sizeof(char)*4)) {
		if (N_A_PASSWORD.length()==0) throw std::runtime_error("Password is required but not provided");
		c.SendAll(N_A_PASSWORD.c_str(), N_A_PASSWORD.length());
		fetch_term(c, buf, CRLF);
		if ('2' != buf[0]) {
			std::ostringstream oss;
			oss << "NNTP AUTH failed [ " << &buf[0] << " on AUTHINFO PASS ]";
			throw std::runtime_error(oss.str());
		}
		return;
	}
	std::ostringstream oss;
	oss << "NNTP AUTH failed (no AUTH PASS requested by server) [ " << &buf[0] << " on AUTHINFO USER ]";
	throw std::runtime_error(oss.str());
}

static void fetch_cmd(Connection& c, const std::string& r, std::vector<char>& buf) {
	static const char*	CRLF = "\r\n";

	if (!r.empty()) {
		const std::string toSend = r+CRLF;
		c.SendAll(toSend.c_str(), toSend.length());
	}
	fetch_term(c, buf, CRLF);
	// if code is 480 you have to authenticate
	if (!memcmp(&buf[0], "480 ", sizeof(char)*4)) {
		auth(c, buf);
		return fetch_cmd(c, r, buf);
	}
	// if the code is not 2xx then is error
	if ('2' != buf[0]) {
		std::ostringstream oss;
		oss << "NNTP action not executed [ " << &buf[0] << " on " << r << " ]";
		throw std::runtime_error(oss.str());
	}
}

static void fetch_head(Connection& c, const std::string& r, std::vector<char>& buf, int *p_bytes, int *p_lines) {
	static const char	*CRLF = "\r\n",
				*HEAD_TERM = "\r\n.\r\n";

	if (r.empty()) throw std::runtime_error("A HEAD request is needed");
	const std::string toSend = r+CRLF;
	c.SendAll(toSend.c_str(), toSend.length());
	const int head_size = fetch_term(c, buf, HEAD_TERM);
	// if code is 480 you have to authenticate
	if (!memcmp(&buf[0], "480 ", sizeof(char)*4)) {
		auth(c, buf);
		return fetch_head(c, r, buf, p_bytes, p_lines);
	}
	// if the code is not 2xx then is error
	if ('2' != buf[0]) {
		std::ostringstream oss;
		oss << "NNTP action not executed (HEAD) [ " << &buf[0] << " on " << r << " ]";
		throw std::runtime_error(oss.str());
	} 
	// all to lowercase
	for(int i =0; i < head_size; ++i) 
		buf[i] = tolower(buf[i]);
	if (settings::NNTP_strict_yenc) {
		// try to get yEnc on subject line
		const char *p_subject = strstr(&buf[0], "subject:");
		if (!p_subject) throw std::runtime_error("NNTP 'Subject:' field not present");
		else {
			const char 	*next_line = strstr(p_subject, CRLF),
					*yenc_token = strstr(p_subject, "yenc");
			if (!next_line) throw std::runtime_error("NNTP 'Subject:' field malformed");
			if (!yenc_token || yenc_token > next_line) throw std::runtime_error("NNTP 'Subject:' not yEnc encoded (strict)");
		}
	} else {
		if (!strstr(&buf[0], "yenc")) throw std::runtime_error("NNTP not yEnc encoded (loose)");
	}
	// try to get "bytes:"
	if (p_bytes) {
		const char* p_info = strstr(&buf[0], "bytes:");
		if(p_info)
			*p_bytes = atoi(p_info+6);
	}
	// try to get "lines:"
	if (p_lines) {
		const char* p_info = strstr(&buf[0], "lines:");
		if(p_info)
			*p_lines = atoi(p_info+6);
	}
}

static void fetch_article(Connection& c, const std::string& r, bstream *filedata, const int sugg_bytes, const int sugg_lines) {
	static const char	*CRLF = "\r\n",
				*HEAD_TERM = "\r\n\r\n",
				*BODY_TERM = "\r\n.\r\n";
	static const int	MB = 16*1024*1024;

	if (r.empty()) throw std::runtime_error("An ARTICLE request is needed");
	const std::string toSend = r+CRLF;
	c.SendAll(toSend.c_str(), toSend.length());
	// let's use suggested size (if any) + 2KB header
	const int		bufSize = (sugg_bytes > -1) ? (sugg_bytes+2048) : MB;
	BufPool::AP_handle	hndl = BufPool::Instance().GetHandle();
	std::vector<char>&	buf = hndl->get();
	vec_adjust_sz(buf, bufSize);
	const int		_max_read = bufSize-1;
	int			rb=0;
	// retrieve till body terminator
	while(rb < _max_read) {
		sig::test_raised();
		const int c_rb = c.Recv(&buf[0] + rb, bufSize-rb);
		// check first recv
		if (0==rb && c_rb>=4) {
			// if code is 480 you have to authenticate
			if (!memcmp(&buf[0], "480 ", sizeof(char)*4)) {
				auth(c, buf);
				return fetch_article(c, r, filedata, sugg_bytes, sugg_lines);
			}
			// report any other code
			if ('2' != buf[0]) {
				buf[c_rb-2] = '\0';
				std::ostringstream oss;
				oss << "NNTP action not executed (BODY) [ " << &buf[0] << " on " << r << " ]";
				throw std::runtime_error(oss.str());
			}
		}
		// check connection
		if (c_rb <= 0) throw std::runtime_error("Connection dropped by server or timed out");
		rb += c_rb;
		if (rb >=5 && !memcmp(&buf[rb-5], BODY_TERM, sizeof(char)*5)) {
			buf[rb] = '\0';
			rb -= dot_unstuff(buf);
			// we should check lines as well...
			char 	*p_head_term = strstr(&buf[0], HEAD_TERM),
				*p_first_line_term = strstr(&buf[0], CRLF);
			if (!p_first_line_term || !p_head_term) throw std::runtime_error("News article is malformed");
			const int write_sz = (&buf[rb] - p_first_line_term - 4 -3);
			if (write_sz < 0) throw std::runtime_error("News body is malformed");
			filedata->write(p_first_line_term+4, write_sz);
			return;
		}
	}
	throw std::runtime_error("News article is too big (>= 16MB)");
}

static void fetch_body(Connection& c, const std::string& r, bstream *filedata, const int sugg_bytes, const int sugg_lines) {
	static const char	*CRLF = "\r\n",
				*BODY_TERM = "\r\n.\r\n";
	static const int	MB = 16*1024*1024;
	
	if (r.empty()) throw std::runtime_error("A BODY request is needed");
	const std::string toSend = r+CRLF;
	c.SendAll(toSend.c_str(), toSend.length());
	// let's use suggested size (if any) + 2KB header
	const int		bufSize = (sugg_bytes > -1) ? (sugg_bytes+2048) : MB;
	BufPool::AP_handle	hndl = BufPool::Instance().GetHandle();
	std::vector<char>&	buf = hndl->get();
	vec_adjust_sz(buf, bufSize);
	const int		_max_read = bufSize-1;
	int			rb=0;
	// retrieve till body terminator
	while(rb < _max_read) {
		sig::test_raised();
		const int c_rb = c.Recv(&buf[0] + rb, bufSize-rb);
		// check first recv
		if (0==rb && c_rb>=4) {
			// if code is 480 you have to authenticate
			if (!memcmp(&buf[0], "480 ", sizeof(char)*4)) {
				auth(c, buf);
				return fetch_body(c, r, filedata, sugg_bytes, sugg_lines);
			}
			// report any other code
			if ('2' != buf[0]) {
				buf[c_rb-2] = '\0';
				std::ostringstream oss;
				oss << "NNTP action not executed (BODY) [ " << &buf[0] << " on " << r << " ]";
				throw std::runtime_error(oss.str());
			}
		}
		// check connection
		if (c_rb <= 0) throw std::runtime_error("Connection dropped by server or timed out");
		rb += c_rb;
		if (rb >=5 && !memcmp(&buf[rb-5], BODY_TERM, sizeof(char)*5)) {
			buf[rb] = '\0';
			rb -= dot_unstuff(buf);
			// we should check lines as well...
			char	*p_first_line_term = strstr(&buf[0], CRLF);
			if (!p_first_line_term) throw std::runtime_error("News article is malformed");
			const int write_sz = (&buf[rb] - p_first_line_term - 2 -3);
			if (write_sz < 0) throw std::runtime_error("News body is malformed");
			filedata->write(p_first_line_term+2, write_sz);
			return;
		}
	}
	throw std::runtime_error("News article is too big (>= 16MB)");
}

bool fetch_nzb_segment(const std::set<std::string>& groups, const std::string& id, bstream *filedata, std::string *err) throw() {
	try {
		const static unsigned int	R_BUF_SIZE = 16*1024;
		const std::string		N_MODE_READER = "MODE READER",
						N_HEAD = "HEAD <" + id + ">",
						N_BODY = "BODY <" + id + ">",
						N_QUIT = "QUIT";
		Connection			*c = NULL;
		// these containers are to hold the connection
		// handles (or local or from the manager)
		std::unique_ptr<Connection>	_conn_holder;
		ConnMgr::AP_ConnHandle 		_manager_conn_holder;

		if (settings::USE_CONN_POOL) {
			_manager_conn_holder = ConnMgr::Instance().GetHandle();
			if (0== _manager_conn_holder.get()) throw std::runtime_error("No connections available");
			c = _manager_conn_holder->get();
		} else {
			_conn_holder = std::unique_ptr<Connection>(new Connection(settings::NNTP_server, settings::NNTP_port, settings::USE_SSL));
			c = _conn_holder.get();
		}
		// set recv tmout
		if (!c->RecvTmOut(settings::NNTP_recv_tmout)) LOG_WARNING << "Can't set NTTP connection timeout (" << settings::NNTP_recv_tmout << ")" << std::endl;
		
		std::vector<char> 	buf(R_BUF_SIZE);
		int			i_bytes = -1,
					i_lines = -1;
		if (err) *err = "";
		// If we use the pool be creaful to initialize the connection
		// only once
		if (settings::USE_CONN_POOL) {
			if (!_manager_conn_holder->already_used())
				fetch_cmd(*c, "", buf);
		} else fetch_cmd(*c, "", buf);

		// set mode reader 
		fetch_cmd(*c, N_MODE_READER, buf);
		// we might have multiple groups
		// try all of them
		for(std::set<std::string>::const_iterator it_grp = groups.begin(); it_grp != groups.end(); ++it_grp) {
			try {
				const std::string	N_GROUP = "GROUP " + *it_grp; 
				fetch_cmd(*c, N_GROUP, buf);
			}
			catch(...) {
				std::set<std::string>::const_iterator it_grp_next = it_grp;
				if(++it_grp_next == groups.end())
					throw;
			}
		}
		// get head...
		fetch_head(*c, N_HEAD, buf, &i_bytes, &i_lines);
		// ...and get the body
		fetch_body(*c, N_BODY, filedata, i_bytes, i_lines);

		// QUIT the connection only if we don't use the pool
		if (!settings::USE_CONN_POOL) fetch_cmd(*c, N_QUIT, buf);
		return true;

	} catch (std::exception& e) {
		if (err) *err = e.what();
	} catch(...) {
		if (err) *err = "Unknonw exception";
	}
	return false;
}

class seg_job : public mt::ThreadPool::Job {
	bool&				_is_ok;
	std::string&			_err;
	const std::string&		_id;
	bstream				*_filedata;
	const std::set<std::string>&	_groups;
	const int			_seg_id;
public:
	seg_job(const std::set<std::string>& groups, const std::string& id, bstream *filedata, bool& is_ok, std::string& err, const int& seg_id) : 
	_is_ok(is_ok), _err(err), _id(id), _filedata(filedata), _groups(groups), _seg_id(seg_id) {
	}

	virtual void run(void) {
		if (settings::NNTP_wait_segment > 0) {
			struct timespec ts = {0};
			ts.tv_sec = settings::NNTP_wait_segment/1000;
			ts.tv_nsec = ((settings::NNTP_wait_segment%1000) * 1000000);
			nanosleep(&ts, NULL);
		}
		www::progress_set(_seg_id, www::S_DL);
		if (fetch_nzb_segment(_groups, _id, _filedata, &_err)) {
			_is_ok=true;
			LOG_INFO << "\tOk [" << _seg_id << "] " << _id << std::endl;
			www::progress_set(_seg_id, www::S_OK);
		} else {
			_is_ok=false;
			LOG_WARNING << "\tError [" << _seg_id << "] " << _id << " (" << _err << ')' << std::endl;
			www::progress_set(_seg_id, www::S_ERR);
		}
	}
};

namespace {
// this class is a helper guaranteeing to delete temporary files in case of exceptions
class auto_filer {
	std::set<std::string>	_files;
public:
	auto_filer() {
	}

	void add(const std::string& f) {
		_files.insert(f);
	}

	void remove(const std::string& f) {
		std::set<std::string>::const_iterator it = _files.find(f);
		if (_files.end() != it) {
			_files.erase(it);
			if (-1 == ::remove(f.c_str())) {
				LOG_WARNING << "Can't remove temporary file (" << f << ')' << std::endl;
			}
		}
	}

	~auto_filer() {
		for(std::set<std::string>::const_iterator it = _files.begin(); it != _files.end(); ++it)
			::remove(it->c_str());
	}
};

typedef shared_ptr<seg_job> 	SP_seg_job;

struct job_info {
	std::string	err;
	SP_bstream	filedata;
	SP_seg_job	job;
	bool		is_ok;
};

}

// this function checks that all needed managers/pools/singletons
// are initialized. This is NOT MT safe, so has to be called in a ST
// context
void init_nzb_struct(void) {
	static bool	__init=false;
	if (!__init) {
		ConnMgr::Instance();
		BufPool::Instance();
		__init=true;
	}
}

bool fetch_nzb_file(const std::set<std::string>& groups, const std::vector<std::string>& vec_id, const std::string& outdir, const bool& tmp_files) {
	// check at the beginning of the function
	sig::test_raised();

	auto_filer			af;
	const int			n_seg = vec_id.size();
	std::vector<job_info>		vec_job_info(n_seg);
	mt::ThreadPool			tp(settings::NNTP_max_conn);
	const std::string		f_name_base = settings::TMP_DIR + ".myNZB.tmp." + XtoS(getpid()) + "." + XtoS(rand());

	LOG_INFO << "Downloading (groups " << *groups.begin() << ") segments:" << std::endl;
	for(int i=0; i < n_seg; ++i) {
		if(tmp_files) {
			const std::string	fname = f_name_base + "." + XtoS(i);
			vec_job_info[i].filedata = new f_bstream(fname.c_str());
			af.add(fname);
		} else {
			vec_job_info[i].filedata = new m_bstream();
		}
		vec_job_info[i].job = new seg_job(groups, vec_id[i], vec_job_info[i].filedata->rewind(), vec_job_info[i].is_ok, vec_job_info[i].err, i);
		tp.add(vec_job_info[i].job.get());
		LOG_INFO << "\t" << vec_id[i] << " (" << vec_job_info[i].filedata->id() << ")" << std::endl;
	}
	// wait for all jobs
	for(int i=0; i < n_seg; ++i) {
		vec_job_info[i].job->wait();
		vec_job_info[i].job = SP_seg_job(0);
	}
	// check here for signal raised after all threads have done
	// we do not check later because in case some jobs are completed
	// try to yDec those
	sig::test_raised();
	// reset connection pool
	if (settings::USE_CONN_POOL) {
		ConnMgr::Instance().Reset();
		LOG_INFO << "Connection pool reset" << std::endl;
	}
	// check the results
	bool	is_all_ok = true;
	int	n_err = 0;
	for(int i=0; i < n_seg; ++i) {
		if (!vec_job_info[i].is_ok) {
			LOG_ERROR << "Error on segment (" << i << ") : " <<  vec_job_info[i].err << std::endl;
			is_all_ok=false;
			++n_err;
		}
	}
	if (!is_all_ok) {
		// check to re download bad segments, limit is 20%
		if ((float)n_err/n_seg <= settings::MAX_PERC_REPAIR) {
			LOG_WARNING << "Broken segments are " << 100.00*n_err/n_seg << "%. Trying to re download ..." << std::endl;
			is_all_ok=true;
			for(int i=0; i < n_seg; ++i) {
				if (!vec_job_info[i].is_ok) {
					LOG_INFO << "Re fetching segment " << vec_id[i] << " ... " << std::endl;
					for(int j=0; j < settings::MAX_SEG_RETRY; ++j) {
						vec_job_info[i].is_ok = fetch_nzb_segment(groups, vec_id[i], vec_job_info[i].filedata->rewind(), &vec_job_info[i].err);
						if(vec_job_info[i].is_ok) {
							www::progress_set(i, www::S_OK);
							break;
						}
					}
					if(!vec_job_info[i].is_ok) {
						LOG_ERROR << "Aborting re download: failed segment (" << i << ") : " << vec_job_info[i].err << std::endl;
						is_all_ok=false;
						break;
					}
				}
			}
		} else {
			LOG_ERROR << "Broken segments are " << 100.00*n_err/n_seg << "% (max is " << 100.0*settings::MAX_PERC_REPAIR << "%). Aborting re download" << std::endl;
		}
		// we check here again in case we've downloaded bad segments
		// tmp files get auto deleted with the auto_filer
		if (!is_all_ok) return false;
	}
	// concatenate all files to pass to yydecode
	const std::string 	big_file = f_name_base + ".big";
	af.add(big_file);
	std::ofstream		ostr(big_file.c_str());
	const static int	buf_size = 1024*1024;
	std::vector<char>	filebuf(buf_size);
	if (!ostr) throw std::runtime_error("Can't open (w) big file");
	for(int i=0; i < n_seg; ++i) {
		while(true) {
			const size_t rb = vec_job_info[i].filedata->read(&filebuf[0], buf_size);
			if (0==rb) break;
			ostr.write(&filebuf[0], rb);
			if (!ostr) throw std::runtime_error("Can't write big file");
		}
		vec_job_info[i].filedata = SP_bstream(0);
	}
	ostr.close();
	// finally yydecode the file
	if (!yDec(big_file, outdir)) {
		LOG_ERROR << "yydecode failed on file \"" << big_file << "\"" << std::endl;
		return false;
	}
	return true;
}
