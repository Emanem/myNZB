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

#include "connection.h"
#include "settings.h"

#ifdef _WIN32
#include <windows.h>
typedef int	socklen_t;
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#define	INVALID_SOCKET	(-1)
#define SOCKET_ERROR	(-1)
#endif
#include <sstream>
#include <memory.h>
#include <stdexcept>
#include <cerrno>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <cstdlib>

struct sslInfo {
	SSL 	*sslHandle;
	SSL_CTX *sslContext;

	sslInfo(SOCKET sd) {
		// create context (new context for each connection)
		sslContext = SSL_CTX_new(SSLv23_client_method());
		if(!sslContext) {
			close(sd);
			throw std::runtime_error("Can't initialize ssl context");
		}
		// set default cert location
		if(!SSL_CTX_set_default_verify_paths(sslContext)) {
			SSL_CTX_free(sslContext);
			close(sd);
			throw std::runtime_error("Can't set default certificate location");
		}
		// create handle
		sslHandle = SSL_new(sslContext);
		if(!sslHandle) {
			SSL_CTX_free(sslContext);
			close(sd);
			throw std::runtime_error("Can't initialize new ssl handle");
		}
		// bind socket
		if (!SSL_set_fd(sslHandle, sd)) {
			SSL_shutdown(sslHandle);
			SSL_free(sslHandle);
			SSL_CTX_free(sslContext);
			close(sd);
			throw std::runtime_error("Can't set sd to ssl handle");
		}
		// perform handshake
		const int retOnConnect = SSL_connect(sslHandle);
		if(1 != retOnConnect) {
			// find out the error
			const int	errId = SSL_get_error(sslHandle, retOnConnect);
			char		buf[16];
			snprintf(buf, 15, "%i", errId);
			buf[15]  = '\0'; 
			SSL_shutdown(sslHandle);
			SSL_free(sslHandle);
			SSL_CTX_free(sslContext);
			close(sd);
			throw std::runtime_error(std::string("Can't perform ssl handshake, err code: ") + buf);
		}
		// after connection, check certificate
		X509 *cert = SSL_get_peer_certificate(sslHandle);
      		if(cert) {
        		const long cert_res = SSL_get_verify_result(sslHandle);
        		if(cert_res != X509_V_OK) {
				LOG_WARNING << "X509 SSL certificate is not valid in current chain!" << std::endl;
        		}
        		X509_free(cert);
      		} else {
			LOG_WARNING << "Can't get X509 SSL certificate" << std::endl;
		}
		
	}

	~sslInfo() {
		SSL_free(sslHandle);
		SSL_CTX_free(sslContext);
	}
};

Connection::Connection() :
_sd(INVALID_SOCKET),
_ip(0),
_port(0),
_host(""),
_sentData(0),
_recvData(0)
{
}

Connection::Connection(unsigned int ip, unsigned short port, const bool useSSL) :
_sd(INVALID_SOCKET),
_ip(0),
_port(0),
_host(""),
_sentData(0),
_recvData(0)
{
	_ip = ip;
	_port = port;
	_host = GetStringIp();
	_sd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == _sd) throw std::runtime_error("Can't create new socket!");
	
	struct sockaddr_in saServer;
	
	saServer.sin_addr.s_addr = _ip;
	saServer.sin_port = htons(_port);
	saServer.sin_family = AF_INET;

	if (SOCKET_ERROR == connect(_sd, (struct sockaddr *)&saServer, sizeof(sockaddr_in)))
	{
#ifdef _WIN32
		closesocket(_sd);
#else
		close(_sd);
#endif
		_sd = INVALID_SOCKET;
		throw std::runtime_error("Can't connect socket!");
	}

	if(useSSL) _ssl = std::auto_ptr<sslInfo>(new sslInfo(_sd));
}

