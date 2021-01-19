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

#ifndef _CCONNECTION_H_
#define _CCONNECTION_H_

#ifdef _WIN32
#include <winsock2.h>
#else
typedef int	SOCKET;
#endif
#include <string>
#include <vector>
#include <memory>

struct sslInfo;

class Connection {
	SOCKET			_sd;
	unsigned int		_ip;
	unsigned short		_port;
	std::string		_host;
	unsigned int		_sentData,
				_recvData;
	std::auto_ptr<sslInfo>	_ssl;

	std::string GetStringIp(void);
	//
	static std::vector<unsigned int>	_currIPs;
	static unsigned short			_currPort;
public:
	Connection();
	Connection(unsigned int ip, unsigned short port, const bool useSSL);
	Connection(const std::string& address, unsigned short port, const bool useSSL);
	Connection(unsigned short port);	// To bind Socket
	~Connection();
	//
	Connection *Accept(void);
	//
	int Send(const char *buffer, unsigned int size);
	int Recv(char *buffer, unsigned int size);
	bool SendAll(const char *buffer, unsigned int size);
	bool RecvAll(char *buffer, unsigned int size);
	//
	bool RecvTmOut(int msec=0);
	//
	void Close(void);
	//
	const char *GetHost(void);
	unsigned int GetIP(void);
	unsigned short GetPort(void);
	//
	bool IsConnected(void);
	//
	unsigned int GetSentData(bool reset = true);
	unsigned int GetRecvData(bool reset = true);
	//
	const char *GetLastError(void);
	//
	static bool Init(void);
	static void Cleanup(void);
	static void AddLocalServerPort(unsigned short port);
	static bool IsGoodIpAddress(unsigned int ip, unsigned short port);
	static unsigned int GetLocalIP(void);
	static unsigned short GetLocalPort(void);
	static void InitSSL(void);
private:
	Connection(const Connection&);
	Connection& operator=(const Connection&);
};

#endif //_CCONNECTION_H_
