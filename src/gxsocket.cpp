// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File Name: gxsocket.cpp
// C++ Compiler Used: MSVC, BCC32, GCC, HPUX aCC, SOLARIS CC
// Produced By: DataReel Software Development Team
// File Creation Date: 09/20/1999
// Date Last Modified: 10/31/2026
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

10/31/2026: Close() sets the closed descriptors to -1, so a second
Close() or the destructor no longer closes a descriptor number the
program has opened again since. ReadSelect() uses poll() on UNIX:
select() with a descriptor at or above FD_SETSIZE wrote past the end
of the fd_set. Send() stops on a send() that returns 0 (it looped for
ever) and, on Linux, sends with MSG_NOSIGNAL so a peer that closed the
connection returns an error instead of killing the program with
SIGPIPE. Added SetIOTimeout() to limit each blocking send and receive.

10/31/2026: Host addresses are copied with gxsCopyHostAddress(), which
checks the address type and length. "*((unsigned long *)h_addr)" read
8 bytes from a 4-byte address on 64-bit UNIX. InitSIN() filled in the
object's sin instead of host_sin (thread safe Linux build), and
SetRemoteIPAddress() dereferenced a null pointer for an unknown host.
GetIPAddress(), GetDomainName() and GetHostName() no longer leak or
write past the gxsMAX_NAME_LEN buffer, and SocketExceptionMessage()
checks for negative codes.
==============================================================
*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include<string.h>
#include <fcntl.h>
#include <limits.h>
#include "gxsocket.h"

#if defined (__UNIX__)
#include <poll.h>  // 10/31/2026: ReadSelect()
#include <errno.h>
#endif

// 10/31/2026: Flag added to every send() on a stream socket so a closed
// peer gives EPIPE instead of SIGPIPE. Only Linux has MSG_NOSIGNAL.
#if defined (__LINUX__) && defined (MSG_NOSIGNAL)
const int gxsSEND_FLAGS = MSG_NOSIGNAL;
#else
const int gxsSEND_FLAGS = 0;
#endif

static int gxsCopyHostAddress(gxsInternetAddress &ia, 
			      const gxsHostNameInfo *hostnm)
// 10/31/2026: Copy the first IPv4 address of a host name lookup.
// Returns -1 if there is none. The address used to be read with
// "*((unsigned long *)hostnm->h_addr)", which reads 8 bytes from the
// 4-byte address on 64-bit UNIX systems.
{
  if(!hostnm) return -1;
  if(hostnm->h_addrtype != AF_INET) return -1;
  if(hostnm->h_length != (int)sizeof(ia.s_addr)) return -1;
  if(!hostnm->h_addr_list || !hostnm->h_addr_list[0]) return -1;
  memcpy(&ia.s_addr, hostnm->h_addr_list[0], sizeof(ia.s_addr));
  return 0;
}

#if defined (__LINUX__) && defined (__REENTRANT__)
static int gxsCopyAddrInfo(gxsInternetAddress &ia, const struct addrinfo *res)
// 10/31/2026: Copy the first IPv4 address of a getaddrinfo() result.
// Returns -1 if there is none.
{
  for(; res; res = res->ai_next) {
    if(res->ai_family != AF_INET || !res->ai_addr) continue;
    if(res->ai_addrlen < (socklen_t)sizeof(struct sockaddr_in)) continue;
    ia.s_addr = ((const struct sockaddr_in *)res->ai_addr)->sin_addr.s_addr;
    return 0;
  }
  return -1;
}
#endif

#if defined(__UNIX__) && !defined(__BSD_UNIX__)  
#include <sys/utsname.h> // Used for host name information
#endif // __UNIX__

#ifdef __BCC32__
#pragma warn -8057
#pragma warn -8071
#pragma warn -8084
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

gxSocket::gxSocket()
// Socket constructor that performs no initialization other then
// setting default values for the socket data members.
{
  // Reset all the gxSocket variables
  Clear();
}

void gxSocket::Clear() 
// Function used to reset all the gxSocket variables.
// NOTE: This function should only be called prior to
// socket initialization. 
{
  address_family = AF_INET;      // Default address family
  socket_type = SOCK_STREAM;     // Default socket type
  protocol_family = IPPROTO_TCP; // Default protocol family
  port_number = gxSOCKET_DEFAULT_PORT; // Default port number
  gxsocket = -1;
  remote_socket = -1;
  bytes_read = bytes_moved = 0;
  is_connected = 0;
  is_bound = 0;
  socket_error = gxSOCKET_NO_ERROR;
  socket_version = gxSOCKET_WSAVER_ONEONE;

  // Zero-out the Internet address structures
  memset(&sin, 0, sizeof(gxsSocketAddress));
  memset(&remote_sin, 0, sizeof(gxsSocketAddress));

#if defined (__WIN32__)
  // Zero-out the WINSOCK socket data structure
  memset(&socket_data, 0, sizeof(WSADATA)); 
#endif
}

gxSocket::gxSocket(gxsAddressFamily af, gxsSocket_t st, gxsProtocolFamily pf,
		   int port, char *hostname)
// Socket constructor used to initialize the socket according to the
// address family, socket type, and protocol family. A hostname name should
// only be specified for client sockets.
{
  // Reset all the gxSocket variables
  Clear();

  // Initialize the socket. NOTE: Any errors detected during initialization
  // will be recorded in the socket_error member.
  if(InitSocketLibrary() == 0) InitSocket(af, st, pf, port, hostname);
}

gxSocket::gxSocket(gxsSocket_t st, gxsPort_t port, char *hostname) 
// Socket constructor used to initialize the socket according to the
// socket type. A hostname name should only be specified for client
// sockets.
{
  // Reset all the gxSocket variables
  Clear();

  // Initialize the socket. NOTE: Any errors detected during initialization
  // will be recorded in the socket_error member.
  if(InitSocketLibrary() == 0) InitSocket(st, port, hostname);
}

gxSocket::gxSocket(gxsSocket_t st, gxsSocketAddress *host_sin)
// Socket constructor used to initialize the socket according to the
// socket type and Internet address. 
{
  // Reset all the gxSocket variables
  Clear();

  // Initialize the socket. NOTE: Any errors detected during initialization
  // will be recorded in the socket_error member.
  if(InitSocketLibrary() == 0) InitSocket(st, host_sin);
}

gxSocket::gxSocket(gxsSocket_t st, gxsProtocolFamily pf,
		   gxsSocketAddress *host_sin) 
// Socket constructor used to initialize the socket according to the
// socket type, protocol family, and Internet address.
{
  // Reset all the gxSocket variables
  Clear();

  // Initialize the socket. NOTE: Any errors detected during initialization
  // will be recorded in the socket_error member.
  if(InitSocketLibrary() == 0) InitSocket(st, pf, host_sin);
}

gxSocket::~gxSocket()
{
  // PC-lint 09/08/2005: Function may throw exception in destructor
  Close();
}

gxsSocket_t gxSocket::Socket()
// Create a socket. Returns a valid socket descriptor or
// -1 if the socket cannot be initialized.
{
  gxsocket = socket(address_family, socket_type, protocol_family);

#if defined (__PCLINT_CHECK__)
  if(gxsocket < 0)
#elif defined (__WIN32__) && !defined (__PCLINT_CHECK__)
  // The SOCKET type is unsigned in the WinSock library
  if(gxsocket == INVALID_SOCKET) // Defined as (SOCKET)(~0)
#elif defined (__UNIX__)
  if(gxsocket < 0)
#else
#error You must define a target platform:\n  __WIN32__ or __UNIX__
#endif
    {
      socket_error = gxSOCKET_INIT_ERROR;
      return -1;
    }

  return gxsocket;
}

gxsSocket_t gxSocket::InitSocket(gxsAddressFamily af,
				 gxsSocket_t st,
				 gxsProtocolFamily pf,
				 int port, char *hostname)