Connection::Connection(const std::string& address, unsigned short port, const bool useSSL) :
_sd(INVALID_SOCKET),
_ip(0),
_port(0),
_host(address),
_sentData(0),
_recvData(0)
{
	in_addr		iaHost;
	struct hostent	*lpHostEntry;

	iaHost.s_addr = inet_addr(address.c_str());
	if (iaHost.s_addr == INADDR_NONE) {
		// Wasn't an IP address string, assume it is a name
		lpHostEntry = gethostbyname(address.c_str());
	} else {
		// It was a valid IP address string
		lpHostEntry = gethostbyaddr((const char *)&iaHost, sizeof(struct in_addr), AF_INET);
	}
	if (lpHostEntry == NULL) {
		throw std::runtime_error("Can't resolve host!");
	}

	_ip = ((in_addr*)*lpHostEntry->h_addr_list)->s_addr;
	_port = port;
	_sd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == _sd) {
		throw std::runtime_error("Can't create socket!");
	}
	
	struct sockaddr_in saServer;
	
	saServer.sin_addr.s_addr = _ip;
	saServer.sin_port = htons(_port);
	saServer.sin_family = AF_INET;

	if (SOCKET_ERROR == connect(_sd, (struct sockaddr *)&saServer, sizeof(sockaddr_in))) {
#ifdef _WIN32
		closesocket(_sd);
#else
		close(_sd);
#endif
		_sd = INVALID_SOCKET;
		throw std::runtime_error("Can't connect socket!");
	}

	if(useSSL) _ssl = std::auto_ptr<sslInfo>(new sslInfo(_sd));
}

Connection::Connection(unsigned short port) :
_sd(INVALID_SOCKET),
_ip(INADDR_ANY),
_port(port),
_host(""),
_sentData(0),
_recvData(0)
{
	_host = GetStringIp();
	_sd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == _sd) throw std::runtime_error("Can't create new socket!");

	struct sockaddr_in	sinInterface;
	
	sinInterface.sin_family      = AF_INET;
    sinInterface.sin_addr.s_addr = _ip;
	sinInterface.sin_port        = htons(_port);

	if(SOCKET_ERROR == bind(_sd, (struct sockaddr*)&sinInterface, sizeof(struct sockaddr_in))) {
#ifdef _WIN32
		closesocket(_sd);
#else
		close(_sd);
#endif
		_sd = INVALID_SOCKET;
		throw std::runtime_error("Can't bind socket!");
	}

	if(SOCKET_ERROR == listen(_sd, SOMAXCONN)) {
#ifdef _WIN32
		closesocket(_sd);
#else
		close(_sd);
#endif
		_sd = INVALID_SOCKET;
		throw std::runtime_error("Can't put to listen socket!");
	}
}

Connection::~Connection()
{
	Close();
}

std::string Connection::GetStringIp(void)
{
	std::stringstream	str;
	unsigned char		*ipPtr = (unsigned char*)&_ip;
	str << (unsigned int)ipPtr[0] << "."
		<< (unsigned int)ipPtr[1] << "."
		<< (unsigned int)ipPtr[2] << "."
		<< (unsigned int)ipPtr[3];

	return str.str();
}

Connection *Connection::Accept(void) {
	struct sockaddr_in	sinRemote;
	int			nAddress = sizeof(struct sockaddr);
	SOCKET			sdAcc = INVALID_SOCKET;
	while(true) {
		sdAcc = accept(_sd, (struct sockaddr*)&sinRemote, (socklen_t*)&nAddress);
		if (INVALID_SOCKET == sdAcc) {
			if (EWOULDBLOCK == errno || EINTR == errno || EAGAIN == errno) continue;
			return NULL; // We don't set an error because this is the closing step
		} else {
			Connection *conn = new Connection();
			conn->_ip = sinRemote.sin_addr.s_addr;
			conn->_port = htons(sinRemote.sin_port);
			conn->_sd = sdAcc;
			conn->_host = conn->GetStringIp();
			return conn;
		}
	}
	return NULL;
}

// Note:
// After a single 'send' or 'recv' we should not update speed
// because after this instruction it's always 0 b/s.
int Connection::Send(const char *buffer, unsigned int size)
{
	int	sb = (_ssl.get()) ? SSL_write(_ssl->sslHandle, buffer, size) : send(_sd, buffer, size, 0);
	if (0 >= sb) {
		Close();
	} else {
		_sentData += sb;
	}
	return sb;
}

int Connection::Recv(char *buffer, unsigned int size)
{
	int	rb = (_ssl.get()) ? SSL_read(_ssl->sslHandle, buffer, size) : recv(_sd, buffer, size, 0);
	if (0 >= rb) {
		Close();
	} else {
		_recvData += rb;
	}
	return rb;
}

