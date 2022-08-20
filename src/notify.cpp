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

#include <dlfcn.h>
#include "notify.h"
#include "settings.h"

typedef void  (*notify_init_t)(const char *);
typedef void  (*notify_uninit_t)(void);
typedef void* (*notify_notification_new_t)(const char *, const char *, char *, char *);
typedef void  (*notify_notification_set_timeout_t)( void *, int );
typedef int   (*notify_notification_update_t)(void *, const char *, const char *, const char *);
typedef void  (*notify_notification_show_t)(void *, char *);
typedef int   (*notify_notification_close_t)(void *, void *);
typedef void  (*g_object_unref_t)(void *);


namespace {
	class shared_object {
		void*	_handle;

		shared_object(const shared_object&);
		shared_object& operator=(const shared_object&);
	public:
		shared_object(const char* so_name) {
			_handle = dlopen(so_name, RTLD_LAZY|RTLD_GLOBAL);
		}

		operator bool() const {
			return 0!=_handle;
		}

		void *sym(const char* s) {
			if (!_handle) return 0;
			return dlsym(_handle, s);
		}

		~shared_object() {
			if (_handle) dlclose(_handle);
		}
	};

	class libnotify_ldr {
		shared_object	lnotify;
		// functions
		notify_init_t				notify_init;
		notify_uninit_t				notify_uninit;
		notify_notification_new_t		notify_notification_new;
		notify_notification_set_timeout_t	notify_notification_set_timeout;
		notify_notification_update_t		notify_notification_update;
		notify_notification_show_t		notify_notification_show;
		notify_notification_close_t		notify_notification_close;
		g_object_unref_t			g_object_unref;
		//
		bool					_is_ok;

		libnotify_ldr() : lnotify(settings::LIB_NOTIFY.c_str()), _is_ok(false) {
			if (lnotify) {
				notify_init = (notify_init_t)lnotify.sym("notify_init");
				notify_uninit = (notify_uninit_t)lnotify.sym("notify_uninit");
				notify_notification_new = (notify_notification_new_t)lnotify.sym("notify_notification_new");
				notify_notification_set_timeout = (notify_notification_set_timeout_t)lnotify.sym("notify_notification_set_timeout");
				notify_notification_update = (notify_notification_update_t)lnotify.sym("notify_notification_update");
				notify_notification_show = (notify_notification_show_t)lnotify.sym("notify_notification_show");
				notify_notification_close = (notify_notification_close_t)lnotify.sym("notify_notification_close");
				g_object_unref = (g_object_unref_t)lnotify.sym("g_object_unref");

				if (notify_init && notify_uninit && notify_notification_new &&
				    notify_notification_set_timeout && notify_notification_update && notify_notification_show &&
				    notify_notification_close && g_object_unref)
					_is_ok = true;
			}
			_is_ok = settings::USE_LIB_NOTIFY && _is_ok;

			if (_is_ok) notify_init("myNZB");
		}

		libnotify_ldr(const libnotify_ldr&);
		libnotify_ldr& operator=(const libnotify_ldr&);
	public:
		static libnotify_ldr& Instance(void) {
			static libnotify_ldr	_l;
			return _l;
		}

		void message(const std::string& title, const std::string& msg, const int& tmout = -1) {
			if (_is_ok){
				void* notification = notify_notification_new(title.c_str(), msg.c_str(), 0, 0);
				notify_notification_set_timeout(notification, tmout);
				notify_notification_show(notification, 0);
				g_object_unref(notification);
			}
		}

		~libnotify_ldr() {
			if (_is_ok) notify_uninit();
		}
	};
}

void notify::init(void) {
	libnotify_ldr::Instance();
}

void notify::message(const std::string& title, const std::string& msg, const int& tmout) {
	libnotify_ldr::Instance().message(title, msg, tmout);
}

