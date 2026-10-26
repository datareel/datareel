// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxsocket.h
// C++ Compiler Used: MSVC, GCC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/20/1999
// Date Last Modified: 10/26/2026
// Copyright (c) 2001-2026 DataReel Software Development
// ----------------------------------------------------------- // 
// ---------- Include File Description and Details  ---------- // 
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

09/25/2002: Removed default parameter from all versions of the
gxSocket::ShutDown() function due to conflicting int types.

10/24/2002: Changed the buf parameter on all versions of the 
datagram SendTo() and ReadFrom() functions from a non-constant 
void * type to a constant void * type.

10/25/2002: Split the datagram functions into three categories, 
Client/Server, Client side, and Sever side. This change was made 
to make it easier for applications to associate Internet address 
data structures with the correct client or server.

10/25/2002: Added new server side datagram functions: 
RemoteRawReadFrom(), RemoteRecvFrom(), RemoteRawWriteTo(), and
RemoteSendTo().

10/25/2002: Added the gxSocket::Clear() helper function used to 
reset all gxSocket variables during object construction.

10/25/2002: Added gxSocket::InitSIN() helper function used to initialize 
socket Internet address data structures. 

10/25/2002: Added the following function used to construct and initialize 
gxSocket objects using socket Internet address data structures:
gxSocket(gxsSocket_t st, gxsSocketAddress *host_sin)
gxSocket(gxsSocket_t st, gxsProtocolFamily pf, gxsSocketAddress *host_sin);
gxsSocket_t InitSocket(gxsSocket_t st, gxsSocketAddress *host_sin);
gxsSocket_t InitSocket(gxsSocket_t st, gxsProtocolFamily pf, 
                       gxsSocketAddress *host_sin);

09/19/2005: Added the int gxSocket::Recv(gxsSocket_t s, void *buf, 
int bytes, int useTimeout, int seconds, int useconds, int flags);
function. 

10/26/2026: DataReel 7 rewrites gxSocket. Addresses are resolved 
with getaddrinfo() and stored in gxsAddress (sockaddr_storage), 
so IPv4 and IPv6 both work. Connect() takes a host name and port 
and tries every address the name resolves to. Listen() binds a 
dual-stack IPv6 socket that also accepts IPv4 clients. Accept() 
returns the client connection as a new gxSocket instead of 
keeping it in remote_socket. Host names and addresses are 
std::string. Windows builds use Winsock 2.
==============================================================
*/
// ----------------------------------------------------------- // 
#ifndef __GX_SOCKET_HPP__
#define __GX_SOCKET_HPP__

#include "gxdlcode.h"

#include <string>
#include <vector>

#include "gxstypes.h"

// Socket address that holds an IPv4 or IPv6 address and port
class GXDLCODE_API gxsAddress
{
public:
  gxsAddress() { Clear(); }
  gxsAddress(const sockaddr *sa, gxsSocketLength_t sa_len);

public:
  void Clear();
  bool IsSet() const { return length > 0; }
  int Family() const { return ((const sockaddr *)&storage)->sa_family; }
  int Port() const;
  void SetPort(int port);
  std::string IPAddress() const; // Numeric address, such as 192.0.2.1 or ::1
  std::string ToString() const;  // 192.0.2.1:80 or [2001:db8::1]:80
  sockaddr *SockAddr() { return (sockaddr *)&storage; }
  const sockaddr *SockAddr() const { return (const sockaddr *)&storage; }
  gxsSocketLength_t Length() const { return length; }
  void SetLength(gxsSocketLength_t len) { length = len; }
  gxsSocketLength_t Capacity() const { return sizeof(storage); }

  // Resolve a host name or numeric address. An empty host with
  // passive true gives the wildcard address for binding. family is
  // AF_UNSPEC, AF_INET or AF_INET6; socket_type is SOCK_STREAM or
  // SOCK_DGRAM. Returns 0 if successful or the getaddrinfo() error.
  static int Resolve(const std::string &host, int port, 
		     std::vector<gxsAddress> &addresses,
		     int socket_type = SOCK_STREAM, 
		     int family = AF_UNSPEC, bool passive = false);

private:
  sockaddr_storage storage;
  gxsSocketLength_t length;
};

class GXDLCODE_API gxSocket
{
public:
  gxSocket();
  explicit gxSocket(gxsSocket_t s); // Take ownership of an open socket
  virtual ~gxSocket();