// Create and initialize a socket according to the address family,
// socket type, and protocol family. The "hostname" variable is an
// optional parameter that allows clients to specify a server name.
// Returns a valid socket descriptor or -1 if the socket cannot be
// initialized. NOTE: Under all UNIX platforms you must have root 
// privileges to use raw sockets. Windows 95/98/ME/NT only works 
// with the IPPROTO_ICMP protocol when using raw sockets and does
// not support the IPPROTO_RAW protocol.
{
#if defined (__WIN32__) && !defined (__WIN2000__)
  if((st == SOCK_RAW) && (pf != IPPROTO_ICMP)) {
    socket_error = gxSOCKET_SOCKETTYPE_ERROR;
    return -1;
  }
#endif

  address_family = af;
  socket_type = st;
  protocol_family = pf;
  port_number = port;

  // Put the server information into the server structure.
  // Ignore BCC32 conversion warning
  sin.sin_family = address_family;

  if(hostname) {
#if defined (__LINUX__) && defined (__REENTRANT__)
    // 08/25/2016: Added thread safe code to replace gethostbyname() call
    struct addrinfo hints, *res;
    memset (&hints, 0, sizeof (hints));
    hints.ai_family = af;
    hints.ai_socktype = st;
    hints.ai_protocol = pf;
    hints.ai_flags |= AI_PASSIVE; // return socket addresses for use with connect(),sendto(), or sendmsg()
    if(getaddrinfo(hostname, 0, &hints, &res) != 0) {
      socket_error = gxSOCKET_HOSTNAME_ERROR;
      return -1;
    }
    if(!res) {
      socket_error = gxSOCKET_HOSTNAME_ERROR;
      return -1;
    }
    // 10/31/2026: Only an IPv4 result fits in sin
    int rv = gxsCopyAddrInfo(sin.sin_addr, res);
    freeaddrinfo(res);
    if(rv != 0) {
      socket_error = gxSOCKET_HOSTNAME_ERROR;
      return -1;
    }
#else
    // Get the server's Internet address
    gxsHostNameInfo *hostnm = gethostbyname(hostname); 
    if(hostnm == (struct hostent *) 0) {
      socket_error = gxSOCKET_HOSTNAME_ERROR;
      return -1;
    }
    // Put the server information into the client structure.
    // 08/24/2016: Prevent core dump if not set
    // 10/31/2026: Copy 4 bytes, see gxsCopyHostAddress()
    if(gxsCopyHostAddress(sin.sin_addr, hostnm) != 0) {
       socket_error = gxSOCKET_HOSTNAME_ERROR;
       return -1;
    }
#endif
  }
  else {   
    sin.sin_addr.s_addr = INADDR_ANY; // Use my IP address
  }

  // The port must be put into network byte order.
  // htons()--"Host to Network Short" 
  // htonl()--"Host to Network Long" 
  // ntohs()--"Network to Host Short" 
  // ntohl()--"Network to Host Long" 
  // Ignore BCC32 conversion warning
  sin.sin_port = htons(port_number); 

  // Create a TCP/IP
  if(Socket() < 0) {
    socket_error = gxSOCKET_INIT_ERROR;
    return -1;
  }

  return gxsocket;
}

gxsSocket_t gxSocket::InitSocket(gxsSocket_t st, gxsSocketAddress *host_sin)
// Create and initialize a socket according to the socket type and 
// socket internet address. The "host_sin" variable must be initialized 
// prior to calling function. This cross-platform function will only accept 
// SOCK_STREAM and SOCK_DGRAM socket types. Returns a valid socket 
// descriptor or -1 if the socket cannot be initialized.
{
  if(st == SOCK_STREAM) {
    socket_type = SOCK_STREAM;
    protocol_family = IPPROTO_TCP;
  }
  else if(st == SOCK_DGRAM) {
    socket_type = SOCK_DGRAM;
    protocol_family = IPPROTO_UDP;
  }
  else {
    socket_error = gxSOCKET_SOCKETTYPE_ERROR;
    return -1;
  }
  
  return InitSocket(socket_type, protocol_family, host_sin);
}

gxsSocket_t gxSocket::InitSocket(gxsSocket_t st, gxsProtocolFamily pf,
				 gxsSocketAddress *host_sin)
// Create and initialize a socket according to the socket type and protocol 
// family. The "host_sin" variable must be initialized prior to calling
// function. Returns a valid socket descriptor or -1 if the socket cannot be
// initialized. NOTE: Under all UNIX platforms you must have root 
// privileges to use raw sockets. Windows 95/98/ME/NT only works 
// with the IPPROTO_ICMP protocol when using raw sockets and does
// not support the IPPROTO_RAW protocol.
{
#if defined (__WIN32__) && !defined (__WIN2000__)
  if((st == SOCK_RAW) && (pf != IPPROTO_ICMP)) {
    socket_error = gxSOCKET_SOCKETTYPE_ERROR;
    return -1;
  }
#endif

  // Set the socket varaibles
  socket_type = st;
  protocol_family = pf;
  address_family = host_sin->sin_family;
  port_number = host_sin->sin_port;

  if(&sin != host_sin) { // Do not set if the object's sin is used
    memmove(&sin, host_sin, sizeof(gxsSocketAddress));
  }

  // Create a TCP/IP
  if(Socket() < 0) {
    socket_error = gxSOCKET_INIT_ERROR;
    return -1;
  }

  return gxsocket;
}

int gxSocket::InitSIN(gxsSocketAddress *host_sin, gxsAddressFamily af, 
		      int port, char *hostname)
// Helper function used to initialize a socket Internet address data 
// structure. To use the Internet address of this host pass a null 
// pointer to hostname variable. Returns -1 if an error occurs.
{
  // Reset the current SIN
  memset(host_sin, 0, sizeof(gxsSocketAddress));

  // Put the server information into the server structure.
  host_sin->sin_family = address_family;

  if(hostname) {
#if defined (__LINUX__) && defined (__REENTRANT__)
    // 08/28/2016: Added thread safe code to replace gethostbyname() call
    struct addrinfo hints, *res;
    memset (&hints, 0, sizeof (hints));
    hints.ai_family = AF_INET; // 10/31/2026: host_sin holds IPv4 only
    hints.ai_socktype = 0;
    hints.ai_protocol = 0;
    hints.ai_flags |= AI_PASSIVE; // return socket addresses for use with connect(),sendto(), or sendmsg()
    if(getaddrinfo(hostname, 0, &hints, &res) != 0) {
      socket_error = gxSOCKET_HOSTNAME_ERROR;
      return -1;
    }
    if(!res) {
      socket_error = gxSOCKET_HOSTNAME_ERROR;
      return -1;
    }
    // 10/31/2026: Fill in host_sin. This set the object's sin and left
    // host_sin with a zero (any) address.
    int rv = gxsCopyAddrInfo(host_sin->sin_addr, res);
    freeaddrinfo(res);
    if(rv != 0) {
      socket_error = gxSOCKET_HOSTNAME_ERROR;
      return -1;
    }
#else
    // Get the Internet address
    gxsHostNameInfo *hostnm = gethostbyname(hostname); 
    if(hostnm == (struct hostent *) 0) {
      socket_error = gxSOCKET_HOSTNAME_ERROR;
      return -1;
    }
    // Put the server information into the client structure.
    // 10/31/2026: Copy 4 bytes, see gxsCopyHostAddress()
    if(gxsCopyHostAddress(host_sin->sin_addr, hostnm) != 0) {
      socket_error = gxSOCKET_HOSTNAME_ERROR;
      return -1;
    }
#endif
  }
  else {  
    host_sin->sin_addr.s_addr = INADDR_ANY; // Use my IP address
  }

  port_number = port ; // Assigne the port number

  // The port must be put into network byte order.
  host_sin->sin_port = htons(port_number); 
  return 0;
}

gxsSocket_t gxSocket::InitSocket(gxsSocket_t st, gxsPort_t port,
				 char *hostname)
// Create and initialize a socket according to the socket type. This 
// cross-platform function will only accept SOCK_STREAM and SOCK_DGRAM
// socket types. The "hostname" variable is an optional parameter that 
// allows clients to specify a server name. Returns a valid socket 
// descriptor or -1 if the socket cannot be initialized. NOTE: Port 
// numbers are divided into three ranges: the Well Known Ports, the 
// Registered Ports, and the Dynamic and/or Private Ports. A complete 
// list of all assigned port numbers is maintained by IANA: 
//
// http://www.iana.org
//
// The Well Known Ports are those from 0 through 1023.
// The Registered Ports are those from 1024 through 49151
// The Dynamic and/or Private Ports are those from 49152 through 65535
{
  if(st == SOCK_STREAM) {
    socket_type = SOCK_STREAM;
    protocol_family = IPPROTO_TCP;
  }
  else if(st == SOCK_DGRAM) {
    socket_type = SOCK_DGRAM;
    protocol_family = IPPROTO_UDP;
  }
  else {
    socket_error = gxSOCKET_SOCKETTYPE_ERROR;
    return -1;
  }

  return InitSocket(AF_INET, socket_type, 
		    protocol_family, port, hostname);
}

int gxSocket::Bind()
// Bind the socket to a name so that other processes can
// reference it and allow this socket to receive messages.
// Returns -1 if an error occurs.
{
  int rv = bind(gxsocket, (struct sockaddr *)&sin, sizeof(sin));
  if(rv >= 0) {
    is_bound = 1;
  } 
  else {
    socket_error = gxSOCKET_BIND_ERROR;
    is_bound = 0;
  }
  return rv;
}

