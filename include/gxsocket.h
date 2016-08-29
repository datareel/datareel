// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File Name: gxsocket.h
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/20/1999
// Date Last Modified: 08/28/2016
// Copyright (c) 2001-2016 DataReel Software Development
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
==============================================================
*/
// ----------------------------------------------------------- //   
#ifndef __GX_SOCKET_HPP__
#define __GX_SOCKET_HPP__

#include "gxdlcode.h"

#include "gxstypes.h"

// gxSocket class 
class GXDLCODE_API gxSocket
{
public:
  gxSocket(); 
  gxSocket(gxsSocket_t st, gxsPort_t port, char *hostname = 0); 
  gxSocket(gxsAddressFamily af, gxsSocket_t st, gxsProtocolFamily pf,
	   int port, char *hostname = 0);
  gxSocket(gxsSocket_t st, gxsSocketAddress *host_sin);
  gxSocket(gxsSocket_t st, gxsProtocolFamily pf,
	   gxsSocketAddress *host_sin);
  virtual ~gxSocket();
  
private: // Disallow copying and assignment
  gxSocket(const gxSocket &ob) { }
  void operator=(const gxSocket &ob) { }
  
public: // Functions used to set the socket parameters
  void SetAddressFamily(gxsAddressFamily af) { address_family = af; }
  void SetProtocolFamily(gxsProtocolFamily pf) { protocol_family = pf; }
  gxsProtocolFamily GetProtocolFamily() { return protocol_family; }
  void SetSocketType(gxsSocket_t st) { socket_type = st; }
  gxsSocket_t GetSocketType() { return socket_type; }
  void SetPortNumber(gxsPort_t p) { port_number = p; }
  gxsSocket_t GetBoundSocket() { return gxsocket; } 
  gxsSocket_t GetSocket() { return gxsocket; }  
  gxsSocket_t GetRemoteSocket() { return remote_socket; }
  void SetSocket(gxsSocket_t s) { gxsocket = s; }
  void SetRemoteSocket(gxsSocket_t s) { remote_socket = s; }

public: // Socket functions
  gxsSocket_t Socket();
  gxsSocket_t InitSocket(gxsSocket_t st, int port, char *hostname = 0);
  gxsSocket_t InitSocket(gxsAddressFamily af, gxsSocket_t st, 
			 gxsProtocolFamily pf, int port, char *hostname = 0);
  gxsSocket_t InitSocket(gxsSocket_t st, gxsSocketAddress *host_sin);
  gxsSocket_t InitSocket(gxsSocket_t st, gxsProtocolFamily pf,
			 gxsSocketAddress *host_sin);
  void Close();
  void Close(gxsSocket_t &s);
  void CloseSocket();
  void CloseRemoteSocket();
  gxsSocket_t Accept();
  int Bind();
  int Connect();
  int Listen(int max_connections = SOMAXCONN);
  gxsSocket_t Accept(gxsSocket_t s, gxsSocketAddress *sa);
  int Bind(gxsSocket_t s, gxsSocketAddress *sa);
  int Connect(gxsSocket_t s, gxsSocketAddress *sa);
  int Listen(gxsSocket_t s, int max_connections);
  int Recv(void *buf, int bytes, int flags = 0);
  int Recv(gxsSocket_t s, void *buf, int bytes, int flags = 0);
  int Recv(void *buf, int bytes, int seconds, int useconds, int flags = 0);
  int Recv(gxsSocket_t s, void *buf, int bytes, int seconds, int useconds, 
	   int flags = 0);
  int Recv(gxsSocket_t s, void *buf, int bytes, int useTimeout, int seconds,
	   int useconds, int flags);
  int Send(const void *buf, int bytes, int flags = 0);
  int Send(gxsSocket_t s, const void *buf, int bytes, int flags = 0);
  int RemoteRecv(void *buf, int bytes, int seconds, int useconds, 
		 int flags = 0);
  int RemoteRecv(void *buf, int bytes, int flags = 0);
  int RemoteSend(const void *buf, int bytes, int flags = 0);
  int RawRead(void *buf, int bytes, int flags = 0);
  int RawRead(gxsSocket_t s, void *buf, int bytes, int flags = 0);
  int RawRemoteRead(void *buf, int bytes, int flags = 0);
  int RawWrite(const void *buf, int bytes, int flags = 0);
  int RawWrite(gxsSocket_t s, const void *buf, int bytes, int flags = 0);
  int RawRemoteWrite(const void *buf, int bytes, int flags = 0);
  void ResetRead() { bytes_read = 0; }
  void ResetWrite() { bytes_moved = 0; } 
  void ShutDown(int how);
  void ShutDown(gxsSocket_t &s, int how);
  void ShutDownSocket(int how);
  void ShutDownRemoteSocket(int how);
  int GetPeerName(gxsSocket_t s, gxsSocketAddress *sa);
  int GetPeerName();
  int GetSockName(gxsSocket_t s, gxsSocketAddress *sa);
  int GetSockName();
  int GetSockOpt(gxsSocket_t s, int level, int optName, 
		 void *optVal, unsigned *optLen);
  int GetSockOpt(int level, int optName, void *optVal, unsigned *optLen);
  int SetSockOpt(gxsSocket_t s, int level, int optName, 
		 const void *optVal, unsigned optLen);
  int SetSockOpt(int level, int optName, const void *optVal, unsigned optLen);
  int GetServByName(char *name, char *protocol = 0);
  int GetServByPort(int port, char *protocol = 0); 
  gxsServent *GetServiceInformation(char *name, char *protocol = 0);
  gxsServent *GetServiceInformation(int port, char *protocol = 0);
  int GetPortNumber(); 
  int GetRemotePortNumber();
  int GetHostName(char *sbuf);
  int GetIPAddress(char *sbuf);
  int GetDomainName(char *sbuf);
  int GetBoundIPAddress(char *sbuf);
  int GetRemoteHostName(char *sbuf);
  gxsHostNameInfo *GetHostInformation(char *hostname);
  void GetClientInfo(char *client_name, int &r_port);
  gxsAddressFamily GetAddressFamily();
  gxsAddressFamily GetRemoteAddressFamily();
  