  gxSocket(gxSocket &&ob) noexcept;
  gxSocket &operator=(gxSocket &&ob) noexcept;
  gxSocket(const gxSocket &) = delete;
  gxSocket &operator=(const gxSocket &) = delete;

public: // TCP client and server functions
  // Connect to host:port, trying each address the name resolves to.
  // timeout_sec limits each connection attempt; 0 waits for the
  // operating system timeout. Returns 0 if successful.
  int Connect(const std::string &host, int port, int timeout_sec = 30,
	      int family = AF_UNSPEC);

  // Bind and listen on port. An empty bind_address listens on every
  // interface, IPv6 and IPv4. Returns 0 if successful.
  int Listen(int port, const std::string &bind_address = std::string(),
	     int max_connections = SOMAXCONN, int family = AF_UNSPEC);

  // Wait for a client and return its connection. The returned socket
  // is not valid (IsOpen() is false) if an error occurred.
  gxSocket Accept();

public: // UDP functions
  // Open a datagram socket. A port above 0 binds it for receiving.
  int OpenDatagram(int family = AF_INET6, int port = 0,
		   const std::string &bind_address = std::string());
  int SendTo(const gxsAddress &to, const void *buf, int bytes);
  int RecvFrom(gxsAddress &from, void *buf, int bytes, 
	       int timeout_sec = -1);

public: // Data transfer functions
  // Send every byte. Returns bytes sent or -1 on error.
  int Send(const void *buf, int bytes);
  int Send(const std::string &s) { return Send(s.data(), (int)s.size()); }

  // Receive up to bytes, waiting at most timeout_sec seconds (-1 waits
  // forever). Returns the number of bytes received, 0 if the peer
  // closed the connection, or -1 on an error or timeout.
  int RecvSome(void *buf, int bytes, int timeout_sec = -1);

  // Receive exactly bytes unless the connection closes, an error 
  // occurs or the timeout passes. Returns the number received.
  int Recv(void *buf, int bytes, int timeout_sec = -1);

  // Single send() and recv() calls with no retry
  int RawWrite(const void *buf, int bytes);
  int RawRead(void *buf, int bytes);

  // Wait until the socket is readable. Returns 1 if readable, 0 on a
  // timeout and -1 on an error. A negative timeout waits forever.
  int WaitReadable(int timeout_ms);
  static int WaitReadable(gxsSocket_t s, int timeout_ms);
  int ReadSelect(int seconds, int useconds) {
    return WaitReadable(seconds * 1000 + useconds / 1000);
  }

public: // Socket control functions
  bool IsOpen() const { return gxsocket != gxsINVALID_SOCKET; }
  bool IsConnected() const { return is_connected; }
  gxsSocket_t GetSocket() const { return gxsocket; }
  gxsSocket_t Release(); // Give up ownership without closing
  void Close();
  void ShutDown(int how);
  int SetSockOpt(int level, int opt_name, const void *opt_val, 
		 gxsSocketLength_t opt_len);
  int GetSockOpt(int level, int opt_name, void *opt_val, 
		 gxsSocketLength_t *opt_len);
  int SetNoDelay(bool on = true);
  int SetKeepAlive(bool on = true);

public: // Address information
  const gxsAddress &PeerAddress() const { return peer; }
  gxsAddress LocalAddress() const;
  std::string PeerIPAddress() const { return peer.IPAddress(); }
  int PeerPort() const { return peer.Port(); }
  int LocalPort() const { return LocalAddress().Port(); }
  static std::string HostName();

public: // Exception handling functions
  gxSocketError GetSocketError() const { return socket_error; }
  gxSocketError SetSocketError(gxSocketError err) {
    return socket_error = err;
  }
  gxSocketError ResetSocketError() { return socket_error = gxSOCKET_NO_ERROR; }
  gxSocketError ResetError() { return socket_error = gxSOCKET_NO_ERROR; }
  const char *SocketExceptionMessage() const;
  static const char *ExceptionMessage(gxSocketError err);
  int SystemError() const { return system_error; } // errno or WSA code

  // Platform specific initialization. Winsock is started on first use
  // and these are no-ops on other platforms.
  static int InitSocketLibrary();
  static void ReleaseSocketLibrary();

public:
  int operator!() const { return socket_error != gxSOCKET_NO_ERROR; }

protected:
  gxSocketError set_error(gxSocketError err);
  void close_socket(gxsSocket_t &s);

protected:
  gxsSocket_t gxsocket;       // Socket this object owns
  gxsAddress peer;            // Remote address of a connected socket
  gxSocketError socket_error; // The last reported socket error
  int system_error;           // errno or WSAGetLastError() for that error
  bool is_connected;
};

#endif // __GX_SOCKET_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