int gxSocket::Bind(gxsSocket_t s, gxsSocketAddress *sa)
// Bind the socket to a name so that other processes can
// reference it and allow this socket to receive messages.
// Returns -1 if an error occurs.
{
  int rv = bind(s, (struct sockaddr *)sa, sizeof(gxsSocketAddress));
  if(rv < 0) socket_error = gxSOCKET_BIND_ERROR;
  return rv;
}

int gxSocket::Connect(gxsSocket_t s, gxsSocketAddress *sa)
// Connect a specified socket to a client or server. On the client 
// side a connect call is used to initiate a connection. 
// Returns -1 if an error occurs.
{
  int rv = connect(s, (struct sockaddr *)sa, sizeof(gxsSocketAddress));
  if(rv < 0) socket_error = gxSOCKET_CONNECT_ERROR;
  return rv;
}

int gxSocket::Connect()
// Connect the socket to a client or server. On the client side
// a connect call is used to initiate a connection.
// Returns -1 if an error occurs.
{
  int rv = connect(gxsocket, (struct sockaddr *)&sin, sizeof(sin));
  if(rv >= 0) {
    is_connected = 1; 
  }
  else {
    socket_error = gxSOCKET_CONNECT_ERROR;
    is_connected = 0;
  }
  return rv;
}

int gxSocket::ReadSelect(gxsSocket_t s, int seconds, int useconds)
// Function used to multiplex reads without polling. Returns false if a 
// reply time is longer then the timeout values. 
{
#if defined (__UNIX__)
  // 10/31/2026: poll() in place of select(). FD_SET() with a descriptor
  // at or above FD_SETSIZE (1024 on Linux) wrote past the end of the
  // fd_set on the stack, and a server with many open files or
  // connections gets such descriptors. A negative timeout waits with
  // no limit. Returns 1 if the socket can be read (or has an error or
  // hang up to report), 0 on a timeout, -1 if poll() fails.
  if(s < 0) return 0; // CWS - socket was disconnected?
  long long ms;
  if(seconds < 0 || useconds < 0) {
    ms = -1;
  }
  else {
    ms = (long long)seconds * 1000 + (useconds + 999) / 1000;
    if(ms > INT_MAX) ms = INT_MAX;
  }
  struct pollfd pfd;
  pfd.fd = s;
  pfd.events = POLLIN;
  pfd.revents = 0;
  int rv;
  do {
    rv = poll(&pfd, 1, (int)ms);
  } while(rv < 0 && errno == EINTR);
  if(rv > 0) return 1;
  return rv;
#else
  struct timeval timeout;
  fd_set fds;
  FD_ZERO(&fds);
  if(s < 0) return 0;	//CWS - socket was disconnected?
  FD_SET(s, &fds);
  
  timeout.tv_sec = seconds;
  timeout.tv_usec = useconds;

  // This function calls select() giving it the file descriptor of
  // the socket. The kernel reports back to this function when the file
  // descriptor has woken it up.
  return select(s+1, &fds, 0, 0, &timeout);
#endif
}

int gxSocket::Recv(void *buf, int bytes, int flags)
// Receive a block of data from the bound socket and do not return
// until all the bytes have been read. Returns the total number of 
// bytes received or -1 if an error occurs.
{
  return Recv(gxsocket, buf, bytes, flags);
}

int gxSocket::Recv(void *buf, int bytes, int seconds, int useconds, int flags)
// Receive a block of data from the bound socket and do not return
// until all the bytes have been read or the timeout value has been 
// exceeded. Returns the total number of bytes received or -1 if an 
// error occurs.
{
  return Recv(gxsocket, buf, bytes, seconds, useconds, flags);
}

int gxSocket::Send(const void *buf, int bytes, int flags)
// Send a block of data to the bound socket and do not return
// until all the bytes have been written. Returns the total number 
// of bytes sent or -1 if an error occurs.
{
  return Send(gxsocket, buf, bytes, flags);
}

int gxSocket::Recv(gxsSocket_t s, void *buf, int bytes, int flags)
// Receive a block of data from a specified socket and do not return
// until all the bytes have been read. Returns the total number of
// bytes received or -1 if an error occurs.
{
  // no timeout receive
  return Recv(s, buf, bytes, 0, 0, 0, flags);
}


int gxSocket::Recv(gxsSocket_t s, void *buf, int bytes, 
		   int seconds, int useconds, int flags)
// Receive a block of data from a specified socket and do not return
// until all the bytes have been read or the timeout value has been 
// exceeded. Returns the total number of bytes received or -1 if an 
// error occurs.
{
  // timeout receive
  return Recv(s, buf, bytes, 1, seconds, useconds, flags);
}

int gxSocket::Recv(gxsSocket_t s, void *buf, int bytes, 
		   int useTimeout, int seconds, int useconds, int flags)
// Receive a block of data from a specified socket and do not return
// until all the bytes have been read. Returns the total number of
// bytes received or -1 if an error occurs and 0 bytes read.
// CWS - optional timeout specified; may have 0 timeout to return immediately
// so added 'useTimeout' flag rather than checking for 0 seconds to indicate 
// no timeout.
{
  bytes_read = 0;        // Reset the byte counter
  int num_read = 0;      // Actual number of bytes read
  int num_req = bytes;   // Number of bytes requested 
  char *p = (char *)buf; // Pointer to the buffer

  while(bytes_read < bytes) { // Loop until the buffer is full
    if(useTimeout && (!ReadSelect(s, seconds, useconds))) {
      //CWS - problem if has bytes and timeout occurs need to return bytes read
      if (bytes_read > 0) return bytes_read;
      socket_error = gxSOCKET_REQUEST_TIMEOUT;
      return -1; // Exceeded the timeout value
    }
    if((num_read = recv(s, p, num_req-bytes_read, flags)) > 0) {
      bytes_read += num_read;   // Increment the byte counter
      p += num_read;            // Move the buffer pointer for the next read
    }
    else {
      if(num_read == 0) {// Gracefully closed
	socket_error = gxSOCKET_DISCONNECTED_ERROR;
      }
      else {
	socket_error = gxSOCKET_RECEIVE_ERROR;
      }

      // CWS - If something has been read, do not return -1 or 
      // will lose the count
      if (bytes_read > 0) return bytes_read;

      return -1; // An error occurred during the read
    }
  }
  return bytes_read;
}

int gxSocket::Send(gxsSocket_t s, const void *buf, int bytes, int flags)
// Send a block of data to a specified socket and do not return
// until all the bytes have been written. Returns the total number of
// bytes sent or -1 if an error occurs.
{
  bytes_moved = 0;       // Reset the byte counter
  int num_moved = 0;     // Actual number of bytes written
  int num_req = bytes;   // Number of bytes requested 
  char *p = (char *)buf; // Pointer to the buffer

  while(bytes_moved < bytes) { // Loop until the buffer is empty
    // 10/31/2026: MSG_NOSIGNAL on Linux, see gxsSEND_FLAGS
    if((num_moved = send(s, p, num_req-bytes_moved, 
			 flags | gxsSEND_FLAGS)) > 0) {
      bytes_moved += num_moved;  // Increment the byte counter
      p += num_moved;            // Move the buffer pointer for the next read
    }
    // 10/31/2026: A send() that moves no bytes is an error. It used
    // to loop here for ever.
    if(num_moved <= 0) {
      socket_error = gxSOCKET_TRANSMIT_ERROR;
      return -1; // An error occurred during the read
    }
  }

  return bytes_moved;
}

int gxSocket::RemoteRecv(void *buf, int bytes, int seconds, int useconds, 
			 int flags)
// Receive a block of data from a remote socket in blocking mode with a
// specified timeout value. Returns the total number of bytes received or 
// -1 if an error occurs.
{
  return Recv(remote_socket, buf, bytes, seconds, useconds, flags);
}

int gxSocket::RemoteRecv(void *buf, int bytes, int flags)
// Receive a block of data from a remote socket in blocking mode.
// Returns the total number of bytes received or -1 if an error occurs.
{
  return Recv(remote_socket, buf, bytes, flags);
}

int gxSocket::RemoteSend(const void *buf, int bytes, int flags)
// Send a block of data to a remote socket and do not return
// until all the bytes have been written. Returns the total 
// number of bytes received or -1 if an error occurs.
{
  return Send(remote_socket, buf, bytes, flags);
}