bool Connection::SendAll(const char *buffer, unsigned int size)
{
	if (0 == size) return true;

	unsigned int	curSb = 0;

	while(true) {
		const int	sb = (_ssl.get()) ? SSL_write(_ssl->sslHandle,  buffer+curSb, size-curSb) : send(_sd, buffer+curSb, size-curSb, 0);
		if (0 < sb) {
			_sentData += sb;
			curSb += sb;
			if (curSb == size) break;
		} else {
			Close();
			return false;
		}
	}

	return true;
}

bool Connection::RecvAll(char *buffer, unsigned int size)
{
	unsigned int	curRb = 0;

	if (0 == size) return true;
	while(true) {
		const int	rb = (_ssl.get()) ? SSL_read(_ssl->sslHandle, buffer+curRb, size-curRb) : recv(_sd, buffer+curRb, size-curRb, 0);
		if (0 < rb) {
			_recvData += rb;
			curRb += rb;
			if (curRb == size) break;
		} else {
			Close();
			return false;
		}
	}

	return true;
}

bool Connection::RecvTmOut(int msec) {
	if (msec < 0) return false;
	struct timeval	_tv;
	_tv.tv_sec = msec/1000;
	_tv.tv_usec = (msec%1000)*1000;
	if (setsockopt(_sd, SOL_SOCKET, SO_RCVTIMEO, &_tv, sizeof(struct timeval)))
		return false;
	return true;
}

void Connection::Close(void)
{
	_ssl.reset(0);

	if (INVALID_SOCKET != _sd) {
		shutdown(_sd, SHUT_RDWR);
#ifdef _WIN32
		closesocket(_sd);
#else
		close(_sd);
#endif
		_sd = INVALID_SOCKET;
	}
}

const char *Connection::GetHost(void)
{
	return _host.c_str();
}

unsigned int Connection::GetIP(void)
{
	return _ip;
}

unsigned short Connection::GetPort(void)
{
	return _port;
}

bool Connection::IsConnected(void)
{
	return _sd != INVALID_SOCKET;
}

unsigned int Connection::GetSentData(bool reset)
{
	unsigned int ret = _sentData;
	if (reset) _sentData = 0;
	return ret;
}

unsigned int Connection::GetRecvData(bool reset)
{
	unsigned int ret = _recvData;
	if (reset) _recvData = 0;
	return ret;
}

// For the host's IPs
std::vector<unsigned int>	Connection::_currIPs;
unsigned short			Connection::_currPort;

bool Connection::Init(void)
{
#ifdef _WIN32
	WSADATA	WSAd;
	if (WSAStartup(MAKEWORD(2,2), &WSAd)) return false;
#endif
	// get all ips of this machine
	_currIPs.push_back(0);			//0.0.0.0
	_currIPs.push_back(16777343);	//127.0.0.1
	char name[128];
	if (!gethostname(name, 128)) {
		struct hostent	*host = gethostbyname(name);
		char **alias = host->h_addr_list; 
		while (*alias != NULL) {
			unsigned int tmpAddr;
			memcpy(&tmpAddr, *alias, sizeof(unsigned int));
			_currIPs.push_back(tmpAddr);
			alias++; 
		}     
	}
	return true;
}

void Connection::Cleanup(void)
{
#ifdef _WIN32
	WSACleanup();
#endif
}

void Connection::AddLocalServerPort(unsigned short port)
{
	_currPort = port;
}

bool Connection::IsGoodIpAddress(unsigned int ip, unsigned short port)
{
	if (port != _currPort) return true;
	for(std::vector<unsigned int>::const_iterator it = _currIPs.begin(); it != _currIPs.end(); ++it)
		if (*it == ip) return false;
	return true;
}

unsigned int Connection::GetLocalIP(void)
{
	if (_currIPs.empty()) throw std::runtime_error("Don't have any local IP addresses!");
	return _currIPs.back();
}

unsigned short Connection::GetLocalPort(void)
{
	return _currPort;
}

void Connection::InitSSL(void)
{
	// Register the error strings for libcrypto & libssl
	SSL_load_error_strings();
	// Register the available ciphers and digests
	SSL_library_init();
}

