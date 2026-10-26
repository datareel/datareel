// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxsocket.cpp
// C++ Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/20/1999
// Date Last Modified: 10/26/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- // 
// ------------- Program Description and Details ------------- // 
// ----------------------------------------------------------- // 
/*
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.
 
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.
 
You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  
USA

The gxSocket class is an object-oriented Winsock/BSD wrapper 
used to create TCP/IP sockets on WIN32 and UNIX platforms. The 
gxSocket class supports stream sockets and datagram sockets and 
includes several low-level functions needed by derived classes to 
establish communication end-points and transfer data.

Changes:
==============================================================
01/29/2002: Added four new functions to accept, bind,
connect, and listen to specified sockets:
gxsSocket_t Accept(gxsSocket_t s, gxsSocketAddress *sa);
int Bind(gxsSocket_t s, gxsSocketAddress *sa);
int Connect(gxsSocket_t s, gxsSocketAddress *sa);
int Listen(gxsSocket_t s, int max_connections);

04/15/2002: Added the gxSOCKET_FILELENGTH_ERROR and
gxSOCKET_FILEMODTIME_ERROR error messages to the 
gxsSocketExceptionMessages array.

06/10/2002: Added embedded protocol error codes to the 
gxsSocketExceptionMessages array.

09/25/2002: Removed unneeded (int) type casts from all versions
of the gxSocket send and recv functions.

10/24/2002: All gxSocket constructors have been modified to 
zero-out the sin and remote_sin data structures prior to using 
them. This change was implemented to eliminate the possibility 
of any erroneous data being set in the memory space of the data 
structures when a gxSocket object is constructed.

10/25/2002: All gxSocket constructors now use the gxSocket::Clear()
function to reset all the gxSocket() variables.

10/25/2002: Removed redundant code from the 
gxSocket::InitSocket(gxsSocket_t st, gxsPort_t port, char *hostname) 
function.

10/24/2002: All gxSocket constructors have been modified to 
zero-out the socket_data data structure prior to use. This 
change was implemented to eliminate the possibility of any 
erroneous data being set in the memory space of the data 
structure when a gxSocket object is constructed.

01/10/2003: The gxSocket::InitSIN() function was not assigning
the specified port number before setting the host_sin port
number.

11/17/2003: Added gxSOCKET_DISCONNECTED_ERROR string to the 
gxsSocketExceptionMessages array.

11/17/2003: Modified all versions of the gxSocket::Recv(), 
gxSocket::RawRead(), gxsocket::RecvFrom(),gxSocket::RawReadFrom() 
functions to signal that the socket has been gracefully closed 
if an recv() or recvfrom() call returns zero bytes.

09/19/2005: Added new Solaris code to the ReadSelect() function. 
Still using old version under Solaris until new version is tested.  

12/19/2005: Added new error codes to the gxsSocketExceptionMessages
array.

10/26/2026: DataReel 7 rewrites gxSocket for IPv4 and IPv6 with 
getaddrinfo(), connect timeouts, poll() based receive timeouts 
and Winsock 2. Sends on Linux use MSG_NOSIGNAL so a closed peer 
returns an error instead of raising SIGPIPE.
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <cerrno>
#include <cstring>
#include <mutex>
#include "gxsocket.h"

#if defined (_WIN32)
#pragma comment(lib, "ws2_32.lib")
#else
#include <fcntl.h>
#include <poll.h>
#include <netinet/tcp.h>
#endif

#if defined (MSG_NOSIGNAL)
const int gxsSEND_FLAGS = MSG_NOSIGNAL;
#else
const int gxsSEND_FLAGS = 0;
#endif

// NOTE: This array must contain the same number of exceptions as the
// gxSocketError enumeration. 
const int gxsMaxSocketExceptionMessages = 58;
const char *gxsSocketExceptionMessages[gxsMaxSocketExceptionMessages] = {
  "Socket exception: No exception reported",             // NO_ERROR
  "Socket exception: Invalid exception code",            // INVALID_ERROR_CODE 
  "Socket exception: Error accepting remote socket",     // ACCEPT_ERROR
  "Socket exception: Could not bind socket",             // BIND_ERROR
  "Socket exception: Buffer overflow error",             // BUFOVER_ERROR
  "Socket exception: Could not connect socket",          // CONNECT_ERROR
  "Socket exception: Socket has been disconnected",      // DISCONNECTED_ERROR
  "Socket exception: File length does not match expected value", // FILELENGTH
  "Socket exception: File modification time dates do not match", // FILEMODTIME
  "Socket exception: A file system error occurred",      // FILESYSTEM_ERROR
  "Socket exception: Error getting socket option",       // GETOPTION_ERROR
  "Socket exception: Could not resolve hostname",        // HOSTNAME_ERROR
  "Socket exception: Initialization error",              // INIT_ERROR
  "Socket exception: Listen error",                      // LISTEN_ERROR
  "Socket exception: Get peer name error",               // PEER_ERROR
  "Socket exception: Unsupported protocol requested",    // PROTOCOL_ERROR
  "Socket exception: Receive error",                     // RECEIVE_ERROR
  "Socket exception: Request timed out",                 // REQUEST_TIMEOUT
  "Socket exception: Unsupported service requested",     // SERVICE_ERROR
  "Socket exception: Error setting socket option",       // SETOPTION_ERROR
  "Socket exception: Get socket name error",             // SOCKNAME_ERROR
  "Socket exception: Unsupported socket type requested", // SOCKETTYPE
  "Socket exception: Transmit error",                    // TRANSMIT_ERROR

  // WIN32 exception messages (Transparent to UNIX applications)
  "WinSock exception: WSACleanup operation failed",        // WINSOCKCLEANUP
  "WinSock exception: Version specified is not supported by this DLL",
                                                           // WINSOCKEINVAL
  "WinSock exception: Could not initialize Winsock",       // WINSOCKINIT
  "WinSock exception: Network subsystem is not ready",     // WINSOCKNETWORK
  "WinSock exception: Requested version in not supported", // WINSOCKVERSION
  
  // Database block exception messages
  "Socket exception: Database block acknowledgment error",  // BLOCKACK
  "Socket exception: Bad database block header",            // BLOCKHEADER
  "Socket exception: Bad database block size",              // BLOCKSIZE
  "Socket exception: Database block synchronization error", // BLOCKSYNC

  "Embedded protocol reported an error condition", // EB_PROTO_ERROR
  "FTP protocol reported an error condition",      // FTP_ERROR
  "HTTP protocol reported an error condition",     // HTTP_ERROR
  "HTTP document status code error condition",     // HTTPDOC_ERROR
  "POP3 protocol reported an error condition",     // POP3_ERROR
  "SMTP protocol reported an error condition",     // SMTP_ERROR
  "Telent protocol reported an error condition",   // TELNET_ERROR
  "Crypto error",                                  // CRYPTO_ERROR
  "DNS protocol reported error condition",         // DNS_ERROR
  "HTTPS protocol reported error condition",       // HTTPS_ERROR
  "IMAP protocol reported an error condition ",    // IMAP_ERROR
  "LDAP protocol reported error condition",        // LDAP_ERROR
  "Line printer reported error condition",         // PRINTER_ERROR
  "NFS protocol reported error condition",         // NFS_ERROR
  "NIS protocol reported error condition",         // NIS_ERROR
  "NTP protocol reported error condition",         // NTP_ERROR
  "RPC protocol reported error condition ",        // RPC_ERROR
  "Rsync protocol reported error condition ",      // RSYNC_ERROR
  "SFTP protocol reported an error condition",     // SFTP_ERROR
  "SCP protocol reported an error condition",      // SCP_ERROR
  "SSH protocol reported an error condition",      // SSH_ERROR
  "SNMP protocol reported an error condition",     // SNMP_ERROR
  "SSL protocol reported an error condition",      // SSL_ERROR
  "Syslog protocol reported an error condition",   // SYSLOG_ERROR
  "TFTP protocol reported an error condition",     // TFTP_ERROR 

  // Exception code used to allow application to set error string
  "Socket application reported an error", // APPDEF_ERROR 
};

static int last_system_error()
{
#if defined (_WIN32)
  return WSAGetLastError();
#else
  return errno;
#endif
}

// --------------------------------------------------------------
// gxsAddress
// --------------------------------------------------------------
gxsAddress::gxsAddress(const sockaddr *sa, gxsSocketLength_t sa_len)
{
  Clear();
  if(sa && sa_len > 0 && (std::size_t)sa_len <= sizeof(storage)) {
    memcpy(&storage, sa, (std::size_t)sa_len);
    length = sa_len;
  }
}

void gxsAddress::Clear()
{
  memset(&storage, 0, sizeof(storage));
  length = 0;
}

int gxsAddress::Port() const
{
  if(Family() == AF_INET) {
    return ntohs(((const sockaddr_in *)&storage)->sin_port);
  }
  if(Family() == AF_INET6) {
    return ntohs(((const sockaddr_in6 *)&storage)->sin6_port);
  }
  return 0;
}

void gxsAddress::SetPort(int port)
{
  if(Family() == AF_INET) {
    ((sockaddr_in *)&storage)->sin_port = htons((unsigned short)port);
  }
  else if(Family() == AF_INET6) {
    ((sockaddr_in6 *)&storage)->sin6_port = htons((unsigned short)port);
  }
}

std::string gxsAddress::IPAddress() const
{
  if(length == 0) return std::string();
  char host[NI_MAXHOST];
  if(getnameinfo(SockAddr(), length, host, sizeof(host), 0, 0, 
		 NI_NUMERICHOST) != 0) {
    return std::string();
  }
  return host;
}

std::string gxsAddress::ToString() const
{
  std::string ip = IPAddress();
  if(ip.empty()) return ip;
  if(Family() == AF_INET6) ip = "[" + ip + "]";
  return ip + ":" + std::to_string(Port());
}

int gxsAddress::Resolve(const std::string &host, int port, 
			std::vector<gxsAddress> &addresses,
			int socket_type, int family, bool passive)
{
  addresses.clear();
  gxSocket::InitSocketLibrary();
  addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = family;
  hints.ai_socktype = socket_type;
  hints.ai_flags = AI_NUMERICSERV;
  if(passive) hints.ai_flags |= AI_PASSIVE;
#if defined (AI_ADDRCONFIG)
  if(!passive) hints.ai_flags |= AI_ADDRCONFIG;
#endif
  std::string service = std::to_string(port);
  addrinfo *res = 0;
  int rv = getaddrinfo(host.empty() ? 0 : host.c_str(), service.c_str(), 
		       &hints, &res);
  if(rv != 0) return rv;
  for(addrinfo *ai = res; ai; ai = ai->ai_next) {
    addresses.emplace_back(ai->ai_addr, (gxsSocketLength_t)ai->ai_addrlen);
  }
  freeaddrinfo(res);
  return addresses.empty() ? EAI_NONAME : 0;
}

// --------------------------------------------------------------
// gxSocket
// --------------------------------------------------------------
gxSocket::gxSocket() : gxsocket(gxsINVALID_SOCKET), 
  socket_error(gxSOCKET_NO_ERROR), system_error(0), is_connected(false)
{
  InitSocketLibrary();
}

gxSocket::gxSocket(gxsSocket_t s) : gxsocket(s), 
  socket_error(gxSOCKET_NO_ERROR), system_error(0), 
  is_connected(s != gxsINVALID_SOCKET)
{
  InitSocketLibrary();
}

gxSocket::~gxSocket()
{
  Close();
}

gxSocket::gxSocket(gxSocket &&ob) noexcept : gxsocket(ob.gxsocket), 
  peer(ob.peer), socket_error(ob.socket_error), 
  system_error(ob.system_error), is_connected(ob.is_connected)
{
  ob.gxsocket = gxsINVALID_SOCKET;
  ob.is_connected = false;
}

gxSocket &gxSocket::operator=(gxSocket &&ob) noexcept
{
  if(this != &ob) {
    Close();
    gxsocket = ob.gxsocket;
    peer = ob.peer;
    socket_error = ob.socket_error;
    system_error = ob.system_error;
    is_connected = ob.is_connected;
    ob.gxsocket = gxsINVALID_SOCKET;
    ob.is_connected = false;
  }
  return *this;
}

int gxSocket::InitSocketLibrary()
{
#if defined (_WIN32)
  static std::once_flag once;
  static int rv = 0;
  std::call_once(once, [] {
    WSADATA data;
    rv = WSAStartup(MAKEWORD(2, 2), &data);
  });
  return rv;
#else
  return 0;
#endif
}

void gxSocket::ReleaseSocketLibrary()
{
  // Winsock is released when the process exits
}

gxSocketError gxSocket::set_error(gxSocketError err)
{
  system_error = (err == gxSOCKET_NO_ERROR) ? 0 : last_system_error();
  return socket_error = err;
}

void gxSocket::close_socket(gxsSocket_t &s)
{
  if(s == gxsINVALID_SOCKET) return;
#if defined (_WIN32)
  closesocket(s);
#else
  ::close(s);
#endif
  s = gxsINVALID_SOCKET;
}

void gxSocket::Close()
{
  close_socket(gxsocket);
  is_connected = false;
}

gxsSocket_t gxSocket::Release()
{
  gxsSocket_t s = gxsocket;
  gxsocket = gxsINVALID_SOCKET;
  is_connected = false;
  return s;
}

void gxSocket::ShutDown(int how)
{
  if(IsOpen()) shutdown(gxsocket, how);
}

static void set_blocking(gxsSocket_t s, bool blocking)
{
#if defined (_WIN32)
  u_long mode = blocking ? 0 : 1;
  ioctlsocket(s, FIONBIO, &mode);
#else
  int flags = fcntl(s, F_GETFL, 0);
  if(flags < 0) return;
  fcntl(s, F_SETFL, blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK));
#endif
}

static int wait_for_socket(gxsSocket_t s, bool for_write, int timeout_ms)
// Returns 1 when ready, 0 on a timeout and -1 on an error
{
#if defined (_WIN32)
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(s, &fds);
  timeval tv, *ptv = 0;
  if(timeout_ms >= 0) {
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    ptv = &tv;
  }
  int rv = for_write ? select(0, 0, &fds, &fds, ptv) : 
    select(0, &fds, 0, 0, ptv);
  return rv < 0 ? -1 : (rv > 0 ? 1 : 0);
#else
  pollfd pfd;
  pfd.fd = s;
  pfd.events = for_write ? POLLOUT : POLLIN;
  pfd.revents = 0;
  int rv;
  do {
    rv = poll(&pfd, 1, timeout_ms);
  } while(rv < 0 && errno == EINTR);
  return rv < 0 ? -1 : (rv > 0 ? 1 : 0);
#endif
}

int gxSocket::Connect(const std::string &host, int port, int timeout_sec,
		      int family)
{
  Close();
  std::vector<gxsAddress> addresses;
  if(gxsAddress::Resolve(host, port, addresses, SOCK_STREAM, family) != 0) {
    return set_error(gxSOCKET_HOSTNAME_ERROR);
  }

  for(const gxsAddress &addr : addresses) {
    gxsSocket_t s = socket(addr.Family(), SOCK_STREAM, IPPROTO_TCP);
    if(s == gxsINVALID_SOCKET) {
      set_error(gxSOCKET_INIT_ERROR);
      continue;
    }
    int rv;
    if(timeout_sec > 0) {
      set_blocking(s, false);
      rv = connect(s, addr.SockAddr(), addr.Length());
      if(rv != 0) {
	int err = last_system_error();
#if defined (_WIN32)
	bool in_progress = (err == WSAEWOULDBLOCK);
#else
	bool in_progress = (err == EINPROGRESS);
#endif
	if(in_progress && wait_for_socket(s, true, timeout_sec * 1000) == 1) {
	  int so_error = 0;
	  gxsSocketLength_t len = sizeof(so_error);
	  getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&so_error, &len);
	  rv = so_error == 0 ? 0 : -1;
	}
	else {
	  rv = -1;
	}
      }
      set_blocking(s, true);
    }
    else {
      rv = connect(s, addr.SockAddr(), addr.Length());
    }
    if(rv == 0) {
      gxsocket = s;
      peer = addr;
      is_connected = true;
      return set_error(gxSOCKET_NO_ERROR);
    }
    set_error(gxSOCKET_CONNECT_ERROR);
    close_socket(s);
  }
  return socket_error;
}

int gxSocket::Listen(int port, const std::string &bind_address,
		     int max_connections, int family)
{
  Close();
  std::vector<gxsAddress> addresses;
  if(bind_address.empty() && family == AF_UNSPEC) {
    // Prefer one dual-stack IPv6 socket, then fall back to IPv4
    gxsAddress::Resolve("::", port, addresses, SOCK_STREAM, AF_INET6, true);
    std::vector<gxsAddress> v4;
    gxsAddress::Resolve("0.0.0.0", port, v4, SOCK_STREAM, AF_INET, true);
    addresses.insert(addresses.end(), v4.begin(), v4.end());
  }
  else if(gxsAddress::Resolve(bind_address, port, addresses, SOCK_STREAM, 
			      family, true) != 0) {
    return set_error(gxSOCKET_HOSTNAME_ERROR);
  }

  for(const gxsAddress &addr : addresses) {
    gxsSocket_t s = socket(addr.Family(), SOCK_STREAM, IPPROTO_TCP);
    if(s == gxsINVALID_SOCKET) {
      set_error(gxSOCKET_INIT_ERROR);
      continue;
    }
#if !defined (_WIN32)
    int on = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof(on));
#endif
    if(addr.Family() == AF_INET6) {
      int off = 0; // Accept IPv4 clients on the IPv6 socket as well
      setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, (const char *)&off, 
		 sizeof(off));
    }
    if(bind(s, addr.SockAddr(), addr.Length()) != 0) {
      set_error(gxSOCKET_BIND_ERROR);
      close_socket(s);
      continue;
    }
    if(listen(s, max_connections) != 0) {
      set_error(gxSOCKET_LISTEN_ERROR);
      close_socket(s);
      continue;
    }
    gxsocket = s;
    return set_error(gxSOCKET_NO_ERROR);
  }
  return socket_error;
}

gxSocket gxSocket::Accept()
{
  gxSocket client;
  if(!IsOpen()) {
    set_error(gxSOCKET_ACCEPT_ERROR);
    return client;
  }
  sockaddr_storage ss;
  gxsSocketLength_t len = sizeof(ss);
  gxsSocket_t s;
  do {
    s = accept(gxsocket, (sockaddr *)&ss, &len);
#if defined (_WIN32)
  } while(false);
#else
  } while(s == gxsINVALID_SOCKET && errno == EINTR);
#endif
  if(s == gxsINVALID_SOCKET) {
    set_error(gxSOCKET_ACCEPT_ERROR);
    return client;
  }
  set_error(gxSOCKET_NO_ERROR);
  client.gxsocket = s;
  client.peer = gxsAddress((sockaddr *)&ss, len);
  client.is_connected = true;
  return client;
}

int gxSocket::OpenDatagram(int family, int port, 
			   const std::string &bind_address)
{
  Close();
  gxsSocket_t s = socket(family, SOCK_DGRAM, IPPROTO_UDP);
  if(s == gxsINVALID_SOCKET) return set_error(gxSOCKET_INIT_ERROR);
  if(family == AF_INET6) {
    int off = 0;
    setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, (const char *)&off, sizeof(off));
  }
  if(port > 0) {
    std::vector<gxsAddress> addresses;
    std::string host = bind_address;
    if(host.empty()) host = (family == AF_INET6) ? "::" : "0.0.0.0";
    if(gxsAddress::Resolve(host, port, addresses, SOCK_DGRAM, family, true) 
       != 0) {
      close_socket(s);
      return set_error(gxSOCKET_HOSTNAME_ERROR);
    }
    if(bind(s, addresses[0].SockAddr(), addresses[0].Length()) != 0) {
      set_error(gxSOCKET_BIND_ERROR);
      close_socket(s);
      return socket_error;
    }
  }
  gxsocket = s;
  return set_error(gxSOCKET_NO_ERROR);
}

int gxSocket::SendTo(const gxsAddress &to, const void *buf, int bytes)
{
  if(!IsOpen()) { set_error(gxSOCKET_TRANSMIT_ERROR); return -1; }
  int rv = (int)sendto(gxsocket, (const char *)buf, bytes, gxsSEND_FLAGS,
		       to.SockAddr(), to.Length());
  if(rv < 0) { set_error(gxSOCKET_TRANSMIT_ERROR); return -1; }
  set_error(gxSOCKET_NO_ERROR);
  return rv;
}

int gxSocket::RecvFrom(gxsAddress &from, void *buf, int bytes, 
		       int timeout_sec)
{
  if(!IsOpen()) { set_error(gxSOCKET_RECEIVE_ERROR); return -1; }
  if(timeout_sec >= 0) {
    int w = WaitReadable(timeout_sec * 1000);
    if(w == 0) { set_error(gxSOCKET_REQUEST_TIMEOUT); return -1; }
    if(w < 0) { set_error(gxSOCKET_RECEIVE_ERROR); return -1; }
  }
  sockaddr_storage ss;
  gxsSocketLength_t len = sizeof(ss);
  int rv = (int)recvfrom(gxsocket, (char *)buf, bytes, 0, 
			 (sockaddr *)&ss, &len);
  if(rv < 0) { set_error(gxSOCKET_RECEIVE_ERROR); return -1; }
  from = gxsAddress((sockaddr *)&ss, len);
  set_error(gxSOCKET_NO_ERROR);
  return rv;
}

int gxSocket::WaitReadable(int timeout_ms)
{
  if(!IsOpen()) return -1;
  return wait_for_socket(gxsocket, false, timeout_ms);
}

int gxSocket::WaitReadable(gxsSocket_t s, int timeout_ms)
{
  if(s == gxsINVALID_SOCKET) return -1;
  return wait_for_socket(s, false, timeout_ms);
}

int gxSocket::RawWrite(const void *buf, int bytes)
{
  if(!IsOpen()) { set_error(gxSOCKET_TRANSMIT_ERROR); return -1; }
  int rv = (int)send(gxsocket, (const char *)buf, bytes, gxsSEND_FLAGS);
  if(rv < 0) { set_error(gxSOCKET_TRANSMIT_ERROR); return -1; }
  set_error(gxSOCKET_NO_ERROR);
  return rv;
}

int gxSocket::RawRead(void *buf, int bytes)
{
  if(!IsOpen()) { set_error(gxSOCKET_RECEIVE_ERROR); return -1; }
  int rv = (int)recv(gxsocket, (char *)buf, bytes, 0);
  if(rv < 0) { set_error(gxSOCKET_RECEIVE_ERROR); return -1; }
  if(rv == 0) {
    set_error(gxSOCKET_DISCONNECTED_ERROR);
    is_connected = false;
    return 0;
  }
  set_error(gxSOCKET_NO_ERROR);
  return rv;
}

int gxSocket::Send(const void *buf, int bytes)
{
  const char *p = (const char *)buf;
  int sent = 0;
  while(sent < bytes) {
    int rv = RawWrite(p + sent, bytes - sent);
    if(rv < 0) {
#if !defined (_WIN32)
      if(system_error == EINTR) continue;
#endif
      return -1;
    }
    sent += rv;
  }
  return sent;
}

int gxSocket::RecvSome(void *buf, int bytes, int timeout_sec)
{
  if(timeout_sec >= 0) {
    int w = WaitReadable(timeout_sec * 1000);
    if(w == 0) { set_error(gxSOCKET_REQUEST_TIMEOUT); return -1; }
    if(w < 0) { set_error(gxSOCKET_RECEIVE_ERROR); return -1; }
  }
  return RawRead(buf, bytes);
}

int gxSocket::Recv(void *buf, int bytes, int timeout_sec)
{
  char *p = (char *)buf;
  int got = 0;
  while(got < bytes) {
    int rv = RecvSome(p + got, bytes - got, timeout_sec);
    if(rv <= 0) break;
    got += rv;
  }
  return got;
}

int gxSocket::SetSockOpt(int level, int opt_name, const void *opt_val, 
			 gxsSocketLength_t opt_len)
{
  if(setsockopt(gxsocket, level, opt_name, (const char *)opt_val, opt_len) 
     != 0) {
    return set_error(gxSOCKET_SETOPTION_ERROR);
  }
  return set_error(gxSOCKET_NO_ERROR);
}

int gxSocket::GetSockOpt(int level, int opt_name, void *opt_val, 
			 gxsSocketLength_t *opt_len)
{
  if(getsockopt(gxsocket, level, opt_name, (char *)opt_val, opt_len) != 0) {
    return set_error(gxSOCKET_GETOPTION_ERROR);
  }
  return set_error(gxSOCKET_NO_ERROR);
}

int gxSocket::SetNoDelay(bool on)
{
  int v = on ? 1 : 0;
  return SetSockOpt(IPPROTO_TCP, TCP_NODELAY, &v, sizeof(v));
}

int gxSocket::SetKeepAlive(bool on)
{
  int v = on ? 1 : 0;
  return SetSockOpt(SOL_SOCKET, SO_KEEPALIVE, &v, sizeof(v));
}

gxsAddress gxSocket::LocalAddress() const
{
  sockaddr_storage ss;
  gxsSocketLength_t len = sizeof(ss);
  if(!IsOpen() || getsockname(gxsocket, (sockaddr *)&ss, &len) != 0) {
    return gxsAddress();
  }
  return gxsAddress((sockaddr *)&ss, len);
}

std::string gxSocket::HostName()
{
  InitSocketLibrary();
  char name[256];
  memset(name, 0, sizeof(name));
  if(gethostname(name, sizeof(name) - 1) != 0) return std::string();
  return name;
}

const char *gxSocket::ExceptionMessage(gxSocketError err)
{
  int e = (int)err;
  if(e < 0 || e > (gxsMaxSocketExceptionMessages-1)) {
    e = (int)gxSOCKET_INVALID_ERROR_CODE;
  }
  return gxsSocketExceptionMessages[e];
}

const char *gxSocket::SocketExceptionMessage() const
{
  return ExceptionMessage(socket_error);
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