void gxSocket::ShutDown(int how)
// Used to close and un-initialize a full-duplex socket.
{
  bytes_moved = 0;
  bytes_read = 0;
  is_connected = 0;
  is_bound = 0;

  // NOTE 05/25/2000: The last error is no longer reset following
  // a close call to allow the application to close the socket
  // and catch the last exception reported if the socket has to be
  // closed after a Bind(), Listen(), or any other initialization
  // failure.
  // socket_error = gxSOCKET_NO_ERROR;
 
  // 0 = User is no longer interested in reading data
  // 1 = No more data will be sent
  // 2 = No data is to be sent or received
  if(gxsocket != -1) shutdown(gxsocket, how);
  if(remote_socket != -1) shutdown(remote_socket, how);

  gxsocket = -1;
  remote_socket = -1;
}

void gxSocket::ShutDown(gxsSocket_t &s, int how)
// Used to close and un-initialize the specified full-duplex socket.
{
  if(s != -1) shutdown(s, how);
  s = -1;
}

void gxSocket::ShutDownSocket(int how)
// Used to close a full-duplex server side socket.
{
  if(gxsocket != -1) shutdown(gxsocket, how);
  gxsocket = -1;
}

void gxSocket::ShutDownRemoteSocket(int how)
// Used to close a full-duplex client side socket.
{
  if(remote_socket != -1) shutdown(remote_socket, how);
  remote_socket = -1;
}

void gxSocket::Close()
// Close any and un-initialize any bound sockets.
{
  bytes_moved = 0;
  bytes_read = 0;
  is_connected = 0;
  is_bound = 0;

  // NOTE 05/25/2000: The last error is no longer reset following
  // a close call to allow the application to close the socket
  // and catch the last exception reported if the socket has to be
  // closed after a Bind(), Listen(), or any other initialization
  // failure.
  // socket_error = gxSOCKET_NO_ERROR;
#if defined (__WIN32__)
  if(gxsocket != -1) closesocket(gxsocket);
  if(remote_socket != -1) closesocket(remote_socket);
#elif defined (__UNIX__)
  if(gxsocket != -1) close(gxsocket);
  if(remote_socket != -1) close(remote_socket);
#else
#error You must define a target platform: __WIN32__ or __UNIX__
#endif
  // 10/31/2026: Forget the closed descriptors. A second Close(), or the
  // destructor after Close(), closed the same numbers again, and by
  // then they could belong to files or sockets the program had opened
  // since.
  gxsocket = -1;
  remote_socket = -1;
}

void gxSocket::Close(gxsSocket_t &s)
// Close the specified socket
{
#if defined (__WIN32__)
  if(s != -1) closesocket(s);
#elif defined (__UNIX__)
  if(s != -1) close(s);
#else
#error You must define a target platform: __WIN32__ or __UNIX__
#endif
  s = -1;
}

void gxSocket::CloseSocket()
// Close the server side socket
{
#if defined (__WIN32__)
  if(gxsocket != -1) closesocket(gxsocket);
#elif defined (__UNIX__)
  if(gxsocket != -1) close(gxsocket);
#else
#error You must define a target platform: __WIN32__ or __UNIX__
#endif
  gxsocket = -1;
}

void gxSocket::CloseRemoteSocket()
// Close the client socket
{
#if defined (__WIN32__)
  if(remote_socket != -1) closesocket(remote_socket);
#elif defined (__UNIX__)
  if(remote_socket != -1) close(remote_socket);
#else
#error You must define a target platform: __WIN32__ or __UNIX__
#endif
  remote_socket = -1;
}

int gxSocket::Listen(gxsSocket_t s, int max_connections)
// Listen for connections on the specified socket.
// The "max_connections" variable determines how many
// pending connections the queue will hold. Returns -1
// if an error occurs.
{
  int rv = listen(s,                // Bound socket
		  max_connections); // Number of connection request queue
  if(rv < 0) socket_error = gxSOCKET_LISTEN_ERROR;
  return rv;
}

int gxSocket::Listen(int max_connections)
// Listen for connections if configured as a server.
// The "max_connections" variable determines how many
// pending connections the queue will hold. Returns -1
// if an error occurs.
{
  return listen(gxsocket, max_connections);
}

gxsSocket_t gxSocket::Accept(gxsSocket_t s, gxsSocketAddress *sa)
// Accept a connect from a specified socket. An Accept() 
// call blocks the server until the a client requests 
// service. Returns a valid socket descriptor or -1 
// if an error occurs.

{
  // Length of client address
  gxsSocketLength_t addr_size = (gxsSocketLength_t)sizeof(gxsSocketAddress); 

  gxsSocket_t client_socket = accept(s, (struct sockaddr *)sa, &addr_size);

#if defined (__PCLINT_CHECK__)
  if(client_socket < 0)
#elif defined (__WIN32__) && !defined (__PCLINT_CHECK__)
  // The SOCKET type is unsigned in the WinSock library
  if(client_socket == INVALID_SOCKET) // Defined as (SOCKET)(~0)
#elif defined (__UNIX__)
  if(client_socket < 0)
#else
#error You must define a target platform: __WIN32__ or __UNIX__
#endif
    {
      socket_error = gxSOCKET_ACCEPT_ERROR;
      return -1;
    }

  return client_socket;
}

gxsSocket_t gxSocket::Accept()
// Accept a connect from a remote socket. An Accept() 
// call blocks the server until the a client requests 
// service. Returns a valid socket descriptor or -1 
// if an error occurs.
{
  // Length of client address
  gxsSocketLength_t addr_size = (gxsSocketLength_t)sizeof(remote_sin); 

  remote_socket = accept(gxsocket, (struct sockaddr *)&remote_sin, &addr_size);

#if defined (__PCLINT_CHECK__)
  if(remote_socket < 0)
#elif defined (__WIN32__) && !defined (__PCLINT_CHECK__)
  // The SOCKET type is unsigned in the WinSock library
  if(remote_socket == INVALID_SOCKET) // Defined as (SOCKET)(~0)
#elif defined (__UNIX__)
  if(remote_socket < 0)
#else
#error You must define a target platform: __WIN32__ or __UNIX__
#endif
    {
      socket_error = gxSOCKET_ACCEPT_ERROR;
      return -1;
    }

  return remote_socket;
}

int gxSocket::GetSockName(gxsSocket_t s, gxsSocketAddress *sa)
// Retrieves the current name for the specified socket descriptor.
// It is used on a bound and/or connected socket and returns the
// local association. This function is especially useful when a
// connect call has been made without doing a bind first in which
// case this function provides the only means by which you can
// determine the local association which has been set by the system.
// Returns -1 if an error occurs.
{
  gxsSocketLength_t namelen = (gxsSocketLength_t)sizeof(gxsSocketAddress);
  int rv = getsockname(s, (struct sockaddr *)sa, &namelen);
  if(rv < 0) socket_error = gxSOCKET_SOCKNAME_ERROR;
  return rv;
}

int gxSocket::GetSockName()
// Retrieves the current name for this objects socket descriptor.
// Returns -1 if an error occurs.
{
  return GetSockName(gxsocket, &sin);
}

int gxSocket::GetPeerName(gxsSocket_t s, gxsSocketAddress *sa)
// Retrieves the current name of the specified socket descriptor.
// Returns -1 if an error occurs.
{
  gxsSocketLength_t namelen = (gxsSocketLength_t)sizeof(gxsSocketAddress);
  // 07/31/2009: modified by Eduardo Garcia
  //int rv = getpeername(s, (struct sockaddr *)&sa, &namelen); 
  int rv = getpeername(s, (struct sockaddr *)sa, &namelen);  
  if(rv < 0) socket_error = gxSOCKET_PEERNAME_ERROR;  
  return rv; 
}

int gxSocket::GetPeerName()
// Retrieves the current name for the remote socket descriptor.
// Returns -1 if an error occurs.
{
  return GetPeerName(remote_socket, &remote_sin);
}

int gxSocket::GetSockOpt(gxsSocket_t s, int level, int optName, 
			 void *optVal, unsigned *optLen)
// Gets the current socket option for the specified option level or name.
// Returns -1 if an error occurs.
{
#if defined (__WIN32__)
  int rv = getsockopt(s, level, optName, (char *)optVal, 
		      (gxsSocketLength_t *)optLen);
#else
  int rv = getsockopt(s, level, optName, optVal, (gxsSocketLength_t *)optLen);
#endif
  if(rv < 0) socket_error = gxSOCKET_SETOPTION_ERROR;
  return rv;
}

int gxSocket::GetSockOpt(int level, int optName, void *optVal, 
			 unsigned *optLen)
// Gets the current socket option for the specified option level or name.
// Returns -1 if an error occurs.
{
  return GetSockOpt(gxsocket, level, optName, optVal, optLen);
}