  // Process control functions
  int ReadSelect(gxsSocket_t s, int seconds, int useconds); 
  int BytesRead() { return bytes_read; }
  int BytesMoved() { return bytes_moved; }
  int SetBytesRead(int bytes = 0) { return bytes_read = bytes; }
  int SetBytesMoved(int bytes = 0) { return bytes_moved = bytes; }
  int *GetBytesRead() { return &bytes_read; }
  int *GetBytesMoved() { return &bytes_moved; }
  int IsConnected() { return is_connected == 1; }
  int IsBound() { return is_bound == 1; }
  void ReleaseSocket() { gxsocket = (gxsSocket_t)-1; }
  void ReleaseRemoteSocket() { remote_socket = (gxsSocket_t)-1; }

  // Client/Server Datagram functions
  int RawReadFrom(gxsSocket_t s, gxsSocketAddress *sa, void *buf,
		  int bytes, int flags = 0);
  int RecvFrom(gxsSocket_t s, gxsSocketAddress *sa, void *buf,
	       int bytes, int flags = 0);
  int RecvFrom(gxsSocket_t s, gxsSocketAddress *sa, void *buf,
	       int bytes, int seconds, int useconds, int flags = 0);
  int RawWriteTo(gxsSocket_t s, gxsSocketAddress *sa, const void *buf,
		 int bytes, int flags = 0);
  int SendTo(gxsSocket_t s, gxsSocketAddress *sa, const void *buf,
	     int bytes, int flags = 0);
  
  // Client side Datagram functions
  int RawReadFrom(void *buf, int bytes, int flags = 0);
  int RecvFrom(void *buf, int bytes, int flags = 0);
  int RecvFrom(void *buf, int bytes, int seconds, int useconds, 
	       int flags = 0);
  int RawWriteTo(const void *buf, int bytes, int flags = 0);
  int SendTo(const void *buf, int bytes, int flags = 0);

  // Server side Datagram functions
  int RemoteRawReadFrom(void *buf, int bytes, int flags = 0);
  int RemoteRecvFrom(void *buf, int bytes, int flags = 0);
  int RemoteRecvFrom(void *buf, int bytes, int seconds, int useconds, 
		     int flags = 0);
  int RemoteRawWriteTo(const void *buf, int bytes, int flags = 0);
  int RemoteSendTo(const void *buf, int bytes, int flags = 0);

  // Exception handling functions
  gxSocketError GetSocketError() { return socket_error; }
  gxSocketError GetSocketError() const { return socket_error; }
  gxSocketError SetSocketError(gxSocketError err) {
    return socket_error = err;
  }
  gxSocketError ResetSocketError() {
    return socket_error = gxSOCKET_NO_ERROR;
  }
  gxSocketError ResetError() {
    return socket_error = gxSOCKET_NO_ERROR;
  }
  const char *SocketExceptionMessage();
  const char *TestExceptionMessage(int err);

  // Helper functions
  int InitSIN(gxsSocketAddress *host_sin, gxsAddressFamily af, 
	      int port, char *hostname);
  void Clear();

  // Platform specific initialization functions
  int InitSocketLibrary();
  int ReleaseSocketLibrary();

  // 07/25/2006: Helper functions to set remote port number and IP address 
  void SetRemotePortNumber(gxsPort_t p);
  void SetRemoteIPAddress(char *IPAddress);

public: // Overloaded operators
  int operator!() const { return socket_error != gxSOCKET_NO_ERROR; }
  int operator!() { return socket_error != gxSOCKET_NO_ERROR; }
  operator const int () const { return socket_error == gxSOCKET_NO_ERROR; }
  operator int () { return socket_error == gxSOCKET_NO_ERROR; }

protected: // Socket variables 
  gxsAddressFamily address_family;   // Object's address family
  gxsProtocolFamily protocol_family; // Object's protocol family
  gxsSocket_t socket_type;           // Object's socket type
  gxsPort_t port_number;             // Object's port number
  gxsSocket_t gxsocket;              // Socket this object is bound to
  gxsSocket_t remote_socket;         // Socket used for remote connections
  gxSocketError socket_error;        // The last reported socket error

protected: // Process control variables
  int bytes_read;   // Number of bytes read following a read operation
  int bytes_moved;  // Number of bytes written following a write operation
  int is_connected; // True if the socket is connected
  int is_bound;     // True if the socket is bound
  
public: // Data structures used to set the internet domain and addresses
  gxsSocketAddress sin;        // Sock Internet address
  gxsSocketAddress remote_sin; // Remote socket Internet address

public:  // Platform specific library extensions
  gxsSocketLibraryVersion socket_version; 

#if defined (__WIN32__)
  WSADATA socket_data; 
#endif
};

#endif // __GX_SOCKET_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
