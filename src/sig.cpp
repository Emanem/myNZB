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

#include "sig.h"
#include "settings.h"
#include <signal.h>

typedef void (*sig_hndl)(int);

static volatile bool	sig_received = false;
static sig_hndl		def_sig_int,
			def_sig_term;

extern "C" {

static void stop_sig(int interrupt) {
	LOG_WARNING << "INT/TERM signal received, myNZB terminating sequence initiated" << std::endl;
	sig_received = true;
	// set signals back to the default value
	signal(SIGINT, def_sig_int);
	signal(SIGTERM, def_sig_term);
}

}

void sig::init(void) {
	def_sig_int = signal(SIGINT, stop_sig);
	if (SIG_ERR == def_sig_int) throw std::runtime_error("SIGINT can't be set");
	def_sig_term = signal(SIGTERM, stop_sig);
	if (SIG_ERR == def_sig_term) throw std::runtime_error("SIGTERM can't be set");
}

void sig::test_raised(void) {
	if (sig_received) throw sig::except("An INT/TERM signal has been received");
}