int gxSocket::SetSockOpt(gxsSocket_t s, int level, int optName,
			 const void *optVal, unsigned optLen)
// Sets the current socket option for the specified option level or name.
// Returns -1 if an error occurs.
{
#if defined (__WIN32__)
  int rv = setsockopt(s, level, optName, (const char *)optVal, 
		      (gxsSocketLength_t)optLen);
#else
  int rv = setsockopt(s, level, optName, optVal, (gxsSocketLength_t)optLen);
#endif
  if(rv < 0) socket_error = gxSOCKET_SETOPTION_ERROR;
  return rv;
}

int gxSocket::SetSockOpt(int level, int optName, const void *optVal, 
			 unsigned optLen)
// Sets the current socket option for the specified option level or name.
// Returns -1 if an error occurs.
{
  return SetSockOpt(gxsocket, level, optName, optVal, optLen);
}

int gxSocket::SetIOTimeout(gxsSocket_t s, int seconds)
// 10/31/2026: Limit each blocking send and receive on the specified
// socket to seconds (SO_RCVTIMEO and SO_SNDTIMEO). A receive or send
// that times out fails with an error. 0 or less removes the limit.
// Returns -1 if an error occurs.
{
  if(seconds < 0) seconds = 0;
#if defined (__WIN32__)
  DWORD ms = (DWORD)seconds * 1000;
  if(SetSockOpt(s, SOL_SOCKET, SO_RCVTIMEO, &ms, sizeof(ms)) < 0) return -1;
  return SetSockOpt(s, SOL_SOCKET, SO_SNDTIMEO, &ms, sizeof(ms));
#else
  struct timeval tv;
  tv.tv_sec = seconds;
  tv.tv_usec = 0;
  if(SetSockOpt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) return -1;
  return SetSockOpt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
}

int gxSocket::SetIOTimeout(int seconds)
// 10/31/2026: SetIOTimeout() for the bound socket and, if one is
// open, the remote socket. Returns -1 if an error occurs.
{
  int rv = 0;
  if(gxsocket != -1 && SetIOTimeout(gxsocket, seconds) < 0) rv = -1;
  if(remote_socket != -1 && SetIOTimeout(remote_socket, seconds) < 0) rv = -1;
  return rv;
}

gxsServent *gxSocket::GetServiceInformation(char *name, char *protocol)
// Function used to obtain service information about a specified name. 
// The source of this information is dependent on the calling function's
// platform configuration which should be a local services file or NIS 
// database. Returns a pointer to a gxsServent data structure if service
// information is available or a null value if the service cannot be
// found. NOTE: The calling function must free the memory allocated
// for gxsServent data structure upon each successful return.
{
  // If the "protocol" pointer is NULL, getservbyname returns
  // the first service entry for which the name matches the s_name
  // or one of the s_aliases. Otherwise getservbyname matches both
  // the name and the proto.
  gxsServent *sp = getservbyname(name, protocol);
  if(sp == 0) return 0;

  gxsServent *buf = new gxsServent;
  if(!buf) return 0; // Memory allocation error
  memmove(buf, sp, sizeof(gxsServent));
  return buf;
}

gxsServent *gxSocket::GetServiceInformation(int port, char *protocol)
// Function used to obtain service information about a specified port. 
// The source of this information is dependent on the calling function's
// platform configuration which should be a local services file or NIS 
// database. Returns a pointer to a gxsServent data structure if service
// information is available or a null value if the service cannot be
// found. NOTE: The calling function must free the memory allocated
// for gxsServent data structure upon each successful return.
{
  // If the "protocol" pointer is NULL, getservbyport returns the
  // first service entry for which the port matches the s_port.
  // Otherwise getservbyport matches both the port and the proto.
  gxsServent *sp = getservbyport(port, protocol);
  if(sp == 0) return 0;

  gxsServent *buf = new gxsServent;
  if(!buf) return 0; // Memory allocation error
  memmove(buf, sp, sizeof(gxsServent));
  return buf;
}

int gxSocket::GetServByName(char *name, char *protocol)
// Set service information corresponding to a service name and protocol.
// Returns -1 if an unknown service or protocol is requested. NOTE: This
// information is obtained from this machines local services file or
// from a NIS database.
{
  // If the "protocol" pointer is NULL, getservbyname returns
  // the first service entry for which the name matches the s_name
  // or one of the s_aliases. Otherwise getservbyname matches both
  // the name and the proto.
  gxsServent *sp = getservbyname(name, protocol);
  if(sp == 0) {
    socket_error = gxSOCKET_PROTOCOL_ERROR;
    return -1;
  }
  sin.sin_port = sp->s_port;
  return 0;
}

int gxSocket::GetServByPort(int port, char *protocol)
// Set service information corresponding to a port number and protocol.
// Returns -1 if an unknown service or protocol is requested. NOTE: This
// information is obtained from this machines local services file or
// from a NIS database.
{
  // If the "protocol" pointer is NULL, getservbyport returns the
  // first service entry for which the port matches the s_port.
  // Otherwise getservbyport matches both the port and the proto.
  gxsServent *sp = getservbyport(port, protocol);
  if(sp == 0) {
    socket_error = gxSOCKET_PROTOCOL_ERROR;
    return -1;
  }
  sin.sin_port = sp->s_port;
  return 0;
}

int gxSocket::GetPortNumber()
// Return the port number actually set by the system. Use this function
// after a call to gxSocket::GetSockName();
{
  return ntohs(sin.sin_port);
}

int gxSocket::GetRemotePortNumber()
// Return the port number of the client socket.
{
  return ntohs(remote_sin.sin_port);
}

gxsAddressFamily gxSocket::GetAddressFamily()
// Returns the address family of this socket
{
  return sin.sin_family;
}

gxsAddressFamily gxSocket::GetRemoteAddressFamily()
// Returns the address family of the remote socket.
{
  return remote_sin.sin_family;  
}

gxsHostNameInfo *gxSocket::GetHostInformation(char *hostname)
// Function used to obtain hostname information about a specified host. 
// The source of this information is dependent on the calling function's
// platform configuration which should be a DNS, local host table, and/or
// NIS database. Returns a pointer to a gxsHostNameInfo data structure
// if information is available or a null value if the hostname cannot be
// found. NOTE: The calling function must free the memory allocated
// for gxsHostNameInfo data structure upon each successful return.
{
  gxsInternetAddress hostia;
  gxsHostNameInfo *hostinfo;
  hostia.s_addr = inet_addr(hostname);

#if defined (__SOLARIS__)
#ifndef INADDR_NONE // IPv4 Internet address integer constant not defined
#define INADDR_NONE 0xffffffff
#endif
#endif

  if(hostia.s_addr == INADDR_NONE) { // Look up host by name
    hostinfo = gethostbyname(hostname); 
  }
  else {  // Look up host by IP address
    hostinfo = gethostbyaddr((const char *)&hostia, 
			     sizeof(gxsInternetAddress), AF_INET);
  }
  if(hostinfo == (gxsHostNameInfo *) 0) { // No host name info avialable
    return 0;
  }

  gxsHostNameInfo *buf = new gxsHostNameInfo;
  if(!buf) return 0; // Memory allocation error
  memmove(buf, hostinfo, sizeof(gxsHostNameInfo));
  return buf;
}
  
int gxSocket::GetHostName(char *sbuf)
// Pass back the host name of this machine in the "sbuf" variable.
// A memory buffer equal to "gxsMAX_NAME_LEN" must be pre-allocated
// prior to using this function. Return -1 if an error occurs.
{
  // 10/31/2026: A null buffer is an error. It used to allocate a local
  // buffer the caller never received, which leaked.
  if(!sbuf) {
    socket_error = gxSOCKET_HOSTNAME_ERROR;
    return -1;
  }
  int rv = gethostname(sbuf, gxsMAX_NAME_LEN);
  if(rv < 0) { 
    socket_error = gxSOCKET_HOSTNAME_ERROR;
  }
  // 10/31/2026: gethostname() does not always end a name it had to
  // cut with a null
  sbuf[gxsMAX_NAME_LEN-1] = 0;
  return rv;
}

