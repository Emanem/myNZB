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

#ifndef _RES_POOL_H_
#define _RES_POOL_H_

#include <memory>
#include <vector>
#include "shared_ptr.h"
#include "mt.h"

template<typename T>
class res_pool {
	struct data_cnt {
		T	data;
		bool	in_use;

		data_cnt() : in_use(true) {
		}
	};

	typedef shared_ptr<data_cnt>	SP_data_cnt;

	// we do need a SP_data_cnt and not simply a data_cnt
	// because due to relocations of std::vector<> all the 
	// referrred variables (eg. data_cnt.in_use) would go
	// awol triggering a Segmentation Fault
	std::vector<SP_data_cnt>	_vec_data;
	mt::Mutex			_mtx;

	res_pool() {
	}

	res_pool(const res_pool&);
	res_pool& operator=(const res_pool&);
public:
	class handle {
		friend class res_pool;
		
		T& 		_data;
		bool&		_in_use;
		mt::Mutex&	_mtx;

		handle(T& data, bool& in_use, mt::Mutex& mtx) : _data(data), _in_use(in_use), _mtx(mtx) {
		}

		handle(const handle&);
		handle& operator=(const handle&);
	public:
		T& get(void) {
			return _data;
		}
		
		~handle() {
			mt::ScopedLock	_sl(_mtx);
			_in_use=false;
		}
	};

	typedef std::unique_ptr<handle>	AP_handle;
	
	static res_pool<T>& Instance(void) {
		static res_pool<T> _r;
		return _r;
	}

	void Reset(void) {
		mt::ScopedLock	_sl(_mtx);
		for(typename std::vector<SP_data_cnt>::const_iterator it = _vec_data.begin(); it != _vec_data.end(); ++it)
			if ((*it)->in_use) {
				LOG_WARNING << "Cannot reset res_pool: in use " << (void*)&(*it) << std::endl;
				return;
			}
		_vec_data.clear();
	}

	AP_handle GetHandle(void) {
		mt::ScopedLock	_sl(_mtx);
		
		// search for an available connection
		for(typename std::vector<SP_data_cnt>::const_iterator it = _vec_data.begin(); it != _vec_data.end(); ++it) {
			if (!(*it)->in_use) {
				(*it)->in_use=true;
				LOG_DEBUG << "res_pool existing element " << (void*)&((*it)->data) << std::endl;
				return AP_handle(new handle((*it)->data, (*it)->in_use, _mtx));
			}
		}
		// otherwise push one back
		_vec_data.push_back(new data_cnt());
		typename std::vector<SP_data_cnt>::reverse_iterator last = _vec_data.rbegin();
		LOG_DEBUG << "res_pool new element" << std::endl;
		return AP_handle(new handle((*last)->data, (*last)->in_use, _mtx));
	}

	~res_pool() {
	}
};

#endif /*_RES_POOL_H_*/

