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

#ifndef _CONNMGR_H_
#define _CONNMGR_H_

#include <vector>
#include <memory>
#include <time.h>
#include "connection.h"
#include "shared_ptr.h"
#include "mt.h"

// we might need a connection manager (don't open and close TCP/IP connections each time)
class ConnMgr {
	typedef shared_ptr<Connection>	SP_Connection;

	struct ConnData {
		SP_Connection	conn;
		bool		in_use;
		bool		already_used;

		ConnData(Connection *c) : conn(c), in_use(true), already_used(false) {
		}
	};

	typedef shared_ptr<ConnData>	SP_ConnData;

	const size_t			_n_c;
	// we do need a SP_ConnData and not simply a ConnData
	// because due to relocations of std::vector<> all the 
	// referrred variables (eg. ConnData.in_use) would go
	// awol triggering a Segmentation Fault
	std::vector<SP_ConnData>	_vec_conn;
	mt::Mutex			_mtx;

	ConnMgr(const int& n_c) : _n_c(n_c) {
	}

	ConnMgr(const ConnMgr&);
	ConnMgr& operator=(const ConnMgr&);
public:
	class ConnHandle {
		friend class ConnMgr;
		Connection* 	_c;
		bool&		_in_use;
		const bool	_already_used;
		mt::Mutex&	_mtx;
		ConnHandle(Connection* c, bool& in_use, const bool& already_used, mt::Mutex& mtx) : _c(c), _in_use(in_use), _already_used(already_used), _mtx(mtx) {
		}

		ConnHandle(const ConnHandle&);
		ConnHandle& operator=(const ConnHandle&);
	public:
		Connection* get(void) {
			return _c;
		}
		
		const bool& already_used(void) {
			return _already_used;
		}

		~ConnHandle() {
			mt::ScopedLock	_sl(_mtx);
			_in_use=false;
		}
	};

	typedef std::auto_ptr<ConnHandle>	AP_ConnHandle;
	
	static ConnMgr& Instance(void) {
		static ConnMgr _c(settings::NNTP_max_conn);
		return _c;
	}

	void Reset(void) {
		mt::ScopedLock	_sl(_mtx);
		for(std::vector<SP_ConnData>::const_iterator it = _vec_conn.begin(); it != _vec_conn.end(); ++it)
			if ((*it)->in_use) {
				LOG_WARNING << "Cannot reset connections pool: in use " << (void*)&(*it) << std::endl;
				return;
			}
		_vec_conn.clear();
	}

	AP_ConnHandle GetHandle(void) {
		mt::ScopedLock	_sl(_mtx);
		
		// search for an available connection
		for(size_t i=0; i < _vec_conn.size(); ++i) {
			if (!_vec_conn[i]->in_use) {
				_vec_conn[i]->in_use=true;
				if (!_vec_conn[i]->conn->IsConnected()) {
					LOG_WARNING << "New connection required (current has been closed/dropped)" << std::endl;
					if(settings::NNTP_wait_conn > 0) {
						struct timespec ts = {0};
						ts.tv_sec = settings::NNTP_wait_conn/1000;
						ts.tv_nsec = ((settings::NNTP_wait_conn%1000) * 1000000);
						nanosleep(&ts, NULL);
					} 
					_vec_conn[i]->conn = new Connection(settings::NNTP_server, settings::NNTP_port, settings::USE_SSL);
					_vec_conn[i]->already_used = false;
				} else {
					_vec_conn[i]->already_used = true;
				}
				return AP_ConnHandle(new ConnHandle(_vec_conn[i]->conn.get(), _vec_conn[i]->in_use, _vec_conn[i]->already_used, _mtx));
			}
		}
		// otherwise push one back
		if (_vec_conn.size() < _n_c) {
			_vec_conn.push_back(new ConnData(new Connection(settings::NNTP_server, settings::NNTP_port, settings::USE_SSL)));
			std::vector<SP_ConnData>::reverse_iterator last = _vec_conn.rbegin();
			return AP_ConnHandle(new ConnHandle((*last)->conn.get(), (*last)->in_use, (*last)->already_used, _mtx));
		}
		return AP_ConnHandle(0);
	}

	~ConnMgr() {
	}
};

#endif /*_CONNMGR_H_*/