int gxSocket::GetIPAddress(char *sbuf)
// Pass back the IP Address of this machine in the "sbuf" variable.
// A memory buffer equal to "gxsMAX_NAME_LEN" must be pre-allocated
// prior to using this function. Return -1 if an error occurs.
{
  // 10/14/2026: A null buffer is an error. It used to allocate a local
  // buffer the caller never received, which leaked.
  // 10/31/2026: Checked first; the host information leaked
  if(!sbuf) return -1;

  char hostname[gxsMAX_NAME_LEN];
  int rv = GetHostName(hostname);
  if(rv < 0) return rv;

  gxsHostNameInfo *hostinfo = GetHostInformation(hostname);
  if(!hostinfo) {
    socket_error = gxSOCKET_HOSTNAME_ERROR;
    return -1;
  }
  // 10/31/2026: Check the address type, length and list before use
  gxsInternetAddress ia;
  rv = gxsCopyHostAddress(ia, hostinfo);
  delete hostinfo;
  if(rv != 0) {
    socket_error = gxSOCKET_HOSTNAME_ERROR;
    return -1;
  }
  strcpy(sbuf, inet_ntoa(ia)); // At most 15 characters
  return 0;
}

int gxSocket::GetDomainName(char *sbuf)
// Pass back the domain name of this machine in the "sbuf" variable.
// A memory buffer equal to "gxsMAX_NAME_LEN" must be pre-allocated
// prior to using this function. Return -1 if an error occurs.
{
  // 10/14/2026: A null buffer is an error. It used to allocate a local
  // buffer the caller never received, which leaked.
  // 10/31/2026: Checked first; the host information leaked
  if(!sbuf) return -1;

  char hostname[gxsMAX_NAME_LEN];
  int rv = GetHostName(hostname);
  if(rv < 0) return rv;

  gxsHostNameInfo *hostinfo = GetHostInformation(hostname);
  if(!hostinfo) {
    socket_error = gxSOCKET_HOSTNAME_ERROR;
    return -1;
  }

  // 10/31/2026: The buffer holds gxsMAX_NAME_LEN bytes and a DNS name
  // can be longer; strcpy() wrote past its end
  sbuf[0] = 0;
  if(hostinfo->h_name) {
    strncpy(sbuf, hostinfo->h_name, (gxsMAX_NAME_LEN-1));
    sbuf[gxsMAX_NAME_LEN-1] = 0;
  }
  int i; int len = strlen(sbuf);
  for(i = 0; i < len; i++) {
    if(sbuf[i] == '.') break;
  }
  if(++i < len) {
    len -= i;
    memmove(sbuf, sbuf+i, len);
    sbuf[len] = 0; // Null terminate the string
  }
  delete hostinfo;
  return 0;
}

int gxSocket::GetBoundIPAddress(char *sbuf)
// Pass back the local or server IP address in the "sbuf" variable.
// A memory buffer equal to "gxsMAX_NAME_LEN" must be pre-allocated
// prior to using this function. Return -1 if an error occurs.
{
  char *s = inet_ntoa(sin.sin_addr);
  if(s == 0) {
    socket_error = gxSOCKET_HOSTNAME_ERROR;
    return -1;
  }

  // 10/14/2026: A null buffer is an error. It used to allocate a local
  // buffer the caller never received, which leaked.
  if(!sbuf) return -1;

  strcpy(sbuf, s);
  return 0;
}

int gxSocket::GetRemoteHostName(char *sbuf)
// Pass back the client host name client in the "sbuf" variable.
// A memory buffer equal to "gxsMAX_NAME_LEN" must be pre-allocated
// prior to using this function. Return -1 if an error occurs.
{
  
#if defined (__LINUX__) && defined (__REENTRANT__)
  // 08/25/2016: Added thread safe code to replace inet_ntoa call
  char client_name[gxsMAX_NAME_LEN];
  memset(client_name, 0, gxsMAX_NAME_LEN);
  inet_ntop(AF_INET, &remote_sin.sin_addr, client_name, gxsMAX_NAME_LEN);

  // 10/14/2026: A null buffer is an error. It used to allocate a local
  // buffer the caller never received, which leaked.
  if(!sbuf) return -1;
  strncpy(sbuf, client_name, (gxsMAX_NAME_LEN-1));
  sbuf[gxsMAX_NAME_LEN-1] = 0; // 10/14/2026: strncpy may not terminate
#else
  char *s = inet_ntoa(remote_sin.sin_addr);
  if(s == 0) {
    socket_error = gxSOCKET_HOSTNAME_ERROR;
    return -1;
  }

  // 10/14/2026: A null buffer is an error. It used to allocate a local
  // buffer the caller never received, which leaked.
  if(!sbuf) return -1;

  strcpy(sbuf, s);
#endif
  return 0;
}

int gxSocket::RawRead(void *buf, int bytes, int flags)
// Receive a block of data from the bound socket. NOTE: A
// raw read may return before all the bytes have been read.
// Returns -1 if an error occurs.
{
  return RawRead(gxsocket, buf, bytes, flags);
}

int gxSocket::RawWrite(const void *buf, int bytes, int flags)
// Send a block of data to the bound socket. NOTE: A raw write
// may return before all the bytes have been written. Returns -1
// if an error occurs.
{
  return RawWrite(gxsocket, buf, bytes, flags);
}

int gxSocket::RawRead(gxsSocket_t s, void *buf, int bytes, int flags)
// Receive a block of data from a specified socket. NOTE: A
// raw read may return before all the bytes have been read.
// Returns -1 if an error occurs.
{
  bytes_read = 0;
  bytes_read = recv(s, (char *)buf, bytes, flags);
  if(bytes_read < 0) { // An error occurred during the read
    socket_error = gxSOCKET_RECEIVE_ERROR;
  }
  if(bytes_read == 0) { // Gracefully closed
    socket_error = gxSOCKET_DISCONNECTED_ERROR;
  }
  return bytes_read;
}

int gxSocket::RawWrite(gxsSocket_t s, const void *buf, int bytes,
		       int flags)
// Send a block of data to a specified socket. NOTE: A raw write
// may return before all the bytes have been written. Returns -1 if an
// error occurs.
{
  bytes_moved = 0;
  // 10/31/2026: MSG_NOSIGNAL on Linux, see gxsSEND_FLAGS
  bytes_moved = send(s, (char *)buf, bytes, flags | gxsSEND_FLAGS);
  if(bytes_moved < 0) socket_error = gxSOCKET_TRANSMIT_ERROR;
  return bytes_moved;
}

int gxSocket::RawRemoteRead(void *buf, int bytes, int flags)
// Receive a block of data from a remote socket.  NOTE: A
// raw read may return before all the bytes have been read.
// Returns -1 if an error occurs.
{
  return RawRead(remote_socket, buf, bytes, flags);
}

int gxSocket::RawRemoteWrite(const void *buf, int bytes, int flags)
// Send a block of data to a remote socket. NOTE: A raw write
// may return before all the bytes have been written. Returns
// -1 if an error occurs.
{
  return RawWrite(remote_socket, buf, bytes, flags);
}

const char *gxSocket::SocketExceptionMessage()
// Returns a null terminated string that can
// be used to log or print a socket exception.
{
  // 10/31/2026: Negative codes read before the start of the array
  if(((int)socket_error > (gxsMaxSocketExceptionMessages-1)) ||
     ((int)socket_error < 0))
    socket_error = gxSOCKET_INVALID_ERROR_CODE;
  
  // Find the corresponding message in the exception array
  int error = (int)socket_error;
  return gxsSocketExceptionMessages[error];
}

const char *gxSocket::TestExceptionMessage(int err)
// Returns a null terminated string that can
// be used to test socket exception messages.
{
  if(err > (gxsMaxSocketExceptionMessages-1)) err = gxSOCKET_INVALID_ERROR_CODE;
  if(err < 0) err = gxSOCKET_INVALID_ERROR_CODE;

  // Find the corresponding message in the exception array
  return gxsSocketExceptionMessages[err];
}

void gxSocket::GetClientInfo(char *client_name, int &r_port)
// Get the client's host name and port number. NOTE: This
// function assumes that a block of memory equal to the
// gxsMAX_NAME_LEN constant has already been allocated.
{
  int rv = GetRemoteHostName(client_name);
  if(rv < 0) {
    char *unc = (char *)"UNKNOWN";
    memset(client_name, 0,  gxsMAX_NAME_LEN);
    strncpy(client_name, unc, (gxsMAX_NAME_LEN-1));
  }
  r_port = GetRemotePortNumber();
}

int gxSocket::RecvFrom(gxsSocket_t s, gxsSocketAddress *sa, void *buf,
		       int bytes, int seconds, int useconds, int flags)
// Receive a block of data from a remote datagram socket 
// and do not return until all the bytes have been read 
// or the timeout value has been exceeded. Returns the total 
// number of bytes received or -1 if an error occurs.
{
  // Length of client address
  gxsSocketLength_t addr_size = (gxsSocketLength_t)sizeof(gxsSocketAddress); 
  bytes_read = 0;        // Reset the byte counter
  int num_read = 0;      // Actual number of bytes read
  int num_req = bytes;   // Number of bytes requested 
  char *p = (char *)buf; // Pointer to the buffer

  while(bytes_read < bytes) { // Loop until the buffer is full
    if(!ReadSelect(s, seconds, useconds)) {
      socket_error = gxSOCKET_REQUEST_TIMEOUT;
      return -1; // Exceeded the timeout value
    }
    if((num_read = recvfrom(s, p, num_req-bytes_read, flags, \
			    (struct sockaddr *)sa, &addr_size)) > 0) {
      bytes_read += num_read;   // Increment the byte counter
      p += num_read;            // Move the buffer pointer for the next read
    }
    if(num_read == 0) { // Gracefully closed
      socket_error = gxSOCKET_DISCONNECTED_ERROR;
      return -1; // An error occurred during the read
    }
    if(num_read < 0) {
      socket_error = gxSOCKET_RECEIVE_ERROR;
      return -1; // An error occurred during the read
    }
  }
  return bytes_read;
}

int gxSocket::RecvFrom(gxsSocket_t s, gxsSocketAddress *sa, void *buf,
		       int bytes, int flags)
// Receive a block of data from a remote datagram socket 
// and do not return until all the bytes have been read. 
// Returns the total number of bytes received or -1 if 
// an error occurs.
{
  // Length of client address
  gxsSocketLength_t addr_size = (gxsSocketLength_t)sizeof(gxsSocketAddress); 
  bytes_read = 0;        // Reset the byte counter
  int num_read = 0;      // Actual number of bytes read
  int num_req = bytes;   // Number of bytes requested 
  char *p = (char *)buf; // Pointer to the buffer

  while(bytes_read < bytes) { // Loop until the buffer is full
    if((num_read = recvfrom(s, p, num_req-bytes_read, flags, \
			    (struct sockaddr *)sa, &addr_size)) > 0) {
      bytes_read += num_read;   // Increment the byte counter
      p += num_read;            // Move the buffer pointer for the next read
    }
    if(num_read == 0) { // Gracefully closed
      socket_error = gxSOCKET_DISCONNECTED_ERROR;
      return -1; // An error occurred during the read
    }
    if(num_read < 0) {
      socket_error = gxSOCKET_RECEIVE_ERROR;
      return -1; // An error occurred during the read
    }
  }
  return bytes_read;
}

int gxSocket::SendTo(gxsSocket_t s, gxsSocketAddress *sa, 
		     const void *buf, int bytes, int flags)
// Send a block of data to a datagram socket and do not return
// until all the bytes have been written. Returns the total number
// of bytes sent or -1 if an error occurs.
{
  // Length of address
  gxsSocketLength_t addr_size = (gxsSocketLength_t)sizeof(gxsSocketAddress);
  bytes_moved = 0;       // Reset the byte counter
  int num_moved = 0;     // Actual number of bytes written
  int num_req = bytes;   // Number of bytes requested 
  char *p = (char *)buf; // Pointer to the buffer

  while(bytes_moved < bytes) { // Loop until the buffer is full
    if((num_moved = sendto(s, p, num_req-bytes_moved, flags, \
			   (const struct sockaddr *)sa, addr_size)) > 0) {
      bytes_moved += num_moved;  // Increment the byte counter
      p += num_moved;            // Move the buffer pointer for the next read
    }
    // 10/31/2026: A sendto() that moves no bytes is an error. It used
    // to loop here for ever.
    if(num_moved <= 0) {
      socket_error = gxSOCKET_TRANSMIT_ERROR;
      return -1; // An error occurred during the read
    }
  }
  return bytes_moved;
}

int gxSocket::RawReadFrom(gxsSocket_t s, gxsSocketAddress *sa, void *buf,
			  int bytes, int flags)
// Receive a block of data from a remote datagram socket. NOTE: 
// A raw read may return before all the bytes have been read.
// Returns -1 if an error occurs.
{
  // Length of client address
  gxsSocketLength_t addr_size = (gxsSocketLength_t)sizeof(gxsSocketAddress);
  bytes_read = 0;
  bytes_read = recvfrom(s, (char *)buf, bytes, flags,
			(struct sockaddr *)sa, &addr_size);
  if(bytes_read < 0) { // An error occurred during the read
    socket_error = gxSOCKET_RECEIVE_ERROR;
  }
  if(bytes_read == 0) { // Gracefully closed
    socket_error = gxSOCKET_DISCONNECTED_ERROR;
  }
  return bytes_read;
}

int gxSocket::RawWriteTo(gxsSocket_t s, gxsSocketAddress *sa, 
			 const void *buf, int bytes, int flags)
// Send a block of data to a datagram socket. NOTE: A raw write
// may return before all the bytes have been written. Returns -1
// if an error occurs.
{
  // Length of address
  gxsSocketLength_t addr_size = (gxsSocketLength_t)sizeof(gxsSocketAddress);
  bytes_moved = 0;
  bytes_moved = sendto(s, (char *)buf, bytes, flags,
		       (const struct sockaddr *)sa, addr_size);
  if(bytes_moved < 0) socket_error = gxSOCKET_TRANSMIT_ERROR;
  return bytes_moved;
}

int gxSocket::InitSocketLibrary()
// Perform any platform specific initialization required
// before network communication can be established.
// Returns zero if no errors occur, -1 if an error occurs
// and the error can be mapped internally, or a value greater
// then zero if an error occurs and the error cannot be
// determined.
{
#if defined (__WIN32__)
  // Initialize the WinSock DLL with the specified version.
  WORD wVersionRequested;
  switch(socket_version) { 
    // NOTE: An application can successfully use a Windows Sockets
    // DLL if there is any overlap in the version ranges. 
    case gxSOCKET_WSAVER_ONEZERO:
      // Included for test purposes only
      // Ignore BCC32 parentheses warning
      wVersionRequested = MAKEWORD(1, 0);
      break;
    case gxSOCKET_WSAVER_ONEONE:
      // Windows 95A/B/C, 98, 98SE, ME, NT4.0, 2000, XP compatiable
      // Ignore BCC32 parentheses warning
      wVersionRequested = MAKEWORD(1, 1);
      break;
    case gxSOCKET_WSAVER_TWOZERO:
      // Ignore BCC32 parentheses warning
      // Windows 98, 98SE, ME, 2000, XP compatiable
      wVersionRequested = MAKEWORD(2, 0);
      break;
    case gxSOCKET_WSAVER_TWOTWO:
      // Ignore BCC32 parentheses warning
      // Windows 98SE, ME, 2000, XP compatiable
      wVersionRequested = MAKEWORD(2, 2);
      break;
    default:
      // Ignore BCC32 parentheses warning
      // Should work on all WIN32 platforms
      wVersionRequested = MAKEWORD(1, 1);
      break;
  }

  // Initialize the WinSock DLL. Every Windows Sockets application
  // MUST make a WSAStartup call before issuing any other Windows
  // Sockets API calls. 
  int rv = WSAStartup(wVersionRequested, &socket_data);
  if(rv != 0) {
    switch(rv) {
      // NOTE: Application calls to WSAGetLastError to determine the WinSock
      // error code cannot be here used since the Windows Sockets DLL may
      // not have established the client data area where the "last error"
      // information is stored. 
      case WSASYSNOTREADY:
	// Network subsystem is not ready for network communication
	socket_error = gxSOCKET_WINSOCKNETWORK_ERROR;
	return -1;
      case WSAVERNOTSUPPORTED:
	// The requested WinSock version is not supported
	socket_error = gxSOCKET_WINSOCKVERSION_ERROR;
	return -1;
      case WSAEINVAL:
	// The WinSock version specified is not supported by this DLL	
	socket_error = gxSOCKET_WINSOCKEINVAL_ERROR;
	return -1;
      default:
	socket_error = gxSOCKET_WINSOCKINIT_ERROR;
	return rv; // Can't map this error so return the WIN32 error code
    }
  }

  // Confirm that the WinSock DLL supports the requested version
  if(socket_data.wVersion != wVersionRequested) return -1;
  switch(socket_version) { 
    case gxSOCKET_WSAVER_ONEZERO:
      if(LOBYTE(socket_data.wVersion ) != 1 ||
	 HIBYTE(socket_data.wVersion ) != 0) {
	socket_error = gxSOCKET_WINSOCKVERSION_ERROR;
	return -1; 
      }
      break;
    case gxSOCKET_WSAVER_ONEONE:
      if(LOBYTE(socket_data.wVersion ) != 1 ||
	 HIBYTE(socket_data.wVersion ) != 1) {
	socket_error = gxSOCKET_WINSOCKVERSION_ERROR;
	return -1; 
      }
      break;
    case gxSOCKET_WSAVER_TWOZERO:
      if(LOBYTE(socket_data.wVersion ) != 2 ||
	 HIBYTE(socket_data.wVersion ) != 0) {
	socket_error = gxSOCKET_WINSOCKVERSION_ERROR;
	return -1; 
      }
      break;
    case gxSOCKET_WSAVER_TWOTWO:
      if(LOBYTE(socket_data.wVersion ) != 2 ||
	 HIBYTE(socket_data.wVersion ) != 2) {
	socket_error = gxSOCKET_WINSOCKVERSION_ERROR;
	return -1; 
      }
      break;
    default:
      socket_error = gxSOCKET_WINSOCKVERSION_ERROR;
      return -1; 
  }
#endif

  return 0;
}

int gxSocket::ReleaseSocketLibrary()
// Perform any platform specific operations required
// to release the socket library and free any resources
// associated with it. Returns -1 if any errors occur.
{
#if defined (__WIN32__)
  // All WIN32 applications or DLLs are required to perform a successful
  // WSAStartup() call before they can use Windows Sockets services. When
  // finished the application or DLL must call WSACleanup() to deregister
  // itself from a Windows Sockets implementation. Any pending blocking or
  // asynchronous calls issued by any thread in this process are canceled
  // without posting any notification messages, or signaling any event
  // objects. NOTE: There must be a call to WSACleanup() for every successful
  // call to WSAStartup() made by a task. Only the final WSACleanup() for that
  // task does the actual cleanup. The preceding calls decrement an internal
  // reference count in the Windows Sockets DLL. In multithreaded environments
  // WSACleanup() terminates Windows Sockets operations for all threads.

  // Will return zero if the operation was successful. Otherwise the value
  // SOCKET_ERROR is returned, and a specific error number may be retrieved
  // by calling int WSAGetLastError(void);
  if(WSACleanup() != 0) {
    socket_error = gxSOCKET_WINSOCKCLEANUP_ERROR;
    return -1;
  }
#endif
  return 0;
}

// BEGIN - Client side Datagram functions
// --------------------------------------------------------------
int gxSocket::RawReadFrom(void *buf, int bytes, int flags)
// Receive a block of data from a remote datagram socket. NOTE: 
// A raw read may return before all the bytes have been read.
// Returns -1 if an error occurs.
{
  // Read data from a remote server and record the Internet
  // address of the server in the remote sin. NOTE: The Internet
  // address of the server should always be compared to the Internet
  // address of the server set when the datagram client was
  // constructed. Testing the Internet address will ensure that 
  // the correct server is sending datagrams to this client.
  return RawReadFrom(gxsocket, &remote_sin, buf, bytes, flags);
}

int gxSocket::RecvFrom(void *buf, int bytes, int flags)
// Receive a block of data from a remote datagram socket 
// and do not return until all the bytes have been read. 
// Returns the total number of bytes received or -1 if 
// an error occurs.
{
  // Read data from a remote server and record the Internet
  // address of the server in the remote sin. NOTE: The Internet
  // address of the server should always be compared to the Internet
  // address of the server set when the datagram client was
  // constructed. Testing the Internet address will ensure that 
  // the correct server is sending datagrams to this client.
  return RecvFrom(gxsocket, &remote_sin, buf, bytes, flags);
}

int gxSocket::RecvFrom(void *buf, int bytes, int seconds, int useconds, 
		       int flags)
// Receive a block of data from a remote datagram socket 
// and do not return until all the bytes have been read 
// or the timeout value has been exceeded. Returns the total 
// number of bytes received or -1 if an error occurs.
{
  // Read data from a remote server and record the Internet
  // address of the server in the remote sin. NOTE: The Internet
  // address of the server should always be compared to the Internet
  // address of the server set when the datagram client was
  // constructed. Testing the Internet address will ensure that 
  // the correct server is sending datagrams to this client.
  return RecvFrom(gxsocket, &remote_sin, buf, bytes, seconds, useconds, flags);
}

int gxSocket::RawWriteTo(const void *buf, int bytes, int flags)
// Send a block of data to a datagram socket. NOTE: A raw write
// may return before all the bytes have been written. Returns -1
// if an error occurs.
{
  // Write data to a remote server using the Internet
  // address of the server set when the datagram client was
  // constructed. 
  return RawWriteTo(gxsocket, &sin, buf, bytes, flags);
}

int gxSocket::SendTo(const void *buf, int bytes, int flags)
// Send a block of data to a datagram socket and do not return
// until all the bytes have been written. Returns the total number
// of bytes sent or -1 if an error occurs.
{
  // Write data to a remote server using the Internet
  // address of the server set when the datagram client was
  // constructed. 
  return SendTo(gxsocket, &sin, buf, bytes, flags);
}
// --------------------------------------------------------------
// END - Client side Datagram functions

// BEGIN - Server side Datagram functions
// --------------------------------------------------------------
int gxSocket::RemoteRawReadFrom(void *buf, int bytes, int flags)
// Receive a block of data from a remote datagram socket. NOTE: 
// A raw read may return before all the bytes have been read.
// Returns -1 if an error occurs.
{
  // Read data from a remote client and record the Internet
  // address of the client in the remote sin. NOTE: After the 
  // first ReadFrom call the client's Internet address should
  // be recorded and used for comparisons during subsequent 
  // server side ReadFrom and WriteTo calls. Testing the Internet
  // address will ensure that the correct client is sending
  // datagrams to this server.
  return RawReadFrom(gxsocket, &remote_sin, buf, bytes, flags);
}

int gxSocket::RemoteRecvFrom(void *buf, int bytes, int flags)
// Receive a block of data from a remote datagram socket 
// and do not return until all the bytes have been read. 
// Returns the total number of bytes received or -1 if 
// an error occurs.
{
  // Read data from a remote client and record the Internet
  // address of the client in the remote sin. NOTE: After the 
  // first RecvFrom call the client's Internet address should
  // be recorded and used for comparisons during subsequent 
  // server side RecvFrom and SendTo calls. Testing the Internet
  // address will ensure that the correct client is sending
  // datagrams to this server.
  return RecvFrom(gxsocket, &remote_sin, buf, bytes, flags);
}

int gxSocket::RemoteRecvFrom(void *buf, int bytes, 
			     int seconds, int useconds, int flags)
// Receive a block of data from a remote datagram socket 
// and do not return until all the bytes have been read 
// or the timeout value has been exceeded. Returns the total 
// number of bytes received or -1 if an error occurs.
{
  // Read data from a remote client and record the Internet
  // address of the client in the remote sin. NOTE: After the 
  // first RecvFrom call the client's Internet address should
  // be recorded and used for comparisons during subsequent 
  // server side RecvFrom and SendTo calls. Testing the Internet
  // address will ensure that the correct client is sending
  // datagrams to this server.
  return RecvFrom(gxsocket, &remote_sin, buf, bytes, seconds, useconds, flags);
}

int gxSocket::RemoteRawWriteTo(const void *buf, int bytes, int flags)
// Send a block of data to a datagram socket. NOTE: A raw write
// may return before all the bytes have been written. Returns -1
// if an error occurs.
{
  // Write data to a remote client using the Internet
  // address of the client set during a previous ReadFrom 
  // call.
  return RawWriteTo(gxsocket, &remote_sin, buf, bytes, flags);
}

int gxSocket::RemoteSendTo(const void *buf, int bytes, int flags)
// Send a block of data to a datagram socket and do not return
// until all the bytes have been written. Returns the total number
// of bytes sent or -1 if an error occurs.
{
  // Write data to a remote client using the Internet
  // address of the client set during a previous RecvFrom
  // call.
  return SendTo(gxsocket, &remote_sin, buf, bytes, flags);
}
// --------------------------------------------------------------
// END - Server side Datagram functions

void gxSocket::SetRemotePortNumber(gxsPort_t p)
{
  remote_sin.sin_port = htons(p); 
}

void gxSocket::SetRemoteIPAddress(char *IPAddress)
{
  // 10/31/2026: An unknown host (null hostnm) or null IPAddress crashed,
  // and the address was read as 8 bytes. The address is left as it was
  // and socket_error is set if the lookup fails.
  if(!IPAddress) {
    socket_error = gxSOCKET_HOSTNAME_ERROR;
    return;
  }
  gxsHostNameInfo *hostnm = gethostbyname(IPAddress);
  if(gxsCopyHostAddress(remote_sin.sin_addr, hostnm) != 0) {
    socket_error = gxSOCKET_HOSTNAME_ERROR;
  }
}

#ifdef __BCC32__
#pragma warn .8057
#pragma warn .8071
#pragma warn .8084
#endif

// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //

